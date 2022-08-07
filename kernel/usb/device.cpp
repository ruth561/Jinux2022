#include "device.hpp"
#include "../logging.hpp"
extern logging::Logger *logger;
int printk(const char *format, ...);
  
namespace
{
    using namespace usb;

    // USBコンフィギュレーションディスクリプタを読み込むクラス
    class ConfigurationDescriptorReader
    {
    public:
        ConfigurationDescriptorReader(uint8_t *desc_buf, int desc_buf_len) : 
            desc_buf_(desc_buf), 
            desc_buf_len_(desc_buf_len),
            p_(desc_buf) {}
        
        uint8_t DescriptorType() { return p_[1]; }

        // 次のディスクリプタへのポインタを返す。
        // p_の値も内部で更新している
        // 存在しない場合、NULLを返す。
        uint8_t *Next() {
            p_ += p_[0];
            if (p_ < desc_buf_ + desc_buf_len_) {
                return p_;
            }
            return nullptr;
        }

        //  現在のpが指すアドレスを返す
        void *Addr() {
            return (void *) p_;
        }
        
    private:
        //  ディスクリプタの先頭ポインタ。
        uint8_t *desc_buf_;
        //  ディスクリプタのサイズ。
        const int desc_buf_len_;
        //  各ディスクリプタを走査する時に使うポインタ。
        uint8_t *p_;

    };

}
 

namespace usb
{
    int Device::StartInitialize()
    {
        is_initialized_ = false;
        initialize_phase_ = 1;
        return GetDescriptor(kDefaultControlPipeID, descriptor_type::kDevice, 0, buf_, sizeof(buf_));
    }

    int Device::InitializePhase1(DeviceDescriptor *dev_desc)
    {
        logger->debug("[DEVICE INIT PHAZE 1] GET DEVICE DESCRIPTOR\n");
        logger->debug("    | Type: %hhd\n", dev_desc->descriptor_type);
        logger->debug("    | Length: %d\n", dev_desc->length);
        logger->debug("    | UsbSpecificationReleaseNumber: %xH\n", dev_desc->usb_release);
        logger->debug("    | [Class, SubClass, Protocol]: [%02hhd, %02hhd, %02hhd]\n", 
            dev_desc->device_class, dev_desc->device_sub_class, dev_desc->device_protocol);
        logger->debug("    | MaxPacketSize: %d\n", 1 << dev_desc->max_packet_size);
        logger->debug("    | VendorId: %xH\n", dev_desc->vendor_id);
        logger->debug("    | ProductId: %xH\n", dev_desc->product_id);
        logger->debug("    | NumConfigurations: %d\n", dev_desc->num_configurations);

        num_configurations_ = dev_desc->num_configurations;
        config_index_ = 0;

        initialize_phase_ = 2;
        return GetDescriptor(kDefaultControlPipeID, descriptor_type::kConfiguration, config_index_, buf_, sizeof(buf_));
    }
 
    int Device::InitializePhase2(ConfigurationDescriptor *config_desc, int len)
    {
        logger->debug("[DEVICE INIT PHAZE 2] GET CONFIGURATION DESCRIPTOR\n");
        logger->debug("    | Length: %d\n", config_desc->length);
        logger->debug("    | DescriptorType: %d\n", config_desc->descriptor_type);
        logger->debug("    | TotalLength: %d\n", config_desc->total_length);
        logger->debug("    | NumInterfaces: %d\n", config_desc->num_interfaces);

        ConfigurationDescriptorReader config_reader((uint8_t *) config_desc, len);
        // ClassDriver *class_driver = nullptr;
        num_ep_configs_ = 0;
        while (config_reader.Next())
        {
            uint8_t desc_type = config_reader.DescriptorType();
            if (desc_type == descriptor_type::kConfiguration) {
                ConfigurationDescriptor *config_desc = reinterpret_cast<ConfigurationDescriptor *>(config_reader.Addr());
                logger->debug("    | [CONFIGURATION]\n");
                logger->debug("         | NumInterfaces: %d\n", config_desc->num_interfaces);
            } else if (desc_type == descriptor_type::kInterface) {
                InterfaceDescriptor *interface_desc = (InterfaceDescriptor *) config_reader.Addr();
                logger->debug("    | [INTERFACE]\n");
                logger->debug("         | InterfaceNumber: %d\n", interface_desc->interface_number);
                logger->debug("         | NumEndpoints: %d\n", interface_desc->num_endpoints);
                logger->debug("         | InterfaceClass: %d\n", interface_desc->interface_class);
                logger->debug("         | InterfaceSubClass: %d\n", interface_desc->interface_sub_class);
                logger->debug("         | InterfaceProtocol: %d\n", interface_desc->interface_protocol);

                uint32_t class_number = (static_cast<uint32_t>(interface_desc->interface_class) << 16) 
                    | (static_cast<uint32_t>(interface_desc->interface_sub_class) << 8)
                    | static_cast<uint32_t>(interface_desc->interface_protocol);
                logger->debug("Class NUmber: %06x\n", class_number);
                if (class_number == 0x030101) { // HID BOOT INTERFACE (KEYBOARD)
                    class_drivers_[interface_desc->interface_number] = new HIDKeyboardDriver(this, interface_desc->interface_number);
                } /* else if (class_number == 0x030102) { // HID BOOT INTERFACE (MOUSE)
                    class_drivers_[interface_desc->interface_number] = new HIDClassDriver(this, interface_desc->interface_number);
                } */
            } else if (desc_type == descriptor_type::kEndpoint) {
                /*  Endpoint Descriptor */
                EndpointDescriptor *ep_desc = (EndpointDescriptor *) config_reader.Addr();
                logger->debug("    | [ENDPOINT]\n");
                logger->debug("         | EndpointAddress: 0x%x\n", ep_desc->endpoint_address);
                logger->debug("         | MaxPacketSize: %d\n", ep_desc->max_packet_size);

                /* if (class_driver) {
                    EndpointConfig conf = {};
                    conf.ep_id = EndpointID(ep_desc->endpoint_address.bits.number, 
                                            ep_desc->endpoint_address.bits.dir_in == 1);
                    conf.ep_type = (EndpointType) (ep_desc->attributes.bits.transfer_type);
                    conf.max_packet_size = ep_desc->max_packet_size;
                    conf.interval = ep_desc->interval;
                    ep_configs_[num_ep_configs_] = conf;
                    num_ep_configs_++;
                    class_drivers_[conf.ep_id.EndpointNumber()] = class_driver;
                } */
            } else if (desc_type == descriptor_type::kHID) {
                HIDDescriptorReader hid_desc_reader((HIDDescriptorHeader *) config_reader.Addr());
                logger->debug("    | [HID]\n");
                logger->debug("         | CountryCode: %d\n", hid_desc_reader.desc_->country_code);
                logger->debug("         | ClassDescriptorNum: %d\n", hid_desc_reader.desc_->num_descriptors);
                uint8_t hid_class_index = 0;
                HIDClassDescriptor *hid_class_desc;
                /* while (1) {
                    hid_class_desc = hid_desc_reader.GetClassDescriptor(hid_class_index);
                    if (hid_class_desc == nullptr) break;
                    printk("        DescriptorType: %d\n", hid_class_desc->descriptor_type);
                    printk("        DescriptorLength: %d\n", hid_class_desc->descriptor_length);
                    hid_class_index++;
                } */

            } else {
                logger->debug("    | [OTHER %d]\n", desc_type);
            }

        }

        /* if (!class_driver) {
            printk("[ERROR] This USB device is not supported\n");
            return -1;
        } */

        initialize_phase_ = 3;
        return SetConfiguration(kDefaultControlPipeID, config_desc->configuration_value);
    }

