#include "pci.hpp"
#include "interrupt.hpp"


extern logging::Logger *logger;
namespace
{
    using namespace pci;

    uint16_t ReadVendorID(uint8_t bus, uint8_t dev, uint8_t func)
    {
        return static_cast<uint16_t>(pci::ConfigRead32(bus, dev, func, 0) & 0xffffu);
    }

    uint16_t ReadDeviceID(uint8_t bus, uint8_t dev, uint8_t func)
    {
        return static_cast<uint16_t>((pci::ConfigRead32(bus, dev, func, 0) >> 16) & 0xffffu);
    }

    uint32_t ReadClassCode(uint8_t bus, uint8_t dev, uint8_t func)
    {
        return pci::ConfigRead32(bus, dev, func, 8);
    }

    uint8_t ReadHeaderType(uint8_t bus, uint8_t dev, uint8_t func)
    {
        return static_cast<uint8_t>((pci::ConfigRead32(bus, dev, func, 0xc) >> 16) & 0xff);
    }

    uint8_t ReadBaseClass(uint8_t bus, uint8_t dev, uint8_t func)
    {
        return static_cast<uint8_t>((pci::ConfigRead32(bus, dev, func, 0x8) >> 24) & 0xff);
    }

    uint8_t ReadSubClass(uint8_t bus, uint8_t dev, uint8_t func)
    {
        return static_cast<uint8_t>((pci::ConfigRead32(bus, dev, func, 0x8) >> 16) & 0xff);
    }

    uint8_t ReadInterface(uint8_t bus, uint8_t dev, uint8_t func)
    {
        return static_cast<uint8_t>((pci::ConfigRead32(bus, dev, func, 0x8) >> 8) & 0xff);
    }

    uint8_t ReadCapabilitiesPointer(uint8_t bus, uint8_t dev, uint8_t func)
    {
        return static_cast<uint8_t>(pci::ConfigRead32(bus, dev, func, 0x34) & 0xff);
    }

    // PCIデバイスのコンフィギュレーション空間内のCapabilityListをたどり、
    // deviceに設定する。
    void ScanCapabilityList(Device *dev)
    {
        uint8_t cap_ptr = ReadCapabilitiesPointer(dev->bus, dev->device, dev->function);
        uint8_t cap_id;
        uint32_t cap_header;
        while (cap_ptr) {
            cap_header = ConfigRead32(dev->bus, dev->device, dev->function, cap_ptr);
            cap_id = static_cast<uint8_t>(cap_header & 0xffu);
            dev->capability_list.push_back(CapabilityRegister{cap_ptr, cap_id});

            cap_ptr = static_cast<uint8_t>((cap_header >> 8) & 0xffu);
        }
    }
}

namespace pci
{
    std::vector<Device> devices; // PCの持つPCIデバイスを管理する配列

