#pragma once 

#include <cstdint>

#include "logging.hpp"

namespace acpi
{
    struct RSDP
    {
        char signature[8];
    } __attribute__((packed));


    void Initialize(const RSDP *rsdp);
}


