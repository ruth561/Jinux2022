#pragma once 

#include <cstdlib>
#include <cstring>
#include <array>

#include "../pci.hpp"
#include "../logging.hpp"
#include "registers.hpp"
#include "packet.hpp"
#include "../network/ethernet.hpp"

namespace rtl8139
{
    class Controller 
    {
    public:
        Controller(uint32_t mmio_base);

        int Initialize();

        uint8_t *MACAddress() { return mac_addr_; } // MACアドレスをuint8_t[6]という形式で返す
        uint8_t *IPAddress() { return ip_addr_; } // IPアドレスをuint8_t[4]という形式で返す

        // Rx Bufferからパケットを受け取り処理をする
        void ReceivePacket();

        // 長さlenの生のパケットpacketを送信する
        void SendPacket(void *packet, uint16_t len);

    private:
        const uint32_t mmio_base_;
        OperationalRegister *opt_;

        uint8_t mac_addr_[6];
        uint8_t ip_addr_[4];
        
        uint64_t rx_buffer_; // Rx Bufferの先頭アドレス
        size_t rx_buffer_size_; // Rx Bufferの大きさ

        size_t max_packet_size_ = 0x700; // 転送可能なパケットの大きさの最大
        std::array<void *, 4> tx_buffers_; // それぞれ、TSAに書き込む値
        int current_tx_buffer_; // 次使用するtx_buffers_の番号（0->1->2->3->0->..）

        uint16_t rx_offset_; // Rx Ringの次の読み込み位置

        std::vector<Packet *> packets_;
    };

    void Initialize();
}
