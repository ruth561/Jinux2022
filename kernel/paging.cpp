#include "paging.hpp"
#include "logging.hpp"
#include "asmfunc.h"

#include <array>
#include <cstddef>
#include <cstdint>

extern logging::Logger *logger;

namespace
{
    const uint64_t kPageSize4K = 4096;
    const uint64_t kPageSize2M = 2 * 1024 * 1024;
    const uint64_t kPageSize1G = 1024 * 1024 * 1024;

    alignas(4096) std::array<uint64_t, 512> pml4_table;
    alignas(4096) std::array<uint64_t, 512> pdp_table;
    alignas(4096) std::array<std::array<uint64_t, 512>, kPageDirectoryCount> page_directory;

}

void SetupIdentityPageTable()
{
    pml4_table[0] = reinterpret_cast<uint64_t>(&pdp_table[0]) | 0x003;
    logger->debug("pml4_table[0] = %lx\n", pml4_table[0]);

    for (int i = 0; i < kPageDirectoryCount; i++) {
        pdp_table[i] = reinterpret_cast<uint64_t>(&page_directory[i][0]) | 0x003;
        logger->debug("pdp_table[%d] = %lx\n", i, pdp_table[i]);
        for (int j = 0; j < 512; j++) {
            page_directory[i][j] = i * kPageSize1G + j * kPageSize2M | 0x083;
        }
    }

    SetCR3(reinterpret_cast<uint64_t>(&pml4_table[0]));
}

