
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
    Controller::Controller(uint32_t mmio_base) : 
        mmio_base_{mmio_base}, 
        opt_{reinterpret_cast<OperationalRegister *>(mmio_base_)} {}

    int Controller::Initialize()
    {
        logger->info("MAC Address: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n", 
            opt_->mac_address[0], opt_->mac_address[1], opt_->mac_address[2], 
            opt_->mac_address[3], opt_->mac_address[4], opt_->mac_address[5]);
        memcpy(mac_addr_, opt_->mac_address, 6);
        
        // 電源を入れる
        opt_->config1.data = 0;

        // リセット
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

        // Rx Configの設定
        logger->debug("Receive Configuration Register: %08x\n", opt_->receive_configuration_register.data);
        opt_->receive_configuration_register.bits.wrap = 0; // 通常のリングバッファ（オーバーフローさせない）
        opt_->receive_configuration_register.bits.rx_buffer_length = 0; // リングの大きさは8192+16bytes
        opt_->receive_configuration_register.bits.multiple_early_interrupt_select = 1; // パケットを受け取るたびに割り込みを発生させる
        logger->debug("Receive Configuration Register: %08x\n", opt_->receive_configuration_register.data);

        //Tx Bufferの確保
        size_t max_packet_size_ = 0x700;
        for (int i = 0; i < 4; i++) {
            void *tx_buffer = malloc(max_packet_size_);
            if (reinterpret_cast<uint64_t>(tx_buffer) > 0xfffffffful) { // 32bitメモリにマップできなければ
                logger->error("Tx Buffer Cannot Map 32bit Memory.\n");
                Halt();
            }
            tx_buffers_[i] = tx_buffer;
            opt_->transmit_start_address_of_descriptor[i] = static_cast<uint32_t>(reinterpret_cast<uint64_t>(tx_buffer)); // ここで設定する必要ない？
            // logger->debug("Tx Buffer %d: 0x%x\n", i, opt_->transmit_start_address_of_descriptor[i]);
            // logger->debug("Tx Status %d: 0x%08x\n", i, opt_->transmit_status_of_descriptor[i]);
        }
        current_tx_buffer_ = 0;

        // 全ての割り込みを許可する
        logger->debug("Interrupt Mask Register: 0x%04hx\n", opt_->interrupt_mask_register.data);
        opt_->interrupt_mask_register.bits.receive_ok_interupt = true;
        // opt_->interrupt_mask_register.bits.receive_error_interrupt = true;
        opt_->interrupt_mask_register.bits.transmit_ok_interupt = true;
        /* opt_->interrupt_mask_register.bits.transmit_error_interupt = true;
        opt_->interrupt_mask_register.bits.rx_buffer_overflow_interrupt = true;
        opt_->interrupt_mask_register.bits.rx_fifo_overflow_interrupt = true;
        opt_->interrupt_mask_register.bits.packet_underrun_link_change_interupt = true;
        opt_->interrupt_mask_register.bits.cable_length_change_interupt = true;
        opt_->interrupt_mask_register.bits.timeout_interupt = true;
        opt_->interrupt_mask_register.bits.system_error_interupt = true; */
        logger->debug("Interrupt Mask Register: 0x%04hx\n", opt_->interrupt_mask_register.data);

        // パケットの受け取りを開始する
        opt_->command_register.bits.receiver_enable = 1;

        // パケットの送信を有効にする
        opt_->command_register.bits.transmitter_enable = 1;
        return 0;
    }

    uint8_t *Controller::MACAddress()
    { 
        return mac_addr_; 
    }

    void *Controller::ReceivePacket()
    {
        if (opt_->command_register.bits.buffer_empty) {
            // Rx Bufferに何も入っていない場合
            return nullptr;
        }

        Packet *packet = reinterpret_cast<Packet *>(rx_buffer_ + rx_offset_);
        if (packet->HasError()) {
            logger->error("Packet Error\n");
            logger->error("Header: %04hx\n", packet->header.data);
            Halt();
            return nullptr;
        }
        if (!packet->header.bits.receive_ok) {
            logger->error("Packet Is Not OK..\n");
            Halt();
            return nullptr;
        }

        // パケットデータをコピーする領域を確保する
        uint16_t pkt_size = packet->length - 4; // データ部分の大きさ
        Packet *p =  reinterpret_cast<Packet *>(malloc(pkt_size)); // returnする変数
        if (rx_offset_ + packet->length > rx_buffer_size_) { // リングの終端に来た場合
            printk("!!!Rx Buffer Reached To End!!!\n"); // 後ほど境界の動作が成功しているかどうかのデバッグに用いる
            int semi_count = rx_buffer_size_ - rx_offset_ - 4; // packetの先頭からバッファの最後までのバイト数
            memcpy(p, packet->data, semi_count);
            memcpy(reinterpret_cast<char *>(p) + semi_count, 
                   reinterpret_cast<void *>(rx_buffer_), 
                   pkt_size - semi_count);
        } else {
            memcpy(p, packet->data, pkt_size);
        }

        // headerの4byte分多めにとり4byteでアラインメント
        // 4byte分多めにとっているのは、パケットの後ろに4byteのデータが付属しているから？
        // CRCなるものが付属しているらしい。後で調べてみよう、、。
        rx_offset_ = (rx_offset_ + packet->length + 4 + 3) & ~3;
        rx_offset_ %= rx_buffer_size_; // リングの最後尾に行ったら先頭に戻ってくるようにする

        // CAPRには、オフセットから0x10引いた値を入れることになっている
        opt_->current_address_of_packet_read = rx_offset_ - 0x10;
        // logger->debug("CAPR: %hd, CBA: %hd\n", opt_->current_address_of_packet_read, opt_->current_buffer_address);
        return p;
    }

    void Controller::SendPacket(void *packet, uint16_t len)
    {
        if (len > max_packet_size_) {
            logger->error("Too Large Packet! (len = %d)\n", len);
            Halt();
        }

        // Tx Bufferにpacketデータをコピー
        memcpy(tx_buffers_[current_tx_buffer_], packet, len);

        // 先頭アドレスの更新（必要ある？）
        opt_->transmit_start_address_of_descriptor[current_tx_buffer_] = static_cast<uint32_t>(
            reinterpret_cast<uint64_t>(tx_buffers_[current_tx_buffer_]));

        // Transmit Statusの設定
        TxStatusRegister *tx_status = &opt_->transmit_status_of_descriptor[current_tx_buffer_];
        tx_status->bits.descriptor_size = len;
        // tx_status->bits.early_tx_threshold = 0x3f; // しきい値の取扱い方わからん
        tx_status->bits.own = 0; // パケットの送信処理を開始させる
        while (!tx_status->bits.own) { 
            printk("."); 
        }

        if (tx_status->bits.transmit_ok) {
            printk("Transmit OK\n");
        } else {
            logger->error("Transmit Not OK. Status Register: %08x\n", *tx_status);
            Halt();
        }
        // logger->debug("Status: %08x\n", *tx_status);
        
        current_tx_buffer_ = (current_tx_buffer_ + 1) % 4; // 0->1->2->3->0->..
    }



    Controller *RTL8139Initialize()
    {
        pci::Device *rtl8139_dev = pci::FindRTL8139();
        if (rtl8139_dev == nullptr) {
            return nullptr;
        }

        uint32_t data = pci::ConfigRead32(rtl8139_dev->bus, rtl8139_dev->device, rtl8139_dev->function, 4);
        // logger->debug("Command: 0x%hx\n", data);
        // logger->debug("Status: 0x%hx\n", data >> 16);
        pci::ConfigWrite32(data | (1 << 1), rtl8139_dev->bus, rtl8139_dev->device, rtl8139_dev->function, 4); // DMAの許可

        data = pci::ConfigRead32(rtl8139_dev->bus, rtl8139_dev->device, rtl8139_dev->function, 0x14);
        // logger->debug("Memory Address: 0x%x\n", data);
        if (data & 1) { // Bit０が１の時
            logger->error("Cannot Access To Memory Mapped Registers.\n");
            return nullptr;
        }
        uint32_t mmio_base = data & ~0xffu;
        
        data = pci::ConfigRead32(rtl8139_dev->bus, rtl8139_dev->device, rtl8139_dev->function, 0x3c);
        uint8_t interrupt_line = static_cast<uint8_t>(data & 0xffu);
        logger->debug("%08x\n", data);
        logger->debug("Interrupt Line: %d\n", interrupt_line);
        // logger->debug("mmio_base: 0x%x\n", mmio_base);

        return new Controller{mmio_base};
    }

}