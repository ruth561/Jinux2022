#include "xhci.hpp"

int printk(const char *format, ...);
void Halt();
extern logging::Logger *logger;
extern TaskManager* task_manager;


namespace pci 
{
    extern std::vector<Device> devices;

    // PCIデバイスの中からxHCを一つ見つけ出し、devices要素へのポインタで返す
    pci::Device *FindXHC() {
        Device *res = nullptr;
        for (int i = 0; i < pci::devices.size(); i++) {
            if (pci::devices[i].base_class == 0x0c &&
                pci::devices[i].sub_class == 0x03 &&
                pci::devices[i].interface == 0x30) {
                logger->debug("USB3.0 HOST CONTROLLER!!\n");
                res = &pci::devices[i];
                if (pci::ConfigureMSI(&pci::devices[i])) {
                    logger->error("Failed to configure MSI.\n");
                }
                break;
            }
        }
        return res;
    }
}


namespace
{
    using namespace usb::xhci;

    //  ポートの設定時における状態の列挙体。
    //  仕様書によると、スロットのリセット処理からアドレスをデバイスに割り当てるまで、
    //  他のポートの設定をしてはならない、とあるので注意する。
    enum class ConfigPhase
    {
        //  初期状態か接続されていない状態を示す。
        kNotConnected,
        //  他のポートの設定が終わるのを待っている状態        //  AddressDeviceCommandのコマンド完了イベントが起こった時に、
        //  この状態のポートのリセット処理が始まる。
        kWaitingAddressed,
        //  ポートをリセットしている状態
        //  ポートのリセットを開始してからPortStatusChangeEventを受け取るまで
        kResettingPort,
        //  xHCにスロットを使用可能にしてもっている状態。
        //  EnableSlotCommandを発行してからCompletionCommandEventを受け取るまで
        kEnablingSlot,
        //  指定したスロットにアドレスを割り当てている状態
        //  AddressDeviceCommandを発行してからCompletionCommandEventを受け取るまで
        kAddressingDevice,
        kInitializingDevice,
        kConfiguringEndpoints,
        kConfigured,
    };

    //  各ポートの設定における状態を保持する配列。
    //  ポートは最大でも２５６個なので、この大きさで確保してある。
    ConfigPhase port_config_phase[256] = {};

    // ０なら初期化中でない
    // ０でないならUSBデバイスを初期化中のタスクIDである
    uint64_t initializing_port[256] = {};

    //  現在リセットからアドレスの割当までの処理を行っているポートの番号。
    //  どのポートもその処理上にない場合、この値は０を示す。
    uint8_t addressing_port = 0;
    
    //  ポートスピードからMaxPacketSizeの値を定める関数
    //  よく理解していないので、あとでUSBの仕様書を読んで理解したい。
    uint32_t DetermineMaxPacketSizeForControlPipe(uint32_t slot_speed) {
        switch (slot_speed) {
            case 4: // Super Speed
                return 512;
            case 3: // High Speed
                return 64;
            default:
                return 8;
        }
    }

} // namespace





namespace usb::xhci
{   
    Controller::Controller(uintptr_t mmio_base) :
        mmio_base_(mmio_base),
        cap_((CapabilityRegisters *) mmio_base_),
        opt_((OperationalRegisters *) (mmio_base_ + cap_->CAPLENGTH)),
        run_((RuntimeRegisters *) (mmio_base_ + cap_->RTSOFF.Offset())),
        doorbell_((DoorbellRegister *) (mmio_base_ + cap_->DBOFF.Offset())) {
        logger->debug("Capability Registers: %p\n", cap_);
        logger->debug("Operational Registers: %p\n", opt_);
        logger->debug("Runtime Registers: %p\n", run_);
        logger->debug("Doorbell Registers: %p\n", doorbell_);


        logger->debug("HCSPARAMS1: %08xH\n", cap_->HCSPARAMS1.data[0]);
        logger->debug("HCSPARAMS2: %08xH\n", cap_->HCSPARAMS2.data[0]);
        logger->debug("HCSPARAMS3: %08xH\n", cap_->HCSPARAMS3.data[0]);

        for (int i = 1; i < 256; i++) {
            ports_[i] = new Port(i, reinterpret_cast<PortRegisterSet *>(
                reinterpret_cast<uint64_t>(opt_) + 0x400 + 0x10 * (i - 1)));
        }
    }