    int Device::InitializePhase3(uint8_t config_value)
    {
        logger->info("[+] SET CONFIGURATION %d!!\n", config_value);
        for (int i = 0; i < sizeof(class_drivers_) / sizeof(class_drivers_[0]); ++i) {
            if (class_drivers_[i]) {
                logger->debug("ClassDriver: %p\n", class_drivers_[i]);
                class_drivers_[i]->Run();
            }
        }
        initialize_phase_ = 4;
        is_initialized_ = true;
        logger->info("[+] Device Configured.\n");
        return 0;
    }

    
    int Device::OnControlCompleted(EndpointID ep_id, SetupData setup_data, void *buf, int len)
    {
        logger->debug("[Device::OnControlCompleted] buf: %p, len: %d\n", buf, len);
        if (is_initialized_) { // 初期化が完了している時
        /* 
        
        めちゃくちゃ適当！！！
        
         */

            if (setup_data.request_type.bits.type == request_type::kClass && class_drivers_[setup_data.index]) {
                class_drivers_[setup_data.index]->OnControlCompleted(ep_id, setup_data, buf, len);
            }
            
            /* if (auto class_driver = event_waiters_.Get(&setup_data)) {
                event_waiters_.Delete(&setup_data);
                return class_driver->OnControlCompleted(ep_id, setup_data, buf, len);
            }
            printk("[Error] There are no event waiter\n"); */
            return 0;
        }

        // 初期化中の場合
        if (initialize_phase_ == 1) {
            // GET_DESCRIPTORからの返信
            // GET_DESCRIPTOR(CONFIGURATION)を実行する
            DeviceDescriptor *dev_desc = reinterpret_cast<DeviceDescriptor *>(buf);
            return InitializePhase1(dev_desc);
        } else if (initialize_phase_ == 2) {
            // GET_DESCRIPTOR(CONFIGURATION)からの返信
            // SET_CONFIGURATIONを実行する
            ConfigurationDescriptor *config_desc = reinterpret_cast<ConfigurationDescriptor *>(buf);
            return InitializePhase2(config_desc, len);
        } else if (initialize_phase_ == 3) {
            return InitializePhase3(setup_data.value & 0xffu);
        }

        return -1;
    }




    int Device::GetDescriptor(EndpointID ep_id, uint8_t desc_type, uint8_t desc_index, void *buf, int len)
    {
        SetupData setup_data = {};
        setup_data.request_type.bits.direction = request_type::kIn;
        setup_data.request_type.bits.type = request_type::kStandard;
        setup_data.request_type.bits.recipient = request_type::kDevice;
        setup_data.request = request::kGetDescriptor;
        // 上位バイトがディスクリプタのタイプ、下位バイトがディスクリプタのインデックスである。
        setup_data.value = (((uint16_t) desc_type) << 8) | desc_index;
        setup_data.index = 0;
        setup_data.length = len;
        return ControlIn(ep_id, setup_data, buf, len);
    }

    int Device::SetConfiguration(EndpointID ep_id, uint8_t config_value)
    {
        SetupData setup_data{};
        setup_data.request_type.bits.direction = request_type::kOut;
        setup_data.request_type.bits.type = request_type::kStandard;
        setup_data.request_type.bits.recipient = request_type::kDevice;
        setup_data.request = request::kSetConfiguration;
        setup_data.value = config_value; // 上位１ByteはReservedなので、、
        setup_data.index = 0;
        setup_data.length = 0;
        return ControlOut(ep_id, setup_data, nullptr, 0); // データ領域は特になし
    }

}