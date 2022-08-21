#pragma once 

#include <cstdint>

#include "logging.hpp"

namespace ioapic
{
    
    struct RedirectionTableEntry
    {
        uint32_t data[2];
        struct {
            uint8_t interrupt_vector;
            uint64_t delivery_mode: 3;
            uint64_t destination_mode: 1;
            uint64_t delivery_status: 1;
            uint64_t interrupt_input_pin_polarity: 1;
            uint64_t remote_irr: 1;
            uint64_t trigger_mode: 1;

            uint64_t interrupt_mask: 1;
            uint64_t : 39;
            uint8_t destination_field;
        } __attribute__((packed)) bits;
    } __attribute__((packed));


    void Initialize();
}

