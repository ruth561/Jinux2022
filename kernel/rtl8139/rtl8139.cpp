
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

        logger->info("Command Register: %hhx\n", opt_->command_register.data);
        opt_->command_register.bits.reset = 1;
        while (opt_->command_register.bits.reset) continue;
        logger->info("RTL8139 Reset Completed.\n");
        logger->info("Command Register: %hhx\n", opt_->command_register.data);

        // Receive用のバッファを確保する
        size_t rx_buffer_size = 8192 + 16;
        rx_buffer_ = reinterpret_cast<uint64_t>(malloc(rx_buffer_size));
        if (rx_buffer_ > 0xfffffffful) { // バッファは32bitメモリ空間に存在していなければならない
            logger->error("[RTL8139] Rx Buffer Pointer Is Over 32bit Address.\n");
            return -1;
        }
        memset(reinterpret_cast<void *>(rx_buffer_), 0, rx_buffer_size);
        logger->debug("Rx Buffer: %p\n", rx_buffer_);
        opt_->receive_buffer_start_address = static_cast<uint32_t>(rx_buffer_);

        logger->debug("Interrupt Mask Register: 0x%04hx\n", opt_->interrupt_mask_register.data);
        // 全ての割り込みを許可する
        opt_->interrupt_mask_register.bits.receive_ok_interupt = true;
        opt_->interrupt_mask_register.bits.receive_error_interrupt = true;
        opt_->interrupt_mask_register.bits.transmit_ok_interupt = true;
        opt_->interrupt_mask_register.bits.transmit_error_interupt = true;
        opt_->interrupt_mask_register.bits.rx_buffer_overflow_interrupt = true;
        opt_->interrupt_mask_register.bits.rx_fifo_overflow_interrupt = true;
        opt_->interrupt_mask_register.bits.packet_underrun_link_change_interupt = true;
        opt_->interrupt_mask_register.bits.cable_length_change_interupt = true;
        opt_->interrupt_mask_register.bits.timeout_interupt = true;
        opt_->interrupt_mask_register.bits.system_error_interupt = true;
        logger->debug("Interrupt Mask Register: 0x%04hx\n", opt_->interrupt_mask_register.data);



        logger->debug("Receive Configuration Register: %x\n", opt_->receive_configuration_register.data);
        opt_->receive_configuration_register.bits.wrap = 0; // 通常のリングバッファ（オーバーフローさせない）
        opt_->receive_configuration_register.bits.rx_buffer_length = 0; // リングの大きさは8192+16bytes
        logger->debug("Receive Configuration Register: %x\n", opt_->receive_configuration_register.data);

        // opt_->current_address_of_packet_read = 0; // リングバッファへのポインタを０で初期化する
        opt_->command_register.bits.receiver_enable = 1; // Receiveを開始する



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

        while (1) {
            rtl8139->ReceivePacket();
        }

    }

    void Controller::ReceivePacket()
    {
        if (opt_->current_buffer_address == opt_->current_address_of_packet_read) {
            // バッファに未読のパケットがない場合
            return;
        }
        Packet *packet = reinterpret_cast<Packet *>(rx_buffer_ + opt_->current_address_of_packet_read);
        printk("Header: %04hx\n", packet->header.data);
        printk("Length: %d\n", packet->length);
        printk("Data:   ");
        for (int i = 0; i < packet->length; i++) {
            printk("%02hhx ", packet->data[i]);
        }

        opt_->current_address_of_packet_read = opt_->current_buffer_address;
    }

}