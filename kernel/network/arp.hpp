#pragma once 
#include <cstdint>

#include "network_lib.hpp"
#include "ethernet.hpp"


namespace arp
{
    enum HardwareType 
    {
        kEthernet = 0x0001
    };

    enum ProtocolType 
    {
        kIPv4 = 0x0800
    };

    enum Opcode 
    {
        kRequest = 1,
        kReply = 2, 
    };

    struct ARPFrame 
    {
        uint16_t hw_type;
        uint16_t proto_type;
        uint8_t hw_size;
        uint8_t proto_size;
        uint16_t opcode;
        uint8_t src_mac_addr[6];
        uint8_t src_ip_addr[4];
        uint8_t dst_mac_addr[6];
        uint8_t dst_ip_addr[4];
    } __attribute__((packed));

    void HandlePacket(ARPFrame *frame);

    void SendPacket(ARPFrame *frame);

    // 指定したIPアドレスへARPリクエストを送信する
    void SendRequest(uint8_t *dst_ip_addr);

}