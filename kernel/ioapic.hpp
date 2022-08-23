#pragma once 

#include <cstdint>

#include "logging.hpp"
#include "interrupt.hpp"

namespace ioapic
{
    
    union RedirectionTableEntry
    {
        volatile uint32_t data[2];
        struct {
            volatile uint64_t interrupt_vector: 8;
            volatile uint64_t delivery_mode: 3;
            volatile uint64_t destination_mode: 1;
            volatile uint64_t delivery_status: 1;
            volatile uint64_t interrupt_input_pin_polarity: 1;
            volatile uint64_t remote_irr: 1;
            volatile uint64_t trigger_mode: 1;

            volatile uint64_t interrupt_mask: 1;
            uint64_t : 39;
            volatile uint8_t destination_field;
        } __attribute__((packed)) bits;
    } __attribute__((packed));


    void Initialize();
}

