#include "network_lib.hpp"
int printk(const char *format, ...);
void Halt();
extern logging::Logger *logger;

void HexDump(void *buf, int len)
{
    uint8_t *data = reinterpret_cast<uint8_t *>(buf);
    printk("[HexDump]");
    for (int i = 0; i < len; i++) {
        if (i % 16 == 0) {
            printk("\n%04hx: ", i);
        }
        printk("%02hhx ", data[i]);
    }
    printk("\n");
}

// host to network short
uint16_t htons(uint16_t host_short)
{
    uint8_t *src = reinterpret_cast<uint8_t *>(&host_short);
    uint8_t dst[2];
    dst[0] = src[1];
    dst[1] = src[0];
    return *reinterpret_cast<uint16_t *>(dst);
}

// network to host short
uint16_t ntohs(uint16_t network_short)
{
    return htons(network_short);
}

// host to network long
uint32_t htonl(uint32_t host_long)
{
    uint8_t *src = reinterpret_cast<uint8_t *>(&host_long);
    uint8_t dst[4];
    dst[0] = src[3];
    dst[1] = src[2];
    dst[2] = src[1];
    dst[3] = src[0];
    return *reinterpret_cast<uint32_t *>(dst);
}

// network to host long
uint32_t ntohl(uint32_t network_long)
{
    return htonl(network_long);
}
