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
        
        uint64_t rx_buffer_;
    };

    void Initialize();
}
