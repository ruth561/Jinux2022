#include "device.hpp"

/* 
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
        SetupStageTRB setup = {};
        setup.request_type = setup_data.request_type.data;
        setup.request = setup_data.request;
        setup.value = setup_data.value;
        setup.index = setup_data.index;
        setup.length = setup_data.length;
        setup.transfer_type = transfer_type;
        setup.trb_type = 2;
        setup.immediate_data = 1;
        setup.trb_transfer_length = 8;
        return setup;
    }

    //  bufポインタで始まるメモリ領域を指定するDataStageTRBを作成する。
    //  lenはTRB Transfer Lengthの値に指定する。
    //  dir_inはデータ転送の向きを指定する。trueの時、INを表す。
    DataStageTRB MakeDataStageTRB(void *buf, int len, bool dir_in)
    {
        DataStageTRB data = {};
        data.data_buffer_pointer = (uint64_t) buf;
        data.trb_transfer_length = len;
        data.td_size = 0;
        data.direction = dir_in;
        data.trb_type = 3;
        return data;
    }

    StatusStageTRB MakeStatusStageTRB()
    {
        StatusStageTRB status = {};
        status.trb_type = 4;
        return status;
    }


    
} 
 */
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

/*     int Device::ControlIn(EndpointID ep_id, SetupData setup_data, 
                    void *buf, int len, ClassDriver *issuer) {
        
        // printk("[Device::ControlIn] ep_dci %d, buf 0x%lx, len %d\n", 
        //  ep_id.DeviceContextID(), buf, len);
        if (auto err = usb::Device::ControlIn(ep_id, setup_data, buf, len, issuer)) {
            return err;
        }

        if (ep_id.EndpointNumber() < 0 || ep_id.EndpointNumber() > 15) {
            printk("        Invailed ep_num %d\n", ep_id.EndpointNumber());
            return -1;
        }
        
        int dci = ep_id.DeviceContextID();
        Ring *tr = transfer_rings_[dci];
        if (tr == NULL) {
            printk("        There are no ring at dci%02d\n", dci);
            return -1;
        }


        StatusStageTRB status = MakeStatusStageTRB();

        if (buf) {  //  DataStageありの場合。
            SetupStageTRB setup = MakeSetupStageTRB(setup_data, 3);
            TRB *setup_trb_position = tr->Push(&setup);
            DataStageTRB data = MakeDataStageTRB(buf, len, true);
            data.interrupt_on_completion = 1;
            TRB *data_trb_position = tr->Push(&data);
            tr->Push(&status);
            if (setup_stage_map.Put(data_trb_position, (SetupStageTRB *) setup_trb_position)) {
                printk("[ERROR] SetupStageMap is full!!\n");
                return -1;
            }
        } else {
            SetupStageTRB setup = MakeSetupStageTRB(setup_data, 0);
            TRB *setup_trb_position = tr->Push(&setup);
            status.direction = true;
            status.interrupt_on_completion = true;
            TRB *status_trb_position = tr->Push(&status);
            if (setup_stage_map.Put(status_trb_position, (SetupStageTRB *) setup_trb_position)) {
                printk("[ERROR] SetupStageMap is full!!\n");
                return -1;
            }
        }

        RingDoorbell(dci);

        return 0;
    }


    int Device::ControlOut(EndpointID ep_id, SetupData setup_data,
                    void* buf, int len, ClassDriver *issuer)
    {
        // printk("[Device::ControlOut] ep_dci %d, buf 0x%lx, len %d\n", 
        //  ep_id.DeviceContextID(), buf, len);
        if (auto err = usb::Device::ControlOut(ep_id, setup_data, buf, len, issuer)) {
            return err;
        }
        
        if (ep_id.EndpointNumber() < 0 || ep_id.EndpointNumber() > 15) {
            printk("        Invailed ep_num %d\n", ep_id.EndpointNumber());
            return -1;
        }

        int dci = ep_id.DeviceContextID();
        Ring *tr = transfer_rings_[dci];
        if (tr == NULL) {
            printk("        There are no ring at dci%02d\n", dci);
            return -1;
        }


        StatusStageTRB status = MakeStatusStageTRB();

        if (buf) {  //  DataStageありの場合。
            SetupStageTRB setup = MakeSetupStageTRB(setup_data, 2);
            TRB *setup_trb_position = tr->Push(&setup);
            DataStageTRB data = MakeDataStageTRB(buf, len, true);
            data.interrupt_on_completion = 1;
            TRB *data_trb_position = tr->Push(&data);
            tr->Push(&status);
            if (setup_stage_map.Put(data_trb_position, (SetupStageTRB *) setup_trb_position)) {
                printk("[ERROR] SetupStageMap is full!!\n");
                return -1;
            }
        } else {
            SetupStageTRB setup = MakeSetupStageTRB(setup_data, 0);
            TRB *setup_trb_position = tr->Push(&setup);
            status.interrupt_on_completion = true;
            TRB *status_trb_position = tr->Push(&status);
            if (setup_stage_map.Put(status_trb_position, (SetupStageTRB *) setup_trb_position)) {
                printk("[ERROR] SetupStageMap is full!!\n");
                return -1;
            }
        }

        RingDoorbell(dci);

        return 0;
    }
 */
/*     int Device::OnTransferEventReceived(TransferEventTRB *trb)
    {
        int residual_length = trb->trb_transfer_length;

        if (trb->completion_code != 1 &&
            trb->completion_code != 13) {
                //  SuccessでもShortPacketsでもない場合。
                printk("[ERROR] TransferEventTRB completion code");
                return -1;
        }
        //printk("Transfer (value %08lx) completed: %s, residual length %d, slot %d, ep dci %d\n",
        //  trb->trb_pointer,
        //  kTRBCompletionCodeToName[trb->completion_code],
        //  trb->trb_transfer_length,
        //  trb->slot_id,
        //  trb->endpoint_id);

        // このイベントを発行したTransfer TRBへのポインタ。
        TRB *issuer_trb = (TRB *) trb->trb_pointer;
        // セットアップTRBを取得する。
        SetupStageTRB *setup_stage_trb = setup_stage_map.Get(issuer_trb);
        if (setup_stage_trb == NULL) {
            printk("[ERROR] There is no setup stage trb\n");
            return -1;
        }
        setup_stage_map.Delete(issuer_trb);
        //  printk("SetupStageTRB (value %08lx)\n", setup_stage_trb);

        //  USBデバイスへ渡す抽象データをTRBから作成する。
        SetupData setup_data = {};
        setup_data.request_type.data = setup_stage_trb->request_type;
        setup_data.request = setup_stage_trb->request;
        setup_data.value = setup_stage_trb->value;
        setup_data.index = setup_stage_trb->index;
        setup_data.length = setup_stage_trb->length;


        void *data_stage_buf = NULL;
        int transfer_length = 0;
        if (issuer_trb->trb_type == 3) {    //  DataStageTRB
            DataStageTRB *data_stage_trb = (DataStageTRB *) issuer_trb;
            data_stage_buf = (void *) data_stage_trb->data_buffer_pointer;
            transfer_length = data_stage_trb->trb_transfer_length - residual_length;    //  実際のバッファの大きさ。
        } else if (issuer_trb->trb_type == 4) { //  StatusStageTRB
            //  pass
        } else {
            return -1;
        }

        EndpointID ep_id(trb->endpoint_id);

        return OnControlCompleted(ep_id, setup_data, data_stage_buf, transfer_length);
    }
 */
}
