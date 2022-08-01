#pragma once

#include <vector>
#include "asmfunc.h"
#include "logging.hpp"


namespace pci
{
    // IOアドレス空間のアドレス
    const uint16_t kConfigAddress = 0xcf8;
    const uint16_t kConfigData = 0xcfc;
    
    // 各デバイスが持つケイパビリティリスト
    struct CapabilityRegister
    {
        uint8_t offset;
        uint8_t id;
    };
    
    // PCIデバイスを識別する構造体
    struct Device
    {
        uint8_t bus;
        uint8_t device;
        uint8_t function;
        uint8_t base_class;
        uint8_t sub_class;
        uint8_t interface;
        uint8_t header_type;

        std::vector<CapabilityRegister> capability_list;
    };


    // 指定したPCIコンフィギュレーション空間に32bitのdataを書き込む
    // offsetは４の倍数
    void ConfigWrite32(uint32_t data, uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

    // 指定したPCIコンフィギュレーション空間から32bit分読み出す
    // offsetは４の倍数
    uint32_t ConfigRead32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);


    union MSIMessageControl
    {
        uint16_t data;
        struct {
            uint8_t enable: 1;
            uint8_t multiple_message_capable: 3; // RO
            uint8_t multiple_message_enable: 3;
            uint8_t address_64_bit: 1; // RO
            uint8_t per_vector_masking_capable: 1; // RO
            uint8_t : 7;
        } bits;
    };


    


    

}

// PCIバスに接続されるデバイスを列挙し出力する関数
void ScanPCIBus();

void InitializePCI();
