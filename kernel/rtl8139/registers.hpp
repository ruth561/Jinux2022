#pragma once 

#include <cstdint>




namespace rtl8139
{
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
        uint64_t mac_address;
        uint8_t reserved[0x48]; // 8 ~ 4fh
        uint8_t command_9346; // 50h
        uint8_t config0; // 51h
        CONFIG1 config1; // 52h
    } __attribute__((packed));
}
