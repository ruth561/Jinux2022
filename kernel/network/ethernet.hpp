#pragma once 
#include <cstdint>

#include "network_lib.hpp"
#include "arp.hpp"
#include "ip.hpp"

#include "../rtl8139/rtl8139.hpp"


namespace ethernet
{
    enum EthernetType 
    {
        kIPv4 = 0x0800, 
        kARP = 0x0806, 
        kIPv6 = 0x86dd,
    };

    struct EthernetFrame 
    {
        uint8_t dst_mac_address[6];
        uint8_t src_mac_address[6];
        uint16_t type;
        uint8_t payload[];
    } __attribute__((packed));

    // イーサネットフレームをダンプして表示する
    void Dump(EthernetFrame *frame);

    // イーサネットフレームを解析し上位層へ渡す
    void HandlePacket(EthernetFrame *frame, uint16_t len);

    // Ethernetパケットをrtl8139を用いて送信する。
    // protoはProtocolTypeを用いて表現すると良い。
    // payloadは上の層のrawデータである。
    void SendPacket(uint8_t *dst_mac_addr, uint8_t *src_mac_addr, uint16_t proto, 
                    void *payload, int payload_len);

}