    void ConfigWrite32(uint32_t data, uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
    {
        uint32_t lbus = bus;
        uint32_t ldev = dev;
        uint32_t lfunc = func;
        uint32_t loffset = offset;

        uint32_t config_address = (1u << 31)
            | (lbus << 16)
            | (ldev << 11)
            | (lfunc << 8)
            | (loffset & 0xfc);

        WriteIOAddressSpace32(kConfigAddress, config_address); // CONFIG_ADDRESSレジスタへ書き込み
        WriteIOAddressSpace32(kConfigData, data);
    }

    uint32_t ConfigRead32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
    {
        uint32_t lbus = bus;
        uint32_t ldev = dev;
        uint32_t lfunc = func;
        uint32_t loffset = offset;

        uint32_t config_address = (1u << 31)
            | (lbus << 16)
            | (ldev << 11)
            | (lfunc << 8)
            | (loffset & 0xfc);

        WriteIOAddressSpace32(kConfigAddress, config_address); // CONFIG_ADDRESSレジスタへ書き込み
        return ReadIOAddressSpace32(kConfigData);
    }


    // devがMSIに対応していたら、現在稼働中のCPUに割り込みをするように設定する。
    // 成功時０、失敗時−１を返す。
    int ConfigureMSI(Device *dev)
    {
        for (auto cap = dev->capability_list.begin(); cap != dev->capability_list.end(); cap++) {
            if (cap->id == 5) { // Capability IDがMSIだったら
                uint32_t msi_header = ConfigRead32(
                    dev->bus, dev->device, dev->function, cap->offset);
                MSIMessageControl message_control;
                message_control.data = static_cast<uint16_t>((msi_header >> 16) & 0xffffu);
                logger->info("[MSI CONFIG] %s mode\n", message_control.bits.address_64_bit ? "64" : "32");
                logger->info("[MSI CONFIG] Per-vector masking %s\n", message_control.bits.per_vector_masking_capable ? "enable" : "disable");
                if ((!message_control.bits.address_64_bit) || message_control.bits.per_vector_masking_capable)
                    continue;
                
                // Message Addressの書き込み
                uint8_t local_apic_id = *reinterpret_cast<uint32_t *>(0xfee00020) >> 24;
                ConfigWrite32(0xfee00000u | (static_cast<uint32_t>(local_apic_id) << 12), 
                    dev->bus, dev->device, dev->function, cap->offset + 4);
                ConfigWrite32(0, // UPPER
                    dev->bus, dev->device, dev->function, cap->offset + 8);

                // Message Dataの書き込み
                uint32_t message_data = static_cast<uint32_t>(InterruptVector::kXHCI) | (1 << 14) | (1 << 15);
                ConfigWrite32(message_data, 
                    dev->bus, dev->device, dev->function, cap->offset + 12);

                // Message Controlへの書き込み
                message_control.data = 0;
                message_control.bits.enable = 1;
                ConfigWrite32(static_cast<uint32_t>(message_control.data) << 16, 
                    dev->bus, dev->device, dev->function, cap->offset);

                return 0;
            }
        }
        return -1;
    }

    

}

void ScanPCIBus()
{
    for (int bus_ = 0; bus_ < 256; bus_++) { // TODO: ここもっときれいに行う方法はあるのか？？
        uint8_t bus = (uint8_t) bus_;
        for (uint8_t dev = 0; dev < 32; dev++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint16_t vendor_id = ReadVendorID(bus, dev, func);
                if (vendor_id != 0xffffu) {
                    uint16_t device_id = ReadDeviceID(bus, dev, func);
                    uint32_t class_code = ReadClassCode(bus, dev, func);
                    uint8_t header_type = ReadHeaderType(bus, dev, func);
                    logger->debug("[%02hhx.%02hhx.%02hhx] VENDOR: %4hxH, DEVICE: %4hxH, CLASS: %08xH, HEADER TYPE: %02xH\n", 
                        bus, dev, func, vendor_id, device_id, class_code, header_type);
                    
                    uint8_t base_class = ReadBaseClass(bus, dev, func);
                    uint8_t sub_class = ReadSubClass(bus, dev, func);
                    uint8_t interface = ReadInterface(bus, dev, func);
                    devices.push_back(Device{ // deviceのリストに追加
                        bus, dev, func, base_class, sub_class, interface, header_type
                    });
                    ScanCapabilityList(&devices.back()); // このデバイスのCapabilityListを走査
                }
            }
        }
    }
}

void InitializePCI()
{
    ScanPCIBus();

    for (auto dev = devices.begin(); dev != devices.end(); dev++) {
        logger->info("[%02hhx.%02hhx.%02hhx] BASE_CLASS: %02hhxH, SUB_CLASS: %02hhxH, INTERFACE: %02hhxH\n", 
            dev->bus, dev->device, dev->function, dev->base_class, dev->sub_class, dev->interface);
        for (auto cap = dev->capability_list.begin(); cap != dev->capability_list.end(); cap++) {
            logger->info("           CAPABILITY_ID: %02hhxH\n", cap->id);
        }

        if (dev->base_class == 0x0c && dev->sub_class == 0x03 && dev->interface == 0x30) {
            logger->debug("USB3.0 HOST CONTROLLER!!\n");
            if (ConfigureMSI(&*dev)) {
                logger->error("Failed to configure MSI.\n");
            }
        }
    }

    return;
}
