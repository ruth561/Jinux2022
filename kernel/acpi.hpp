#pragma once 

#include <cstdint>
#include <cstring>

#include "logging.hpp"
#include "asmfunc.h"

namespace acpi
{
    struct RSDP
    {
        char signature[8];
        uint8_t checksum;
        char oem_id[6];
        uint8_t revision;
        uint32_t rsdt_address;
        uint32_t length;
        uint64_t xsdt_address;
        uint8_t extended_checksum;
        char reserved[3];

        bool IsValid() const;
    } __attribute__((packed));

    struct DescriptionHeader {
        char signature[4];
        uint32_t length;
        uint8_t revision;
        uint8_t checksum;
        char oem_id[6];
        char oem_table_id[8];
        uint32_t oem_revision;
        uint32_t creator_id;
        uint32_t creator_revision;

        bool IsValid(const char* expected_signature) const;
    } __attribute__((packed));

    struct XSDT {
        DescriptionHeader header;
        DescriptionHeader *entries[];

        // エントリの数
        size_t Size() const
        {
            return (header.length - sizeof(DescriptionHeader)) / sizeof(uint64_t);
        }
    } __attribute__((packed));

    struct FADT {
        DescriptionHeader header;

        char reserved1[76 - sizeof(header)];
        volatile uint32_t pm_tmr_blk;
        char reserved2[112 - 80];
        volatile uint32_t flags;
        char reserved3[276 - 116];
    } __attribute__((packed));

    // Multiple APIC Description Table
    struct MADT {
        DescriptionHeader header;
        uint32_t lapic_address;
        uint32_t flags;
    } __attribute__((packed));
    
    struct MADTRecord {
        uint8_t type;
        uint8_t len;
        uint8_t data[];
    } __attribute__((packed));

    enum MADTRecordType 
    {
        ProcessorLocalAPIC = 0,
        IOAPIC = 1,
        InterruptSourceOverride = 2
    };

    // MADTを解析して読み出してくれるクラス
    class APICReader {
    public:
        APICReader(MADT *madt) : madt_{madt} {}

        // 中身のデータを解析する
        void Analyze();


    private:
        MADT *madt_;
    };
    


    const int kPMTimerFreq = 3579545;
    extern FADT* fadt;
    extern APICReader *apic_reader;

    // PMタイマーを用いてmsecミリ秒を測る
    void WaitMilliseconds(unsigned long msec);

    void Initialize(const RSDP *rsdp);
}


