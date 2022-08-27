#pragma once 

#include <cstdint>
#include "network.hpp"
#include "../logging.hpp"

namespace tcp
{


    struct Frame 
    {
        uint16_t src_port;
        uint16_t dst_port;

        uint32_t seq_num;

        uint32_t ack_num;

        uint8_t : 4;
        uint8_t data_offset: 4; // ✕４の値がTCPヘッダの長さになる。optionsはこの値によっては含まれない可能性がある
        uint8_t control_flag;
        uint16_t window;

        uint16_t checksum;
        uint16_t urgent_ptr;
        
        uint32_t options; // ここの構造は後回し
        uint8_t data[];
    } __attribute__((packed));

    // TCPフレームをダンプする
    void Dump(Frame *frame);

    // TCPパケットを受け取り、対応する
    void HandlePacket(Frame *frame);

}