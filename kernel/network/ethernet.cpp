#include "ethernet.hpp"
#include "../logging.hpp"
int printk(const char *format, ...);
void Halt();
extern logging::Logger *logger;


namespace ethernet
{

    void HandlePacket(EthernetFrame *packet, uint16_t len)
    {
        logger->debug("Ethernet, Src: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx, Dst: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n", 
            packet->src_mac_address[0], packet->src_mac_address[1], packet->src_mac_address[2], 
            packet->src_mac_address[3], packet->src_mac_address[4], packet->src_mac_address[5], 
            packet->dst_mac_address[0], packet->dst_mac_address[1], packet->dst_mac_address[2],
            packet->dst_mac_address[3], packet->dst_mac_address[4], packet->dst_mac_address[5]);
        
        switch (ntohs(packet->type)) {
            case EthernetType::kARP:
                logger->debug("(ARP Packet)\n");
                break;
            
            case EthernetType::kIPv4:
                logger->debug("(IPv4 Packet)\n");
                break;

            case EthernetType::kIPv6:
                logger->debug("(IPv6 Packet)\n");
                break;

            default:
                logger->warning("Ethernet Unknown Type: %04hx\n", packet->type);
                break;
        }
    }
}