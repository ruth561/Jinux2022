#include "arp.hpp"
#include "../logging.hpp"
int printk(const char *format, ...);
void Halt();
extern logging::Logger *logger;

namespace arp
{
    void HandlePacket(ARPFrame *frame)
    {
        // ダンプ
        logger->debug("#ARP\n");
        logger->debug("    Hardware Type: %04hx\n", ntohs(frame->hw_type));
        logger->debug("    Protocol Type: %04hx\n", ntohs(frame->proto_type));
        logger->debug("    Hardware Size: %hhd\n", frame->hw_size);
        logger->debug("    Protocol Size: %hhd\n", frame->proto_size);
        logger->debug("    Opcode: %04hx\n", ntohs(frame->opcode));
        logger->debug("    Src MAC Address: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
            frame->src_mac_addr[0], frame->src_mac_addr[1], frame->src_mac_addr[2], 
            frame->src_mac_addr[3], frame->src_mac_addr[4], frame->src_mac_addr[5]);
        logger->debug("    Src IP Address: %hhu:%hhu:%hhu:%hhu\n", 
            frame->src_ip_addr[0], frame->src_ip_addr[1], frame->src_ip_addr[2], frame->src_ip_addr[3]);
        logger->debug("    Dst MAC Address: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
            frame->dst_mac_addr[0], frame->dst_mac_addr[1], frame->dst_mac_addr[2], 
            frame->dst_mac_addr[3], frame->dst_mac_addr[4], frame->dst_mac_addr[5]);
        logger->debug("    Dst IP Address: %hhu:%hhu:%hhu:%hhu\n", 
            frame->dst_ip_addr[0], frame->dst_ip_addr[1], frame->dst_ip_addr[2], frame->dst_ip_addr[3]);

        if (ntohs(frame->opcode) == Opcode::kRequest) {
            // TODO: ARPリクエストの処理

        } else if (ntohs(frame->opcode) == Opcode::kReply) {
            // ARPリプライ
            // 特に何もしない
        } else {
            logger->error("ARP Opcode %04hx Is Invalid.\n", ntohs(frame->opcode));
            return;
        }
    }
}
