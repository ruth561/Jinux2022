#pragma once

#include <cstdint>
#include "../rtl8139/rtl8139.hpp"

namespace ip
{
    enum ProtocolType 
    {
        kICMP = 1,
        kTCP = 6,
        kUDP = 17
    };

    // このOSはリトルエンディアンで動作していることを仮定する
    struct Frame 
    {
        uint8_t header_length: 4; // ✕４した値がヘッダの大きさ（bytes）
        uint8_t version: 4; // 4か6か
        uint8_t type_of_service;
        uint16_t packet_len; // IPヘッダとIPペイロードを合わせた長さ(bytes)

        uint16_t identification;
        uint16_t flagment_offset: 13;
        uint16_t flags: 3;

        uint8_t ttl;
        uint8_t protocol;
        uint16_t header_checksum;

        uint8_t src_ip_addr[4];
        uint8_t dst_ip_addr[4];
        uint8_t payload[];
    } __attribute__((packed));

    // IPフレームをダンプする
    void Dump(Frame *frame);

    // IPパケットを受け取り、対応する
    void HandlePacket(Frame *frame);
}