    int Controller::Initializer()
    {
        logger->info("[+] Start Initializing XHC.\n");
        // xHCの初期化（リセット）を行う
        if (!opt_->USBSTS.bits.host_controller_halted) {
            logger->warning("xHC is not halted.\n");
            opt_->USBCMD.bits.run_stop = 0; // リセットを開始する
        }

        // リセットが完了するまで待つ
        while (!opt_->USBSTS.bits.host_controller_halted);

        // リセットの開始
        opt_->USBCMD.bits.host_controller_reset = 1;
        while (opt_->USBCMD.bits.host_controller_reset); // リセットが完了するまで待つ
        while (opt_->USBSTS.bits.controller_not_ready); // レジスタへの書き込みが可能になるまで待つ

        logger->info("[xHC Init] MaxPorts        %d\n", cap_->HCSPARAMS1.bits.max_ports);
        logger->info("[xHC Init] MaxDeviceSlots  %d\n", cap_->HCSPARAMS1.bits.max_device_slots);
        logger->info("[xHC Init] MaxInterrupters %d\n", cap_->HCSPARAMS1.bits.max_interrupters);

        // デバイススロットの有効化数は最大数までに設定する
        device_size_ = cap_->HCSPARAMS1.bits.max_device_slots;
        opt_->CONFIG.bits.max_device_slots_enabled = device_size_;
        slot_to_port_.resize(device_size_ + 1);

        // デバイスマネージャーの初期化
        logger->info("[xHC Init] MaxDeviceSlot   %d\n", device_size_);
        devmgr_.Initialize(device_size_);
        opt_->DCBAAP.SetPointer(devmgr_.DeviceContexts());
        logger->info("[xHC Init] DCBAA           %p\n", opt_->DCBAAP.Pointer());

        // コマンドリングの初期化 -> CRCRへ設定
        cr_.Initialize(32);
        CRCR_Bitmap crcr{};
        crcr.data[0] = 0;
        crcr.bits.ring_cycle_state = true;
        crcr.SetPointer(cr_.Buffer());
        opt_->CRCR.data[0] = crcr.data[0]; 
        logger->debug("Set CRCR %lxH\n", opt_->CRCR.data[0]);

        // イベントリングの初期化 
        InterrupterRegisterSet *primary_interrupt = &run_->IR[0];
        er_.Initialize(32, primary_interrupt);

        // 割り込みの設定
        primary_interrupt->IMOD.bits.interrupt_moderation_interval = 4000;
        // primary_interrupt->IMOD.bits.interrupt_moderation_counter = 10; // この設定はなくていい？
        primary_interrupt->IMAN.bits.interrupt_pending = true;
        primary_interrupt->IMAN.bits.interrupt_enable = true;
        opt_->USBCMD.bits.interrupter_enable = true;
        logger->debug("Setup MSI Interrupt.\n");
        logger->info("[+] Finish Initializing XHC.\n");

        return 0;
    }
    
    void Controller::Run()
    {
        opt_->USBCMD.bits.run_stop = true;
        while (opt_->USBSTS.bits.host_controller_halted);

        logger->info("xHC power on!!\n");
    }

    void Controller::AllPortsInit()
    {
        //  ソフトウェア内部での各ポートの状態を初期化して置く。
        addressing_port = 0;
        for (int i = 0; i < 256; i++) {
            port_config_phase[i] = ConfigPhase::kNotConnected;
        }

        //  各ポートのレジスタの値を解析し、USBデバイスが接続されている
        //  ポートにリセット処理を介しするように促す。
        for (uint8_t port_num = MaxPorts(); port_num > 0; port_num--) {
            Port *port = PortAt(port_num);
            // printk("Port %02d: IsConnected=%d IsEnabled=%d IsConnectStatusChanged=%d\n", 
            //    port.Number(), 
            //    port.IsConnected(),
            //    port.IsEnabled(), 
            //    port.IsConnectStatusChanged());

            //  各ステータスフラグのクリアを行う。
            if (port->IsConnectStatusChanged()) 
                port->ClearConnectStatusChanged();
            if (port->IsPortResetChanged())  
                port->ClearPortResetChange();

            //  デバイスが接続されているポートをリセットする。
            if (port->IsConnected()) {
                task_manager
                    ->NewTask()
                    ->InitContext(InitUSBDeviceTask, static_cast<int64_t>(port_num))
                    ->Wakeup(3); // カーネルの特権レベルで実行
                // Halt(); // 一旦停止しておく
                // ResetPort(&port);
            }
        }
    }

