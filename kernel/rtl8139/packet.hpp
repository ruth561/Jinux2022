#pragma once 

#include <cstdint>

namespace rtl8139
{
    // バッファに書き込まれるパケットの先頭に置かれるデータ構造
    union PacketHeader 
    {
        uint16_t data;
        struct {
            uint16_t receive_ok: 1; // 正しいパケットが受け取れたことを示す
            uint16_t frame_alignment_error: 1;
            uint16_t crc_error: 1;
            uint16_t long_packet: 1; // パケット長が4Kbyteを越えている時１
            uint16_t runt_packet_received: 1; // パケット長が64byteよりも小さい時１
            uint16_t invalid_symbol_error: 1;
            uint16_t : 7;
            uint16_t broadcast_address_received: 1;
            uint16_t physical_address_matched: 1; // 宛先アドレスが自身のMACアドレスにマッチしている時
            uint16_t multicast_address_received: 1;
        } __attribute__((packed)) bits;
    } __attribute__((packed));

    struct Packet
    {
        PacketHeader header;
        uint16_t length;
        uint8_t data[];

        bool HasError() {
            return header.bits.frame_alignment_error | 
                   header.bits.crc_error |
                   header.bits.long_packet |
                   header.bits.runt_packet_received |
                   header.bits.invalid_symbol_error;
        }
    };

    void PacketDump(Packet *packet);
    


}
