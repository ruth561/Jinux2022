
#include "rtl8139.hpp"

int printk(const char *format, ...);
void Halt();
extern logging::Logger *logger;


namespace pci 
{
    extern std::vector<Device> devices;

    // PCIデバイスの中からxHCを一つ見つけ出し、devices要素へのポインタで返す
    pci::Device *FindRTL8139() {
        for (int i = 0; i < pci::devices.size(); i++) {
            if (pci::devices[i].vendor_id == 0x10ec &&
                pci::devices[i].device_id == 0x8139) {
                logger->debug("RTL8139 found!\n");
                /* 
                 * MSIの設定もする？
                 */
                return &pci::devices[i];
            }
        }
        return nullptr;
    }
}


namespace rtl8139
{
    Controller *rtl8139;

    Controller::Controller(uint32_t mmio_base) : 
        mmio_base_{mmio_base}, 
        opt_{reinterpret_cast<OperationalRegister *>(mmio_base_)} {}

    int Controller::Initialize()
    {
        logger->info("MAC address: %012lx\n", opt_->mac_address);
        logger->info("CONFIG1: %hhx\n", opt_->config1.data);
        opt_->config1.data = 0; // power on


        return 0;
    }

    void Initialize()
    {
        pci::Device *rtl8139_dev = pci::FindRTL8139();
        if (rtl8139_dev == nullptr) {
            return;
        }

        uint32_t data = pci::ConfigRead32(rtl8139_dev->bus, rtl8139_dev->device, rtl8139_dev->function, 4);
        logger->debug("Command: 0x%hx\n", data);
        logger->debug("Status: 0x%hx\n", data >> 16);
        pci::ConfigWrite32(data | (1 << 1), rtl8139_dev->bus, rtl8139_dev->device, rtl8139_dev->function, 4); // DMAの許可

        data = pci::ConfigRead32(rtl8139_dev->bus, rtl8139_dev->device, rtl8139_dev->function, 0x14);
        logger->debug("Memory Address: 0x%x\n", data);
        if (data & 1) { // Bit０が１の時
            logger->error("Cannot Access To Memory Mapped Registers.\n");
            return;
        }
        uint32_t mmio_base = data & ~0xffu;
        logger->debug("mmio_base: 0x%x\n", mmio_base);

        rtl8139 = new Controller{mmio_base};
        rtl8139->Initialize();

    }
}