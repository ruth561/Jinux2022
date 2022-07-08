#pragma once

#include <stdint.h>

struct MemoryMap {
    unsigned long long map_size;        // メモリマップの大きさ（byte）。
    void *mem_map;                      // メモリマップの先頭アドレス。
    unsigned long long map_key;         // カーネルでは使わない？
    unsigned long long descriptor_size; // ディスクリプタの大きさ（byte）。sizeof(MemoryDescriptor)とは限らない。
    uint32_t descriptor_version;        // ディスクリプタのバージョン。
};

struct MemoryDescriptor {
    uint32_t type;                      // メモリ領域の種別。
    uintptr_t physical_start;           // メモリ領域先頭の物理アドレス。
    uintptr_t virtual_start;            // メモリ領域先頭の仮想アドレス。
    uint64_t number_of_pages;           // メモリ領域の大きさ（4kiBページ単位）。
    uint64_t attribute;                 // ？
};

#ifdef __cplusplus
enum class MemoryType {
    kEfiReservedMemoryType,
    kEfiLoaderCode,
    kEfiLoaderData,
    kEfiBootServicesCode,
    kEfiBootServicesData,
    kEfiRuntimeServicesCode,
    kEfiRuntimeServicesData,
    kEfiConventionalMemory,
    kEfiUnusableMemory,
    kEfiACPIReclaimMemory,
    kEfiACPIMemoryNVS,
    kEfiMemoryMappedIO,
    kEfiMemoryMappedIOPortSpace,
    kEfiPalCode,
    kEfiPersistentMemory,
    kEfiMaxMemoryType
};

// これらのオペレータで、MemoryDescriptor->typeとmemoryTypeの比較が可能。
inline bool operator==(uint32_t lhs, MemoryType rhs) {
    return lhs == static_cast<uint32_t>(rhs);
}

inline bool operator==(MemoryType lhs, uint32_t rhs) {
    return rhs == lhs;
}

// 指定したメモリ領域が「空き領域」であるか判定してくれるプロシージャ。
// この関数がfalseを返す領域は、使用してはならないので注意。
inline bool isAvailable(MemoryType memory_type)
{
    return
        memory_type == MemoryType::kEfiBootServicesCode ||
        memory_type == MemoryType::kEfiBootServicesData ||
        memory_type == MemoryType::kEfiConventionalMemory;
} 

const int kUEFIPageSize = 4096;

#endif