#pragma once 

#include <cstdint>




namespace rtl8139
{
    union CommandRegister
    {
        uint8_t data;
        struct {
            volatile uint8_t buffer_empty: 1;
            uint8_t : 1;
            volatile uint8_t transmitter_enable: 1;
            volatile uint8_t receiver_enable: 1;
            volatile uint8_t reset: 1;
            uint8_t : 3;
        } __attribute__((packed)) bits;
    } __attribute__((packed));

    union CONFIG1
    {
        volatile uint8_t data;
        struct {
            uint8_t power_management_enable: 1;
            uint8_t vital_product_data_enable: 1;
            uint8_t io_mapping: 1;
            uint8_t memory_mapping: 1;
            uint8_t lwake_active_mode: 1;
            uint8_t driver_load: 1;
            uint8_t led: 1;
        } __attribute__((packed)) bits;
    } __attribute__((packed));

    struct OperationalRegister 
    {
        uint64_t mac_address; // 0h
        uint8_t multicast_registers[8]; // 8h~fh
        uint32_t transmit_status_of_descriptor[4]; // 10h~1fh
        uint32_t transmit_start_address_of_descriptor[4]; // 20h~2fh
        uint32_t receive_buffer_start_address; // 30h~33h
        uint16_t early_receive_byte_count; // 34h~35h
        uint8_t early_rx_status_register; // 36h
        CommandRegister command_register; // 37h
        uint16_t current_address_of_packet_read; // 38h~39h
        uint16_t current_buffer_address; // 3ah~3bh
        uint16_t interrupt_mask_register; // 3ch~3dh
        uint16_t interrupt_status_register; // 3eh~3fh
        uint32_t transmit_configuration_register; // 40h~43h
        uint32_t receive_configuration_register; // 44h~47h
        uint32_t timer_count_register; // 48h~4bh
        uint32_t missed_packet_counter; // 4ch~4fh
        uint8_t command_9346; // 50h
        uint8_t config0; // 51h
        CONFIG1 config1; // 52h
    } __attribute__((packed));
}
