#pragma once

#include "../memory_manager.hpp"
extern BitmapMemoryManager* memory_manager;


namespace usb
{
    //  動的メモリのためのメモリプールの大きさ
    static const size_t kMemoryPoolSize = 128 * 0x1000;

    // sizeで指定した大きさのメモリ領域を動的に割り当てる。
    // alignmentは０なら無視
    // boundaryは０なら無視
    // 確保できればそのポインタを、できなければnullptrを返す
    void *AllocMem(size_t size, uint64_t alignment, uint64_t boundary);

    // AllocMemで割り当てたメモリ領域の解放
    // 現状実装はない。
    int FreeMem(void *addr);
}
