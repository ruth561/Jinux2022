#include "arp.hpp"
#include "../logging.hpp"
int printk(const char *format, ...);
void Halt();
extern logging::Logger *logger;
namespace rtl8139
{
    extern Controller *rtl8139;
}

namespace {
    uint8_t broadcast_mac_address[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    uint8_t unknown_mac_address[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    // IPアドレスを比較し等しいならtrueを返す
    bool CompareIPAddress(uint8_t *addr1, uint8_t *addr2)
    {
        return addr1[0] == addr2[0] & 
               addr1[1] == addr2[1] & 
               addr1[2] == addr2[2] & 
               addr1[3] == addr2[3];
    }

}

namespace arp
{
    void ARPDump(ARPFrame *frame)
    {
        // ダンプ
        printk("# ARP\n");
        printk("    Hardware Type: %04hx\n", ntohs(frame->hw_type));
        printk("    Protocol Type: %04hx\n", ntohs(frame->proto_type));
        printk("    Hardware Size: %hhd\n", frame->hw_size);
        printk("    Protocol Size: %hhd\n", frame->proto_size);
        printk("    Opcode: %04hx\n", ntohs(frame->opcode));
        printk("    Src MAC Address: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
            frame->src_mac_addr[0], frame->src_mac_addr[1], frame->src_mac_addr[2], 
            frame->src_mac_addr[3], frame->src_mac_addr[4], frame->src_mac_addr[5]);
        printk("    Src IP Address: %hhu:%hhu:%hhu:%hhu\n", 
            frame->src_ip_addr[0], frame->src_ip_addr[1], frame->src_ip_addr[2], frame->src_ip_addr[3]);
        printk("    Dst MAC Address: %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n",
            frame->dst_mac_addr[0], frame->dst_mac_addr[1], frame->dst_mac_addr[2], 
            frame->dst_mac_addr[3], frame->dst_mac_addr[4], frame->dst_mac_addr[5]);
        printk("    Dst IP Address: %hhu:%hhu:%hhu:%hhu\n", 
            frame->dst_ip_addr[0], frame->dst_ip_addr[1], frame->dst_ip_addr[2], frame->dst_ip_addr[3]);
    }

    void HandlePacket(ARPFrame *frame)
    {
        ARPDump(frame);

        if (ntohs(frame->opcode) == Opcode::kRequest) {
            // TODO: ARPリクエストの処理
            SendReply(frame);
        } else if (ntohs(frame->opcode) == Opcode::kReply) {
            // ARPリプライ
            // 特に何もしない
        } else {
            logger->error("ARP Opcode %04hx Is Invalid.\n", ntohs(frame->opcode));
            return;
        }
    }

    void SendPacket(ARPFrame *frame)
    {
        uint8_t *src_mac_address = rtl8139::rtl8139->MACAddress();

        ethernet::SendPacket(broadcast_mac_address, src_mac_address, ethernet::EthernetType::kARP, frame, sizeof(ARPFrame));
    }

    void SendRequest(uint8_t *dst_ip_addr)
    {
        ARPFrame frame;
        frame.hw_type = htons(HardwareType::kEthernet);
        frame.proto_type = htons(ProtocolType::kIPv4);
        frame.hw_size = 6;
        frame.proto_size = 4;
        frame.opcode = htons(Opcode::kRequest);
        
        memcpy(frame.src_mac_addr, rtl8139::rtl8139->MACAddress(), 6);
        frame.src_ip_addr[0] = 10;
        frame.src_ip_addr[1] = 0;
        frame.src_ip_addr[2] = 2;
        frame.src_ip_addr[3] = 14;

        memcpy(frame.dst_mac_addr, unknown_mac_address, 6);
        memcpy(frame.dst_ip_addr, dst_ip_addr, 4);

        ethernet::SendPacket(broadcast_mac_address, rtl8139::rtl8139->MACAddress(), ethernet::EthernetType::kARP, 
                             &frame, sizeof(ARPFrame));
    }

    void SendReply(ARPFrame *request)
    {
        // 色々省略している、、
        if (!CompareIPAddress(request->dst_ip_addr, rtl8139::rtl8139->IPAddress())) {
            // 宛先IPアドレスが自分自身と違う場合
            return;
        }

        ARPFrame frame;
        frame.hw_type = htons(HardwareType::kEthernet);
        frame.proto_type = htons(ProtocolType::kIPv4);
        frame.hw_size = 6;
        frame.proto_size = 4;
        frame.opcode = htons(Opcode::kReply);
        
        memcpy(frame.src_mac_addr, rtl8139::rtl8139->MACAddress(), 6);
        memcpy(frame.src_ip_addr, request->dst_ip_addr, 4);

        memcpy(frame.dst_mac_addr, request->src_mac_addr, 6);
        memcpy(frame.dst_ip_addr, request->src_ip_addr, 4);

        ethernet::SendPacket(request->src_mac_addr, rtl8139::rtl8139->MACAddress(), ethernet::EthernetType::kARP, 
                             &frame, sizeof(ARPFrame));
    }
}
