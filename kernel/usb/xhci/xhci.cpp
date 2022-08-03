#include "xhci.hpp"

int printk(const char *format, ...);

extern logging::Logger *logger;
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
        opt_->CONFIG.bits.max_device_slots_enabled = cap_->HCSPARAMS1.bits.max_device_slots;

        // デバイスマネージャーの初期化
        devmgr_.Initialize(cap_->HCSPARAMS1.bits.max_device_slots);
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

    void Controller::SendNoOpCommand()
    {
        NoOpCommandTRB trb = NoOpCommandTRB();
        cr_.Push(&trb);
        doorbell_[0].Ring(0, 0);
    }   





    Controller *xhc;

    // カーネルから呼び出される関数
    // PCIの初期化を行った後に呼び出す必要がある
    // グローバルなxhcにControllerのオブジェクトを作り、初期化をする。
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

        for (int i = 0; i < 5; i++) {
            xhc->SendNoOpCommand();
        }


    }
}