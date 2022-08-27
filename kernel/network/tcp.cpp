#include "tcp.hpp"

int printk(const char *format, ...);
void Halt();
extern logging::Logger *logger;
namespace network
{
    extern NetworkManager *manager;
}





namespace tcp
{
    void Dump(Frame *frame)
    {
        printk("# TCP\n");
        printk("    Src Port: %d\n", ntohs(frame->src_port));
        printk("    Dst Port: %d\n", ntohs(frame->dst_port));
        printk("    Sequence Number: %d\n", ntohl(frame->seq_num));
        printk("    Acknowledgement Number: %d\n", ntohl(frame->ack_num));
        printk("    Header Length: %d bytes (%d)\n", frame->data_offset * 4, frame->data_offset);
        printk("    Flags: 0x%02hhx\n", frame->control_flag);
        printk("    Window Size: %d\n", ntohs(frame->window));
        printk("    Checksum: 0x%04hx\n", ntohs(frame->checksum));
        printk("    Urgent Pointer: %d\n", ntohs(frame->urgent_ptr));

    }

    void HandlePacket(Frame *frame)
    {
        Dump(frame);
    }
}