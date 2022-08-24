#include "packet.hpp"
int printk(const char *format, ...);

namespace rtl8139
{
    void PacketDump(Packet *packet)
    {
        uint16_t packet_size = packet->length - 4;
        printk("[Packet Dump]");
        for (int i = 0; i < packet_size; i++) {
            if (i % 16 == 0) {
                printk("\n%04hx: ", i);
            }
            printk("%02hhx ", packet->data[i]);
        }
        printk("\n");
    }
}