    Ring *Controller::CommandRing()
    {
        return &cr_;
    }

    EventRing *Controller::PrimaryEventRing()
    {
        return &er_;
    }

    InterrupterRegisterSet *Controller::PrimaryInterruptRegs()
    {
        return &run_->IR[0];
    }

    uint8_t Controller::MaxPorts() const { 
        return static_cast<uint8_t>(cap_->HCSPARAMS1.bits.max_ports); 
    }

    Port *Controller::PortAt(uint8_t port_num) 
    {
        return ports_[port_num];
    }

    void Controller::SendNoOpCommand()
    {
        NoOpCommandTRB trb = NoOpCommandTRB();
        cr_.Push(&trb);
        doorbell_[0].Ring(0, 0);
    }   

    int Controller::ProcessEvent()
    {
        int ret;
        if (!er_.HasFront()) { // イベントリングが空
            return -1;
        }
        // logger->debug("PROCESS EVENT (%p)\n", er_.Front());
        TRB *event_trb = er_.Front();
        switch (event_trb->bits.trb_type) {
            case CommandCompletionEventTRB::Type:
                ret = OnEvent(reinterpret_cast<CommandCompletionEventTRB *>(event_trb));
                break;
            case PortStatusChangeEventTRB::Type:
                ret = OnEvent(reinterpret_cast<PortStatusChangeEventTRB *>(event_trb));
                break;
            case TransferEventTRB::Type:
                ret = OnEvent(reinterpret_cast<TransferEventTRB *>(event_trb));
                break;
            default:
                logger->warning("[ProcessEvent] Invalid Event TRB!!\n");
                ret = -1;
                break;  
        }
        er_.Pop();
        
        return ret;
    }

