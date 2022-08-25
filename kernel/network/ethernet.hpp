#pragma once 
#include <cstdint>

#include "network_lib.hpp"
#include "arp.hpp"


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

    void HandlePacket(EthernetFrame *frame, uint16_t len);

}
