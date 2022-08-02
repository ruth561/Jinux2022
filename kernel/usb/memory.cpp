#include "memory.hpp"


namespace
{
    //  ptrをalignmentでアラインメントした値を返す。
    //  １６でアラインメントする場合、１５を足した後に
    //  〜０ｘ００００ｆと論理積を取ることで、
    //  ptrより大きな値で、最小のalignmentされた値を返すことができる。
    uint64_t Ceil(uint64_t ptr, uint64_t alignment)
    {
        return (ptr + alignment - 1) & ~(alignment - 1);
    }
}

namespace usb
{
    //  メモリプール
    uint8_t memory_pool[kMemoryPoolSize];
    uint64_t alloc_ptr = (uint64_t) memory_pool;

    void *AllocMem(size_t size, uint64_t alignment, uint64_t boundary)
    {
        if (alignment > 0)
            alloc_ptr = Ceil(alloc_ptr, alignment);
        
        if (boundary > 0) {
            uint64_t next_boundary = Ceil(alloc_ptr, boundary);
            if (next_boundary < alloc_ptr + size) {
                alloc_ptr = next_boundary;
            }
        }

        if ((uint64_t) memory_pool + kMemoryPoolSize < alloc_ptr + size)
            return nullptr;
        
        void *resp = (void *) alloc_ptr;
        alloc_ptr += size;
        return resp;
    }
 
    int FreeMem(void *addr) 
    { 
        return -1;
    }
}