    int Controller::ConfigureEndpoints(Device *dev)
    {
        // USBデバイスの初期化が終わり、Configuration Descriptorの情報などがDeviceオブジェクトに保存されている状態を前提とする。
        EndpointConfig *configs = dev->EndpointConfigs();
        int len = dev->NumEndpointConfig();
        /* // logger->debug("[+] ConfigureEndpoints\n");
        for (int i = 0; i < len; i++) {
            // logger->debug("    | DeviceContextIndex: %d\n", configs[i].ep_id.DeviceContextID());
            // logger->debug("    | EndPointType: %s\n", usb::kEndpointTypeToName[configs[i].ep_type]);
            // logger->debug("    | MaxPacketSize: %d\n", configs[i].max_packet_size);
            // logger->debug("    | Interval: %d\n", configs[i].interval);
        } */

        memset(&dev->InputContextPtr()->input_control_context, 0, sizeof(InputControlContext)); //  InputControlContextのデータを０クリアする。
        //  SlotContextのデータをデバイスコンテキスト内のデータで上書きする。
        memcpy(&dev->InputContextPtr()->slot_context, &dev->DeviceContextPtr()->slot_context, sizeof(SlotContext));
        auto slot_ctx = dev->InputContextPtr()->EnableSlotContext();
        slot_ctx->bits.context_entries = 31;

        // ポートスピードの値からMaxPacketSizeを設定する
        const uint8_t port_id = dev->DeviceContextPtr()->slot_context.bits.root_hub_port_num;
        const int port_speed = PortAt(port_id)->Speed();
        if (port_speed == 0 || port_speed > PortSpeed::kSuperSpeed) {
            logger->error("[ConfigEndpoint] Port Speed Unsupported\n");
            return -1;
        }
        // logger->debug("    | PortSpeed: %d\n", port_speed);

        for (int i = 0; i < len; i++) { // 全てのエンドポイントに対して設定を行ってゆく
            auto ep_ctx = dev->InputContextPtr()->EnableEndpoint(DeviceContextIndex{configs[i].ep_id.DeviceContextID()});
            switch (configs[i].ep_type) { // EP type
                case EndpointType::kControl:
                    ep_ctx->bits.ep_type = 4;
                    break;
                case EndpointType::kIsochronous:
                    ep_ctx->bits.ep_type = configs[i].ep_id.IsIn() ? 5 : 1;
                    break;
                case EndpointType::kBulk:
                    ep_ctx->bits.ep_type = configs[i].ep_id.IsIn() ? 6 : 2;
                    break;
                case EndpointType::kInterrupt:
                    ep_ctx->bits.ep_type = configs[i].ep_id.IsIn() ? 7 : 3;
                    break;
            }

            ep_ctx->bits.max_packet_size = configs[i].max_packet_size;
            // Intervalの変換
            if (port_speed == kFullSpeed || port_speed == kLowSpeed) {
                if (configs[i].ep_type == EndpointType::kInterrupt) {
                    // 簡単のため、bInterval: configs[i].interval, interval: ep_ctx->bits.intervalと区別して扱う。
                    // USB仕様では、periods=bInterval*1(ms)で計算されるが、
                    // xHC仕様では、periods=2^{interval - 1}*125(μs)で計算されるのでこの変換が必要。
                    // 計算する値は、aを2^a≦bInterval＜2^{a+1}とすると、interval=a+3で設定できる。
                    int a = 0;
                    while ((1 << a) < configs[i].interval) a++;
                    ep_ctx->bits.interval = a + 3;
                    logger->error("    | This Port Speed %d Is Not Available.\n", port_speed);
                    return -1;
                } else {
                    logger->error("This Pair Of Type And Port Speed Is Not Implemented..\n");
                    return -1;
                }
            } else {
                ep_ctx->bits.interval = configs[i].interval - 1; // デバイスへのリクエスト間隔（125μs * interval）
            }
            ep_ctx->bits.average_trb_length = 1;

            auto tr = dev->AllocTransferRing(DeviceContextIndex{configs[i].ep_id.DeviceContextID()}, 32);
            ep_ctx->SetTransferRingBuffer(tr->Buffer());
            ep_ctx->bits.dequeue_cycle_state = 1;
            ep_ctx->bits.max_primary_streams = 0;
            ep_ctx->bits.mult = 0;
            ep_ctx->bits.error_count = 3;
        }

        ConfigureEndpointCommandTRB cmd{dev->InputContextPtr(), dev->SlotID()};
        CommandRing()->Push(&cmd);
        RingCommandRing();
        return 0;
    }


// Controller(private)
    void Controller::RingCommandRing() {
        doorbell_[0].Ring(0, 0);
    }

    int Controller::OnEvent(CommandCompletionEventTRB *trb)
    {
        uint8_t port_id;
        uint32_t slot_id = trb->bits.slot_id;
        TRB *issuer_trb = trb->Pointer(); // このイベントを発行したTRBへのポインタ
        uint32_t issuer_type = issuer_trb->bits.trb_type;
        /* logger->set_level(logging::kDEBUG);
        logger->debug("[OnEvent] CommandCompletionEvent\n");
        logger->debug("    | Issuer: %s (%p)\n", kTRBTypeToName[issuer_trb->bits.trb_type], issuer_trb);
        logger->debug("    | SlotID: %d\n", trb->bits.slot_id);
        logger->debug("    | CompletionCode: %s\n", kTRBCompletionCodeToName[trb->bits.completion_code]); */
        
        if (issuer_type == EnableSlotCommandTRB::Type) {
            port_id = addressing_port; // EnableSlotを実行しているのはひとつだけ
            PortAt(port_id)->SetSlot(slot_id);
            if (initializing_port[port_id]) {
                task_manager->Wakeup(initializing_port[port_id]);
                return 0;
            }
        } else if (issuer_type == AddressDeviceCommandTRB::Type) {
            // このコマンドが正常に完了した場合、インプットスロットの内容は、
            // 全てアウトプットスロットの内容にコピーされているはずである。
            // また、EP０の内容もコピーされているはずである。
            Device *dev = devmgr_.FindBySlot(slot_id);
            port_id = static_cast<uint8_t>(dev->DeviceContextPtr()->slot_context.bits.root_hub_port_num);
            if (initializing_port[port_id]) {
                task_manager->Wakeup(initializing_port[port_id]);
                return 0;
            }
        } else if (issuer_type == DisableSlotCommandTRB::Type) {
            if (trb->bits.completion_code == 1) {
                logger->info("[+] Slot%02d Disabled.\n", slot_id);
                slot_to_port_[slot_id] = 0;
                devmgr_.DisableSlot(slot_id); // デバイス用に確保したメモリ領域を確保などする
            }
        } else if (issuer_type == ConfigureEndpointCommandTRB::Type) {
            Device *dev = devmgr_.FindBySlot(slot_id);
            if (dev == nullptr) {
                logger->error("[OnEvent::ConfigureEndpointCommand]\n");
                return -1;
            }

            port_id = dev->DeviceContextPtr()->slot_context.bits.root_hub_port_num;
            if (initializing_port[port_id]) {
                task_manager->Wakeup(initializing_port[port_id]);
                return 0;
            }
            // logger->info("[+] Completed Configure Endpoint!\n");
            // logger->info("DeviceContextPtr(): %p\n", dev->DeviceContextPtr());
            return -1;
        }

        return 0;
    }

