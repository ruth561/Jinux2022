#include "ip.hpp"
#include "../logging.hpp"
int printk(const char *format, ...);
void Halt();
extern logging::Logger *logger;
namespace rtl8139
{
    extern Controller *rtl8139;
}


namespace ip
{

    void Dump(Frame *frame)
    {
        // ダンプ
        printk("# IP\n");
        printk("    Version: %d\n", frame->version);
        printk("    Header Length: %d bytes (%d)\n", frame->header_length * 4, frame->header_length);
        printk("    Type of Service: 0x%02hhx\n", frame->type_of_service);
        printk("    Packet Length: %d\n", ntohs(frame->packet_len));
        printk("    Identification: 0x%04hx\n", ntohs(frame->identification));
        printk("    Flags: 0x%hhx\n", frame->flags);
        printk("    Flagment Offset: 0x%04hx\n", frame->flagment_offset);
        printk("    TTL: %d\n", frame->ttl);
        printk("    Protocol Number: %d\n", frame->protocol);
        printk("    Header Checksum: 0x%04hx\n", ntohs(frame->header_checksum));
        printk("    Src IP Address: %hhu:%hhu:%hhu:%hhu\n", 
            frame->src_ip_addr[0], frame->src_ip_addr[1], frame->src_ip_addr[2], frame->src_ip_addr[3]);
        printk("    Dst IP Address: %hhu:%hhu:%hhu:%hhu\n", 
            frame->dst_ip_addr[0], frame->dst_ip_addr[1], frame->dst_ip_addr[2], frame->dst_ip_addr[3]);
    }

    void HandlePacket(Frame *frame)
    {
        Dump(frame);

        if (frame->protocol == ProtocolType::kICMP) {
            // ICMP
        } else if (frame->protocol == ProtocolType::kTCP) {
            // TCP
        } else if (frame->protocol == ProtocolType::kUDP) {
            // UDP
        } else {
            logger->warning("IP Unknown Type: %04hx\n", frame->protocol);
        }

    }
}