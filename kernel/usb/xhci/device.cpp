#include "device.hpp"


namespace
{
    using namespace usb::xhci;

    //  usbで定義されたセットアップデータ構造をもとに、トランスファーリング上で機能する
    //  SetupStageTRB構造体のインスタンスを作成するプロシージャ。
    //  この関数によって、usbデバイスドライバとxHCI間での抽象化が実現される。
    //
    //  transfer_typeは以下を示す。
    //  ０：No Data Stage
    //  ２：OUT Data Stage
    //  ３：IN Data Stage
    SetupStageTRB MakeSetupStageTRB(usb::SetupData setup_data, int transfer_type)
    {
        SetupStageTRB setup{};
        setup.bits.request_type = setup_data.request_type.data;
        setup.bits.request = setup_data.request;
        setup.bits.value = setup_data.value;
        setup.bits.index = setup_data.index;
        setup.bits.length = setup_data.length;
        setup.bits.transfer_type = transfer_type;
        return setup;
    }

    //  bufポインタで始まるメモリ領域を指定するDataStageTRBを作成する。
    //  lenはTRB Transfer Lengthの値に指定する。
    //  dir_inはデータ転送の向きを指定する。trueの時、INを表す。
    DataStageTRB MakeDataStageTRB(void *buf, int len, bool dir_in)
    {
        DataStageTRB data{};
        data.bits.data_buffer_pointer = reinterpret_cast<uint64_t>(buf);
        data.bits.trb_transfer_length = len;
        data.bits.td_size = 0;
        data.bits.direction = dir_in;
        return data;
    }

    StatusStageTRB MakeStatusStageTRB()
    {
        StatusStageTRB status = {};
        return status;
    }

} 

namespace usb::xhci
{
    Device::Device(uint8_t slot_id, DoorbellRegister *doorbell) : 
        slot_id_(slot_id), 
        doorbell_(doorbell) {}
     
    int Device::Initialize()
    {
        dev_ctx_ = reinterpret_cast<DeviceContext *>(usb::AllocMem(sizeof(struct DeviceContext), 64, 0x1000));
        input_ctx_ = reinterpret_cast<InputContext *>(usb::AllocMem(sizeof(struct InputContext), 64, 0x1000));

        logger->debug("[Device::Initialize()] dev_ctx_(%p)\n", dev_ctx_);
        logger->debug("[Device::Initialize()] input_ctx_(%p)\n", input_ctx_);

        //  各コンテキスト領域を０で初期化する。
        memset(dev_ctx_, 0, sizeof(struct DeviceContext));
        memset(input_ctx_, 0, sizeof(struct InputContext));

        return 0;
    }

    DeviceContext* Device::DeviceContextPtr()
    {
        return dev_ctx_;
    }

    InputContext* Device::InputContextPtr() 
    { 
        return input_ctx_;
    }

    uint8_t Device::SlotID()
    {
        return slot_id_;
    }

    Ring *Device::AllocTransferRing(DeviceContextIndex dci, uint32_t ring_size)
    {
        transfer_rings_[dci.value] = new Ring;
        if (transfer_rings_[dci.value] == NULL) return NULL;

        transfer_rings_[dci.value]->Initialize(ring_size);
        return transfer_rings_[dci.value];
    }

    int Device::ControlIn(EndpointID ep_id, SetupData setup_data, void *buf, int len) 
    {
        if (ep_id.EndpointNumber() < 0 || ep_id.EndpointNumber() > 15) {
            logger->error("[ControlIn] Invailed Endpoint Number %d\n", ep_id.EndpointNumber());
            return -1;
        }
          
        uint8_t dci = ep_id.DeviceContextID();
        Ring *tr = transfer_rings_[dci];
        if (tr == NULL) {
            logger->error("[ControlIn] There Are No Ring At DCI4%02d\n", dci);
            return -1;
        }

        StatusStageTRB status = MakeStatusStageTRB();
        if (buf) {  //  DataStageありの場合。
            SetupStageTRB setup = MakeSetupStageTRB(setup_data, 3); // IN DATA STAGE
            TRB *setup_trb_position = tr->Push(&setup);
            DataStageTRB data = MakeDataStageTRB(buf, len, true);
            data.bits.interrupt_on_completion = 1; // 割り込みを発生させるよう要請
            TRB *data_trb_position = tr->Push(&data);
            status.bits.direction = false; // ACKの向きなのでOUT方向
            tr->Push(&status);
            if (setup_stage_map.Put(data_trb_position, (SetupStageTRB *) setup_trb_position)) {
                logger->error("[ControlIn] SetupStageMap is full!!\n");
                return -1;
            }
        } else {
            SetupStageTRB setup = MakeSetupStageTRB(setup_data, 0); // NO DATA STAGE
            TRB *setup_trb_position = tr->Push(&setup);
            status.bits.direction = true; // ACKの向きなのでIN方向
            status.bits.interrupt_on_completion = true; // 割り込みを発生させるよう要請
            TRB *status_trb_position = tr->Push(&status);
            if (setup_stage_map.Put(status_trb_position, (SetupStageTRB *) setup_trb_position)) {
                logger->error("[ControlIn] SetupStageMap is full!!\n");
                return -1;
            }
        }

        RingDoorbell(DeviceContextIndex{dci});
        return 0;
    }