    int Controller::OnEvent(PortStatusChangeEventTRB *trb)
    {
        /* 
         * ポートの状態が変化した時に呼ばれる関数。変化後の状態は以下のいずれかになる。
         * ・Disconnected（ポートからデバイスが切り離された）
         * ・Polling（ポートへデバイスが接続された）
         * ・Enable（ポートがリセットによって有効化された）
         */
        int res = -1;
        uint8_t port_id = static_cast<uint8_t>(trb->bits.port_id);
        Port *port = PortAt(port_id);
        // logger->debug("[OnEvent] PortStatusChangeEvent\n");
        // logger->debug("    | PortNumber: %hhd\n", port.Number());
        // logger->debug("    | CompletionCode: %s\n", kTRBCompletionCodeToName[trb->bits.completion_code]);
        
        if (port->IsConnectStatusChanged()) { // ポート接続の変化
            if (port->IsConnected()) {
                logger->info("[+] Device Attached To Port%hhd.\n", port->Number());
                if (port_config_phase[port_id] == ConfigPhase::kNotConnected) // 接続検知
                    res = ResetPort(port);
            } else {
                logger->info("[+] Device Detached From Port%hhd\n", port->Number());
                port_config_phase[port->Number()] = ConfigPhase::kNotConnected;
                // ポートにスロットが割り当てられていれば、そのスロットを無効にする
                for (int i = 1; i <= device_size_; i++) {
                    if (slot_to_port_[i] == port->Number()) {
                        DisableSlot(i);
                        break;
                    }
                }
            }
            port->ClearConnectStatusChanged();
        }
        if (port->IsPortResetChanged()) { // リセット処理が終わった場合
            if (initializing_port[port_id]) {
                task_manager->Wakeup(initializing_port[port_id]);
                return 0;
            }
            return -1;
        }

        return res;
    }

    int Controller::OnEvent(TransferEventTRB *trb)
    {
        // TRB *issuer_trb = trb->Pointer(); // このイベントを発行したTRBへのポインタ 
        // logging::LoggingLevel current_level = logger->current_level(); //  一旦出力を減らす
        // logger->set_level(logging::kINFO);
        // logger->debug("[OnEvent] TransferEvent\n");
        // logger->debug("    | Issuer: %s (%p)\n", kTRBTypeToName[issuer_trb->bits.trb_type], issuer_trb);
        // logger->debug("    | SlotID: %hhd\n", trb->bits.slot_id);
        // logger->debug("    | EndpointID: %hhd\n", trb->bits.endpoint_id);
        // logger->debug("    | CompletionCode: %s\n", kTRBCompletionCodeToName[trb->bits.completion_code]);
        // logger->set_level(current_level);

        uint8_t slot_id = trb->bits.slot_id;
        Device *dev = devmgr_.FindBySlot(slot_id); // デバイスの特定を行う
        if (dev == NULL) {
            logger->error("There Are No Device For Issuer TRB.\n");
            return -1;
        }
        dev->OnTransferEventReceived(trb);
        
        uint8_t port_id = dev->DeviceContextPtr()->slot_context.bits.root_hub_port_num;

        if (initializing_port[port_id]) { // 初期化中の時は毎度起こすようにする
            task_manager->Wakeup(initializing_port[port_id]);
        }
        return -1;
    }

