#pragma once

#include <stdint.h>
#include "registers.hpp"
#include "context.hpp"
#include "../memory.hpp"
#include "../endpoint.hpp"
#include "../device.hpp"
#include "../setupdata.hpp"
#include "../../logging.hpp"
extern logging::Logger *logger;
int printk(const char *format, ...);

namespace usb::xhci
{
    const int kSetupStageMapSize = 32;
    class SetupStageMap
    {
    public:
        //  引数で指定したtrbに関連するコントロール転送のSetupStageTRBへの
        //  ポインタを辞書内から探す関数。このtrbは、DataStageTRBまたはStatusStageTRBへの
        //  ポインタになる。
        //  見つからなければNULLを返す。
        SetupStageTRB *Get(TRB *issuer_trb)
        {
            for (int i = 0; i < kSetupStageMapSize; i++) {
                /* if (dict_[i][0]) {
                    printk("dict_[%d][0]=0x%016lx issuer_trb=0x%016lx\n", i, dict_[i][0], issuer_trb);
                } */
                if ((uint64_t) dict_[i][0] == (uint64_t) issuer_trb) {
                    return (SetupStageTRB *) dict_[i][1];
                }
            }
            return NULL;
        }

        //  イベントを発行するTRBとセットアップTRBの組を辞書に追加する。
        //  成功すれば０を、失敗すれば−１を返す。
        int Put(TRB *issuer_trb, SetupStageTRB *setup_trb)
        {
            for (int i = 0; i < kSetupStageMapSize; i++) {
                if (dict_[i][0] == NULL) {
                    dict_[i][0] = (void *) issuer_trb;
                    dict_[i][1] = (void *) setup_trb;
                    return 0;
                }
            }
            return -1;
        }

        //  指定したTRBとセットアップTRBの組のデータを廃棄する
        void Delete(TRB *issure_trb) {
            for (int i = 0; i < kSetupStageMapSize; i++) {
                if (dict_[i][0] == (void *) issure_trb) {
                    dict_[i][0] = NULL;
                    dict_[i][1] = NULL;
                    break;
                }
            }
        }

    private:
        //  イベントを発行するTRBとそれに関連するSetupTRBの組を保持する。
        //  組の数は、kSetupStageMapSizeで指定される。
        void *dict_[kSetupStageMapSize][2] = {};
    };


    class Device : public usb::Device
    {
    public:
        Device(uint8_t slot_id, DoorbellRegister *doorbell);

        int Initialize(); // ２種のコンテキストのメモリ領域を確保し０で初期化する

        DeviceContext* DeviceContextPtr();
        InputContext* InputContextPtr();
        uint8_t SlotID();

        //  dci（Device Context Index）で指定したエンドポイントに、
        //  Transfer Ringを追加する。Ringの大きさは、ring_sizeで指定される。
        //  作成されたRingオブジェクトはtransfer_rings_[dci]に格納される。
        //  成功すればそのリングへのポインタを返す。
        //  失敗すればNULLを返す。
        Ring *AllocTransferRing(DeviceContextIndex dci, uint32_t ring_size);

        //  成功したら０を、失敗したら−１を返す。
        //  bufには、扱うデータ領域の先頭ポインタを格納する。
        //  lenは、そのbufの大きさを示す。
        //  buf==NULLの時、DataStageは省略される。
        //  bufとlenは、コントロール転送で用いるバッファとその大きさを指定する。
        //  setup_dataは、SetupDataTRBの設定に用いられる。
        //  bufがNULLの時は、setup->statusというTRBを送信する。
        //  bufがNULLでない時は、setup->data->statusというTRBを送信する。
        int ControlIn(EndpointID ep_id, SetupData setup_data, void *buf, int len) override;

        int ControlOut(EndpointID ep_id, SetupData setup_data, void* buf, int len) override;
        
        
        //  TransferEventを受け取った時に実行されるメンバ関数。
        //  引数には、そのイベントTRBへのポインタが渡される。
        //  内部の動作としては、まず、受け取ったTRBにかかれているissuerTRBを特定する。
        //  次に、そのTRBが含まれるコントロール転送のSetupStageTRBを取得する。
        //  （issuerTRBとSetupStageTRBの組は、このクラス内で管理している。）
        //  得られたSetupDataとやり取り用のデータバッファを、OnControlCompleted関数の引数に渡し、
        //  実行する。この関数は、xHCIに関係しないUSB規格上で動くものなので、
        //  このクラスの親クラスで定義されている。
        int OnTransferEventReceived(TransferEventTRB *trb);

    private:
        struct DeviceContext *dev_ctx_;    //  デバイスコンテキスト
        struct InputContext *input_ctx_;   //  インプットコンテキスト

        uint8_t slot_id_;           //  スロットの番号
        DoorbellRegister *doorbell_;  //  ドアベルレジスタ（スロットに対応）へのポインタ

        //  各エンドポイントが制御するTransfer Ringへのポインタ。
        //  リングが割り当てられていない場合は、NULLを返す。
        //  transfer_rings_[dci]：デバイスコンテキスト番号がdciのトランスファーリングへの
        //  ポインタを格納している。
        Ring *transfer_rings_[32] = {};

        SetupStageMap setup_stage_map;

        // dciで指定したエンドポイントのドアベルを鳴らす
        void RingDoorbell(DeviceContextIndex dci) {
            doorbell_->Ring(dci.value, 0);
        }

    };
}

