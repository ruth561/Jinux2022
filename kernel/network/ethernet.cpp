#include "ethernet.hpp"
#include "../logging.hpp"
int printk(const char *format, ...);
void Halt();
extern logging::Logger *logger;


namespace ethernet
{

    void HandlePacket(EthernetFrame *frame, uint16_t len)
    {
        logger->debug("Ethernet, Src: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx, Dst: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n", 
            frame->src_mac_address[0], frame->src_mac_address[1], frame->src_mac_address[2], 
            frame->src_mac_address[3], frame->src_mac_address[4], frame->src_mac_address[5], 
            frame->dst_mac_address[0], frame->dst_mac_address[1], frame->dst_mac_address[2],
            frame->dst_mac_address[3], frame->dst_mac_address[4], frame->dst_mac_address[5]);
        
        switch (ntohs(frame->type)) {
            case EthernetType::kARP:
                // logger->debug("(ARP)\n");
                arp::HandlePacket(reinterpret_cast<arp::ARPFrame *>(frame->payload));
                break;
            
            case EthernetType::kIPv4:
                logger->debug("(IPv4)\n");
                break;

            case EthernetType::kIPv6:
                logger->debug("(IPv6)\n");
                break;

            default:
                logger->warning("Ethernet Unknown Type: %04hx\n", frame->type);
                break;
        }
    }
}