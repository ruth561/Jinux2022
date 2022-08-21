#pragma once 

#include <cstdlib>

#include "../pci.hpp"
#include "../logging.hpp"
#include "registers.hpp"

namespace rtl8139
{
    class Controller 
    {
    public:
        Controller(uint32_t mmio_base);

        int Initialize();

    private:
        const uint32_t mmio_base_;

        OperationalRegister *opt_;
        void *rx_buffer_;
    };

    void Initialize();
}