    int Device::ControlOut(EndpointID ep_id, SetupData setup_data, void* buf, int len)
    {
         if (ep_id.EndpointNumber() < 0 || ep_id.EndpointNumber() > 15) {
            logger->error("[ControlOut] Invailed Endpoint Number %d\n", ep_id.EndpointNumber());
            return -1;
        }

        uint8_t dci = ep_id.DeviceContextID();
        Ring *tr = transfer_rings_[dci];
        if (tr == NULL) {
            logger->error("[ControlOut] There Are No Ring At DCI4%02d\n", dci);
            return -1;
        }

        StatusStageTRB status = MakeStatusStageTRB();
        if (buf) {  //  DataStageありの場合。
            SetupStageTRB setup = MakeSetupStageTRB(setup_data, 2); // OUT DATA STAGE
            TRB *setup_trb_position = tr->Push(&setup);
            DataStageTRB data = MakeDataStageTRB(buf, len, false); // OUT方向
            data.bits.interrupt_on_completion = 1; // 割り込み要求
            TRB *data_trb_position = tr->Push(&data);
            status.bits.direction = true; // IN方向
            tr->Push(&status);
            if (setup_stage_map.Put(data_trb_position, (SetupStageTRB *) setup_trb_position)) {
                logger->error("[ControlOut] SetupStageMap is full!!\n");
                return -1;
            }
        } else {
            SetupStageTRB setup = MakeSetupStageTRB(setup_data, 0); // NO DATA STAGE
            TRB *setup_trb_position = tr->Push(&setup);
            status.bits.direction = true; // ACKの向きなのでIN方向
            status.bits.interrupt_on_completion = true; // 割り込み要求
            TRB *status_trb_position = tr->Push(&status);
            if (setup_stage_map.Put(status_trb_position, (SetupStageTRB *) setup_trb_position)) {
                logger->error("[ControlOut] SetupStageMap is full!!\n");
                return -1;
            }
        }

        RingDoorbell(DeviceContextIndex{dci});
        return 0;
    }

    int Device::InterruptIn(EndpointID ep_id, void *buf, int len)
    {
        if (buf == nullptr) {
            logger->error("[InterruptIn] Data Buffer Is Null.\n");
            return -1;
        }
        NormalTRB normal_trb{};
        normal_trb.bits.data_buffer_pointer = reinterpret_cast<uint64_t>(buf);
        normal_trb.bits.trb_transfer_length = len;
        normal_trb.bits.interrupt_on_completion = true;
        normal_trb.bits.interrupt_on_short_packet = true; // ShortPacketでも良いとする
        normal_trb.bits.interrupter_target = 0; // Primary Event

        uint8_t dci = ep_id.DeviceContextID();
        transfer_rings_[dci]->Push(&normal_trb);
        doorbell_->Ring(dci, 0);
        return 0;
    }

    int Device::InterruptOut(EndpointID ep_id, void *buf, int len)
    {
        /* 
        未実装、、、
         */
        return -1;
    }

    int Device::OnTransferEventReceived(TransferEventTRB *trb)
    {
        void *buf = NULL; // TRBで使ったバッファへのポインタ
        int length = 0; // バッファの長さ
        int residual_length = trb->bits.trb_transfer_length; // バッファの余った部分の領域のサイズ
        EndpointID ep_id{DeviceContextIndex{static_cast<uint8_t>(trb->bits.endpoint_id)}}; // 使われたエンドポイント
        // printk("[Device::OnTransferEventReceived]\n");

        if (trb->bits.completion_code != 1 &&
            trb->bits.completion_code != 13) {
                //  SuccessでもShortPacketsでもない場合
                logger->error("[Device::OnTransferEventReceived] Invalid Completion Code %d.\n", trb->bits.completion_code);
                logger->error("%s\n", kTRBCompletionCodeToName[trb->bits.completion_code]);
                return -1;
        }

        TRB *issuer_trb = trb->Pointer(); // 対象となるTRBへのポインタ
        if (issuer_trb->bits.trb_type == 1) { // NormalTRBの時
            // printk("NORMAL TRB!!\n");
            NormalTRB *normal = reinterpret_cast<NormalTRB *>(issuer_trb);
            buf = reinterpret_cast<void *>(normal->bits.data_buffer_pointer);
            length = normal->bits.trb_transfer_length - residual_length;
            return OnInterruptCompleted(ep_id, buf, length);
        }

        ///////////////////////////////////////////////////////////////////////////
        // 以降は、setup->(data)->statusという組のコントロール転送に関するコード //
        ///////////////////////////////////////////////////////////////////////////

        // セットアップTRBを取得する。
        SetupStageTRB *setup_stage_trb = setup_stage_map.Get(issuer_trb);
        if (setup_stage_trb == NULL) {
            logger->error("[Device::OnTransferEventReceived] There Is No Setup Stage TRB.\n");
            return -1;
        }
        setup_stage_map.Delete(issuer_trb);

        //  USBデバイスへ渡す抽象データをTRBから作成する。
        SetupData setup_data = {};
        setup_data.request_type.data = setup_stage_trb->bits.request_type;
        setup_data.request = setup_stage_trb->bits.request;
        setup_data.value = setup_stage_trb->bits.value;
        setup_data.index = setup_stage_trb->bits.index;
        setup_data.length = setup_stage_trb->bits.length;
        
        if (issuer_trb->bits.trb_type == 3) {  //  DataStageTRB
            DataStageTRB *data_stage_trb = reinterpret_cast<DataStageTRB *>(issuer_trb);
            buf = reinterpret_cast<void *>(data_stage_trb->bits.data_buffer_pointer);
            length = data_stage_trb->bits.trb_transfer_length - residual_length;    //  実際のバッファの大きさ。
            return OnControlCompleted(ep_id, setup_data, buf, length);
        } else if (issuer_trb->bits.trb_type == 4) { //  StatusStageTRB
            return OnControlCompleted(ep_id, setup_data, buf, length);
        }

        logger->error("This Transfer Ring TRB Type Is Invalid.\n");
        return -1; 
    }

}
