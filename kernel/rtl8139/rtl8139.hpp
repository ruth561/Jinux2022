#pragma once 

#include <cstdlib>
#include <cstring>

#include "../pci.hpp"
#include "../logging.hpp"
#include "registers.hpp"
#include "packet.hpp"

namespace rtl8139
{
    class Controller 
    {
    public:
        Controller(uint32_t mmio_base);

        int Initialize();

        void ReceivePacket();

    private:
        const uint32_t mmio_base_;

        OperationalRegister *opt_;
        
        uint64_t rx_buffer_; // Rx Bufferの先頭アドレス
        size_t rx_buffer_size_; // Rx Bufferの大きさ

        uint16_t rx_offset_; // Rx Ringの次の読み込み位置

        std::vector<uintptr_t> packets_;
    };

    void Initialize();
}