    int Controller::ResetPort(Port *port)
    {
        if (!port->IsConnected()) {
            /*  指定したポートにUSBデバイスが接続されていない場合は、
                エラーを返す。 */
            return -1;
        }
        
        if (addressing_port != 0) {
            /*  他のポートがアドレス割当の処理中なのでリセット処理ができない */
            port_config_phase[port->Number()] = ConfigPhase::kWaitingAddressed;
        } else {
            if (port_config_phase[port->Number()] != ConfigPhase::kNotConnected &&
                port_config_phase[port->Number()] != ConfigPhase::kWaitingAddressed) {
                return -1;
            } else {
                addressing_port = port->Number();
                port_config_phase[port->Number()] = ConfigPhase::kResettingPort;
                port->Reset();
            }
        }
        return 0;
    }

    int Controller::EnableSlot(Port *port)
    {
        /*  ポートが有効かつリセット処理が無事終わった場合のみ処理が行われる。
            これらの条件を満たしていない場合は、エラーを吐く。 */
        if (!port->IsEnabled()) return -1; // ポートが有効か？
        if (!port->IsPortResetChanged()) return -1; // リセット処理が完了しているか？
        if (addressing_port != port->Number()) {
            logger->error("This Port Is Not Addressing Port\n");
            return -1;
        }
        // logger->info("[+] ENABLE SLOT FOR PORT%02hhd\n", port->Number());

        port->ClearPortResetChange();

        EnableSlotCommandTRB command_trb = EnableSlotCommandTRB{};
        cr_.Push(&command_trb);
        RingCommandRing();
        return 0;
    }

    int Controller::DisableSlot(uint8_t slot_id)
    {
        // logger->info("[+] DISABLE SLOT%02hhd\n", slot_id);
        DisableSlotCommandTRB command_trb(slot_id);
        
        cr_.Push(&command_trb);
        RingCommandRing();
        return 0;
    }

    int Controller::AddressDevice(uint8_t port_id, uint8_t slot_id)
    {
        int res;
        // logger->debug("[+] ADRESS DEVICE (PORT%02hhd, SLOT%02hhd)\n", port_id, slot_id);

        res = devmgr_.AllocDevice(slot_id, &doorbell_[slot_id]);
        if (res == -1) {
            printk("[DEBUG CTRer::AddressDev] FAILED!! AllocDevice\n");
            return -1;
        }

        Device *device = devmgr_.FindBySlot(slot_id);

        //  2. InputControlContextのA０とA１を１にする。
        SlotContext *slot_ctx = device->InputContextPtr()->EnableSlotContext();
        EndpointContext *ep0_ctx = device->InputContextPtr()->EnableEndpoint(DeviceContextIndex{1});

        //  3. InputSlotContextを初期化。
        Port *port = PortAt(addressing_port);
        slot_ctx->bits.root_hub_port_num = port->Number();
        slot_ctx->bits.route_string = 0;
        slot_ctx->bits.context_entries = 1;
        slot_ctx->bits.speed = port->Speed();     //  ここよく分かってない。

        //  4. EP０の初期化。
        ep0_ctx->bits.ep_type = 4;
        ep0_ctx->bits.max_packet_size = DetermineMaxPacketSizeForControlPipe(slot_ctx->bits.speed);
        ep0_ctx->bits.max_burst_size = 0;

        //  5. デバイスのデフォルトコントロールパイプにTransferRingを作成し初期化
        Ring *ep0_transfer_ring = device->AllocTransferRing(DeviceContextIndex{1}, 32);

        //  6. EP０コンテキストにTransferRingのポインタを書き込む
        ep0_ctx->SetTransferRingBuffer(ep0_transfer_ring->Buffer());
        ep0_ctx->bits.dequeue_cycle_state = 1;
        ep0_ctx->bits.interval = 0;
        ep0_ctx->bits.max_primary_streams = 0;
        ep0_ctx->bits.mult = 0;
        ep0_ctx->bits.error_count = 3;

        //  7. DCBAAにデバイスコンテキストを追加する。
        devmgr_.LoadDCBAA(slot_id);

        //  8.  AddressDeviceCommandTRBの発行。
        AddressDeviceCommandTRB addr_dev_cmd(device->InputContextPtr(), slot_id);
        cr_.Push(&addr_dev_cmd);
        RingCommandRing();

        return 0;
    }
    



