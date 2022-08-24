
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
        
        // power on
        opt_->config1.data = 0;

        // reset
        opt_->command_register.bits.reset = 1;
        while (opt_->command_register.bits.reset) {}
        logger->info("RTL8139 Reset Completed.\n");

        // Rx Bufferの確保
        rx_buffer_size_ = 8192 + 16;
        rx_buffer_ = reinterpret_cast<uint64_t>(malloc(rx_buffer_size_));
        if (rx_buffer_ > 0xfffffffful) { 
            // バッファは32bitメモリ空間に存在していなければならない
            logger->error("[RTL8139] Rx Buffer Pointer Is Over 32bit Address.\n");
            return -1;
        }
        memset(reinterpret_cast<void *>(rx_buffer_), 0, rx_buffer_size_);
        opt_->receive_buffer_start_address = static_cast<uint32_t>(rx_buffer_);
        rx_offset_ = 0; // Rx Offsetをバッファの先頭に初期化
        logger->debug("Rx Buffer: %p\n", rx_buffer_);

        // 全ての割り込みを許可する
        logger->debug("Interrupt Mask Register: 0x%04hx\n", opt_->interrupt_mask_register.data);
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

        // Rx Configの設定
        logger->debug("Receive Configuration Register: %08x\n", opt_->receive_configuration_register.data);
        opt_->receive_configuration_register.bits.wrap = 0; // 通常のリングバッファ（オーバーフローさせない）
        opt_->receive_configuration_register.bits.rx_buffer_length = 0; // リングの大きさは8192+16bytes
        opt_->receive_configuration_register.bits.multiple_early_interrupt_select = 1; // パケットを受け取るたびに割り込みを発生させる
        logger->debug("Receive Configuration Register: %08x\n", opt_->receive_configuration_register.data);

        // Receiveを開始する
        opt_->command_register.bits.receiver_enable = 1;
        logger->debug("CAPR: %hx, CBA: %hx\n", opt_->current_address_of_packet_read, opt_->current_buffer_address);

        return 0;
    }

    void Controller::ReceivePacket()
    {
        if (opt_->command_register.bits.buffer_empty) {
            // Rx Bufferに何も入っていない場合
            return;
        }

        Packet *packet = reinterpret_cast<Packet *>(rx_buffer_ + rx_offset_);
        if (packet->HasError()) {
            logger->error("Packet Error\n");
            logger->error("Header: %04hx\n", packet->header.data);
            return;
        }

        uint16_t packet_size = packet->length - 4; // 先頭４バイト分はいらない
        printk("Packet:   ");
        for (int i = 0; i < packet_size; i++) {
            printk("%02hhx ", packet->data[i]);
        }
        printk("\n");

        if (rx_offset_ + packet->length > rx_buffer_size_) {
            // リングの終端に来た場合
        }

        // headerの4byte分多めにとり4byteでアラインメント
        rx_offset_ = (rx_offset_ + packet->length + 4 + 3) & ~3;

        // CAPRには、オフセットから0x10引いた値を入れることになっている
        opt_->current_address_of_packet_read = rx_offset_ - 0x10;
        logger->debug("CAPR: %hd, CBA: %hd\n", opt_->current_address_of_packet_read, opt_->current_buffer_address);
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

}