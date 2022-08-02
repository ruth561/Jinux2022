#include "xhci.hpp"

int printk(const char *format, ...);

extern logging::Logger *logger;
namespace pci 
{
    extern std::vector<Device> devices;
}




namespace usb::xhci
{   
    Controller::Controller(uintptr_t mmio_base) :
        mmio_base_(mmio_base),
        cap_((CapabilityRegisters *) mmio_base_),
        opt_((OperationalRegisters *) (mmio_base_ + cap_->CAPLENGTH)),
        run_((RuntimeRegisters *) (mmio_base_ + cap_->RTSOFF.bits.runtime_register_space_offset)),
        doorbell_((Doorbell_Bitmap *) (mmio_base_ + cap_->DBOFF.Offset())) {
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
        printk("%d\n", opt_->CONFIG.data[0]);
        opt_->CONFIG.bits.max_device_slots_enabled = cap_->HCSPARAMS1.bits.max_device_slots;
        printk("%d\n", opt_->CONFIG.data[0]);

        // デバイスマネージャーの初期化
        devmgr_.Initialize(cap_->HCSPARAMS1.bits.max_device_slots);
        opt_->DCBAAP.SetPointer(devmgr_.DeviceContexts());
        logger->info("[xHC Init] DCBAA           %p\n", opt_->DCBAAP.Pointer());

        return -1;
    }





    Controller *xhc;

    // カーネルから呼び出される関数
    // PCIの初期化を行った後に呼び出す必要がある
    // グローバルなxhcにControllerのオブジェクトを作り、初期化等をする。
    void Initialize()
    {
        pci::Device *xhc_dev = nullptr;
        for (int i = 0; i < pci::devices.size(); i++) {
            if (pci::devices[i].base_class == 0x0c &&
                pci::devices[i].sub_class == 0x03 &&
                pci::devices[i].interface == 0x30) {
                logger->debug("USB3.0 HOST CONTROLLER!!\n");
                xhc_dev = &pci::devices[i];
                if (pci::ConfigureMSI(&pci::devices[i])) {
                    logger->error("Failed to configure MSI.\n");
                }
                break;
            }
        }
        if (!xhc_dev) { // xHCが存在しなかった場合はなにもしない
            return;
        }
        uint64_t mmio_base_low = static_cast<uint64_t>(
            pci::ConfigRead32(xhc_dev->bus, xhc_dev->device, xhc_dev->function, 0x10));
        uint64_t mmio_base_high = static_cast<uint64_t>(
            pci::ConfigRead32(xhc_dev->bus, xhc_dev->device, xhc_dev->function, 0x14));
        uint64_t mmio_base = ((mmio_base_high << 32) | mmio_base_low) & ~0xflu;
        logger->debug("MMIO BASE: %lx\n", mmio_base);

        xhc = new Controller(mmio_base);
        xhc->Initializer();


    }
}