    Controller *xhc;

// カーネルから参照される手続き
    void Initialize()
    {
        pci::Device *xhc_dev = pci::FindXHC();
        if (!xhc_dev) { // xHCが存在しなかった場合はなにもしない
            return;
        }

        // xHCのメモリマップドIOを取得
        uint64_t mmio_base_low = static_cast<uint64_t>(
            pci::ConfigRead32(xhc_dev->bus, xhc_dev->device, xhc_dev->function, 0x10));
        uint64_t mmio_base_high = static_cast<uint64_t>(
            pci::ConfigRead32(xhc_dev->bus, xhc_dev->device, xhc_dev->function, 0x14));
        uint64_t mmio_base = ((mmio_base_high << 32) | mmio_base_low) & ~0xflu;
        logger->debug("MMIO BASE: %lx\n", mmio_base);

        xhc = new Controller(mmio_base);
        xhc->Initializer();

        xhc->Run();
        xhc->SendNoOpCommand();
        
        xhc->AllPortsInit();





    }

    void ProcessEvents()
    {
        // logger->debug("[+] Process Events!!\n");
        while (xhc->PrimaryEventRing()->HasFront()) {
            xhc->ProcessEvent();
        }
    }

    // 初期化中にイベントを待つ
    void WaitEvent(uint8_t port_num)
    {
        Task *current_task = task_manager->CurrentTask();
        initializing_port[port_num] = current_task->ID();
        current_task->Sleep();
    }

    // 初期化中に失敗したら永久スリープ
    void ErrorOnInit() {
        task_manager->CurrentTask()->Sleep();
        Halt();
    }

// 初期化時に使用される関数
    void InitUSBDeviceTask(uint64_t id, int64_t port_num_)
    {
        uint8_t port_num = static_cast<uint8_t>(port_num_);
        Port *port = xhc->PortAt(port_num);

        
        printk("[TASK %ld] PORT%d WILL BE INITIALIZED.\n", id, port_num);
        if (xhc->ResetPort(port)) {
            ErrorOnInit();
        }

        WaitEvent(port_num);

        printk("PORT%d RESET COMPLETED.\n", port_num);
        if (xhc->EnableSlot(port)) {
            ErrorOnInit();
        }

        WaitEvent(port_num);

        uint8_t slot_num = port->Slot();
        printk("SLOT%d ENABLED FOR PORT%d\n", slot_num, addressing_port);
        xhc->slot_to_port_[slot_num] = port_num;
        xhc->AddressDevice(port_num, slot_num);

        WaitEvent(port_num);

        printk("ADDRESS DEVICE ON PORT%d\n", port_num);
        Device *dev = xhc->devmgr_.FindBySlot(slot_num);
            
        /*     // アドレス割当が完了したので、アドレス処理待ちの他のポートの割当を行う。
            addressing_port = 0;
            for (uint8_t i = MaxPorts(); i > 0; i--) {
                if (port_config_phase[i] == ConfigPhase::kWaitingAddressed) {
                    Port *port = PortAt(i);
                    res = ResetPort(port);
                    if (res == -1) {
                        logger->error("Failed To Reset Port%02hhd\n", i);
                        continue;
                    }
                    break;
                }
            } */

        dev->StartInitialize(); // デバイスの初期化処理

        while (!dev->IsInitialized()) {
            WaitEvent(port_num);   
        }
        printk("DEVICE CONFIGURED!!\n");
        xhc->ConfigureEndpoints(dev);

        WaitEvent(port_num);   

        dev->OnEndpointsConfigured();
        printk("PORT%d (SLOT%d) REACHED TO CONFIGURED STATE.\n", port_num, slot_num);
        
        initializing_port[port_num] = 0;
        Halt();
    }
}