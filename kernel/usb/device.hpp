#pragma once

#include <new>

#include "endpoint.hpp"
#include "setupdata.hpp"
#include "descriptor.hpp"

#include "classdriver/base.hpp"
#include "classdriver/hid.hpp"

// #include "./classdriver/base.hpp"

int printk(const char *format, ...);

namespace
{
    using namespace usb;

/*     const int kEventWaitersMapSize = 32;

    bool CompareSetupData(SetupData *setup1, SetupData *setup2)
    {
        if (setup1->index != setup2->index) return false;    printk("index\n");
        if (setup1->length != setup2->length) return false;  printk("length\n");
        if (setup1->request != setup2->request) return false;  printk("request\n");
        if (setup1->request_type.data != setup2->request_type.data) return false;  printk("request_data\n");
        if (setup1->value != setup2->value) return false;  printk("value\n");

        return true;
    } */

    //  イベントを待つクラスドライバと、そのイベントのセットアップデータの組を保持する辞書。
/*     class EventWaitersMap
    {
    public:
        //  setup_dataに関連するクラスドライバを返す
        ClassDriver *Get(SetupData *setup_data)
        {
            for (int i = 0; i < kEventWaitersMapSize; i++) {
                // if (dict_[i][0]) {
                //    printk("dict_[%d][0]=0x%016lx issuer_trb=0x%016lx\n", i, dict_[i][0], issuer_trb);
                //}
                if (dict_[i][0]) {
                    printk("dict_[%d][0]=0x%016lx issuer_trb=0x%016lx\n", i, dict_[i][0], setup_data);
                    if (CompareSetupData((SetupData *) dict_[i][0], setup_data)) {
                        printk("correspond!!\n");
                        return (ClassDriver *) dict_[i][1];
                    }
                }
            }
            return NULL;
        }


        //  成功すれば０を、失敗すれば−１を返す。
        int Put(SetupData *setup_data, ClassDriver *class_driver)
        {
            for (int i = 0; i < kEventWaitersMapSize; i++) {
                if (dict_[i][0] == NULL) {
                    printk("put setup %lx class %lx\n", setup_data, class_driver);
                    dict_[i][0] = (void *) setup_data;
                    dict_[i][1] = (void *) class_driver;
                    return 0;
                }
            }
            return -1;
        }

        //  指定したTRBとセットアップTRBの組のデータを廃棄する
        void Delete(SetupData *setup_data) {
            for (int i = 0; i < kEventWaitersMapSize; i++) {
                if (CompareSetupData((SetupData *) dict_[i][0], setup_data)) {
                    dict_[i][0] = NULL;
                    dict_[i][1] = NULL;
                    break;
                }
            }
        }

    private:
        //  イベントを発行するTRBとそれに関連するSetupTRBの組を保持する。
        //  組の数は、kSetupStageMapSizeで指定される。
        void *dict_[kEventWaitersMapSize][2] = {};
    };
 */
}

namespace usb
{
    class Device 
    {
    public:
        virtual int ControlIn(EndpointID ep_id, SetupData setup_data, void *buf, int len) { return -1; };

        virtual int ControlOut(EndpointID ep_id, SetupData setup_data, void* buf, int len) { return -1; };
 
        // デバイスディスクリプタの取得をリクエスト
        int StartInitialize();

        //  デバイスディクリプタの情報をもとに設定を行い、コンフィギュレーションディスクリプタの取得を試みる。
        int InitializePhase1(DeviceDescriptor *dev_desc);

        //  lenは、受け取ったデータのサイズを表している。
        int InitializePhase2(ConfigurationDescriptor *config_desc, int len);

        int InitializePhase3(uint8_t config_value);

        //  一連のコントロール転送が完了し、イベントが発行された後に呼び出される。
        //  USB規格上で扱えるように、SetupDataとデータバッファなどが解析された後に呼び出される。
        //  xHCIでは、OnTransferEventReceived()から呼び出される。
        //  ep_id：エンドポイントの番号を特定するもの
        //  setup_data：一連のコントロール転送のSetupData
        //  buf：コントロール転送で使用したバッファの先頭ポインタ
        //  len：実際に送受信したデータサイズ
        int OnControlCompleted(EndpointID ep_id, SetupData setup_data, void *buf, int len);
        
        //  各エンドポイントに設定が終わった時に呼ばれる関数。
        //  クラスドライバのOnEndpointsConfigured()を呼び出す。
        int OnEndpointsConfigured();

        bool IsInitialized() { return is_initialized_; }

/*         //  このクラスの使用するエンドポイントのコンフィグ情報が詰まった配列を返す。
        EndpointConfig *EndpointConfigs() { return ep_configs_; }
        int NumEndpointConfig() { return num_ep_configs_; } */

    private:
        //  コントロール転送でデータをやり取りするのに用いられるメモリ領域。
        uint8_t buf_[256] = {};

        // class_driver_[interface_number] := インターフェースに対応したクラスドライバへのポインタ
        ClassDriver *class_drivers_[256] = {};

        //  i番目のエンドポイントの設定情報を格納する配列。
        //  ここで管理されるエンドポイントは、このOSが対応しているインターフェースの
        //  使用するエンドポイントのみである。
        EndpointConfig ep_configs_[16];
        uint8_t num_ep_configs_;

        //  このデバイスが持つコンフィギュレーションの数
        uint8_t num_configurations_;
        //  扱うコンフィグレーションの番号を指定する。
        //  ０≦config_index_≦num_configurations_−１
        //  Initialize１で得られるデバイスディスクリプタの情報をもとに設定される。
        uint8_t config_index_;

        //  初期化処理が完了しているか。
        bool is_initialized_ = false;
        //  初期化処理は５段階に分けて行われる。イベントリングからの通知の時、
        //  今どの段階にいるのかを示してくれる。
        int initialize_phase_ = 0;

        // EventWaitersMap event_waiters_;

        //  スタンダードデバイスリクエストの中のGET_DESCRIPTORを発行するプロシージャ。
        //  bufには、ディスクリプタのデータが格納される。lenはbufのサイズである。
        //  desc_typeは、usb::descriptor_type名前空間上のデータを指定する。
        //  （デバイスディクスリプタやコンフィギュレーションディスクリプタなどを指定できる。）
        //  desc_indexは基本０で良い。（ConfigurationとStringのときだけ、意味を持つ。
        //  一つのディスクリプタタイプに複数のディスクリプタが存在する時、それらを指定するために使われる。）
        int GetDescriptor(EndpointID ep_id, uint8_t desc_type, uint8_t desc_index, void *buf, int len);

        int SetConfiguration(EndpointID ep_id, uint8_t config_value);


    };
}
