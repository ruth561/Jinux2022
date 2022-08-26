#include "ethernet.hpp"
#include "../logging.hpp"
int printk(const char *format, ...);
void Halt();
extern logging::Logger *logger;
namespace network
{
    extern NetworkManager *manager;
}

namespace ethernet
{
    void Dump(EthernetFrame *frame)
    {
        printk("# Ethernet\n");
        printk("    Src MAC Address: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n", 
            frame->src_mac_address[0], frame->src_mac_address[1], frame->src_mac_address[2], 
            frame->src_mac_address[3], frame->src_mac_address[4], frame->src_mac_address[5]);
        printk("    Dst MAC Address: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n", 
            frame->dst_mac_address[0], frame->dst_mac_address[1], frame->dst_mac_address[2],
            frame->dst_mac_address[3], frame->dst_mac_address[4], frame->dst_mac_address[5]);
        printk("    Type: 0x%04hx\n", ntohs(frame->type));
    }

    void HandlePacket(EthernetFrame *frame)
    {
        Dump(frame);
        
        switch (ntohs(frame->type)) {
            case EthernetType::kARP:
                // logger->debug("(ARP)\n");
                arp::HandlePacket(reinterpret_cast<arp::ARPFrame *>(frame->payload));
                break;
            
            case EthernetType::kIPv4:
                // logger->debug("(IPv4)\n");
                ip::HandlePacket(reinterpret_cast<ip::Frame *>(frame->payload));
                break;

            case EthernetType::kIPv6:
                logger->debug("(IPv6)\n");
                break;

            default:
                logger->warning("Ethernet Unknown Type: %04hx\n", frame->type);
                break;
        }
    }

    void SendPacket(uint8_t *dst_mac_addr, uint8_t *src_mac_addr, uint16_t proto, 
                    void *payload, int payload_len)
    {
        int pkt_len = sizeof(EthernetFrame) + payload_len;
        EthernetFrame *pkt_buf = reinterpret_cast<EthernetFrame *>(malloc(pkt_len));
        
        // MAC
        memcpy(pkt_buf->dst_mac_address, dst_mac_addr, 6);
        memcpy(pkt_buf->src_mac_address, src_mac_addr, 6);

        pkt_buf->type = htons(proto);

        memcpy(pkt_buf->payload, payload, payload_len);

        printk("[Send Packet]\n");
        HexDump(pkt_buf, pkt_len);
        network::manager->SendPacket(pkt_buf, pkt_len);

        free(pkt_buf);
    }
}