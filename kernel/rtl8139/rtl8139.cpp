
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

    void Initialize()
    {
        pci::Device *rtl8139 = pci::FindRTL8139();
    }
}