#include "paging.hpp"
#include "logging.hpp"
#include "asmfunc.h"
#include "memory_manager.hpp"
#include "task.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

extern BitmapMemoryManager* memory_manager;
extern logging::Logger *logger;

namespace
{
    // const uint64_t kPageSize4K = 4096;
    const uint64_t kPageSize2M = 2 * 1024 * 1024;
    const uint64_t kPageSize1G = 1024 * 1024 * 1024;

    // ページングの最大の大きさをGBで指定する。
    const size_t kPageDirectoryCount = 260;

    alignas(4096) std::array<uint64_t, 512> pml4_table;
    alignas(4096) std::array<uint64_t, 512> pdp_table;
    alignas(4096) std::array<std::array<uint64_t, 512>, kPageDirectoryCount> page_directory;

}


void SetupIdentityPageTable()
{
    /* コメントアウトしているのは設定した値をコンソールに出力するというもの。 */
    logger->info("[+] Setup Paging Structure\n");

    pml4_table[0] = reinterpret_cast<uint64_t>(&pdp_table[0]) | 0x003;
    // logger->debug("pml4_table[0] = %lx\n", pml4_table[0]);
    for (int i = 0; i < kPageDirectoryCount; i++) {
        pdp_table[i] = reinterpret_cast<uint64_t>(&page_directory[i][0]) | 0x003;
        // logger->debug("pdp_table[%d] = %lx\n", i, pdp_table[i]);
        for (int j = 0; j < 512; j++) {
            page_directory[i][j] = i * kPageSize1G + j * kPageSize2M | 0x083;
            // logger->debug("    page_directory[%d][%d] = %lx\n", i, j, page_directory[i][j]);
        }
    }

    logger->debug("before set %p to cr3\n", &pml4_table[0]);
    SetCR3(reinterpret_cast<uint64_t>(&pml4_table[0]));
    logger->debug("after set cr3\n");

    // logger->info("[+] Identity paging structure mapped!!\n");
}

int SetIDMapEntry(LinearAddress4Level linear_address)
{
    PageMapEntry *pml4 = reinterpret_cast<PageMapEntry *>(GetCR3());
    PageMapEntry *entry = &pml4[linear_address.bits.pml4];
    if (!entry->bits.present) { // エントリが指すページマップ構造体が存在しない場合
        FrameID frame = memory_manager->Allocate(1);
        if (frame.ID() == kNullFrame.ID())
            return 0;
        memset(frame.Frame(), 0, sizeof(kBytesPerFrame));
        entry->SetPointer(frame.Frame());
        entry->bits.present = 1;
        entry->bits.writable = 1;
    }

    PageMapEntry *pdpt = reinterpret_cast<PageMapEntry *>(entry->Pointer());
    entry = &pdpt[linear_address.bits.pdpt];
    if (!entry->bits.present) { // エントリが指すページマップ構造体が存在しない場合
        FrameID frame = memory_manager->Allocate(1);
        if (frame.ID() == kNullFrame.ID())
            return 0;
        memset(frame.Frame(), 0, sizeof(kBytesPerFrame));
        entry->SetPointer(frame.Frame());
        entry->bits.present = 1;
        entry->bits.writable = 1;
    }

    PageMapEntry *directory = reinterpret_cast<PageMapEntry *>(entry->Pointer());
    entry = &directory[linear_address.bits.directory];
    if (!entry->bits.present) { // エントリが指すページマップ構造体が存在しない場合
        FrameID frame = memory_manager->Allocate(1);
        if (frame.ID() == kNullFrame.ID())
            return 0;
        memset(frame.Frame(), 0, sizeof(kBytesPerFrame));
        entry->SetPointer(frame.Frame());
        entry->bits.present = 1;
        entry->bits.writable = 1;
    }

    PageMapEntry *table = reinterpret_cast<PageMapEntry *>(entry->Pointer());
    entry = &table[linear_address.bits.table];
    if (!entry->bits.present) { // エントリが指すページマップ構造体が存在しない場合
        FrameID frame = memory_manager->Allocate(1);
        if (frame.ID() == kNullFrame.ID())
            return 0;
        memset(frame.Frame(), 0, sizeof(kBytesPerFrame));
        entry->SetPointer(frame.Frame());
        entry->bits.present = 1;
        entry->bits.writable = 1; // 書き込み可能か
        entry->bits.page_size = 1; // このポインターが指す領域がページである
    }

    // logger->debug("Successed set page map at 0x%lx\n", linear_address.data);
    return 1;
}


uintptr_t Translate4LevelPaging(uintptr_t linear_address)
{
    LinearAddress4Level linear;
    linear.data = linear_address;
    // リニアアドレス構造体がきちんと各フィールドを表せているかの確認。
    /* logger->debug("linear address %p, pml4 %lx, pdpt %lx, directory %lx, table %lx, offset %lx\n", 
        linear.data, 
        linear.bits.pml4,
        linear.bits.pdpt, 
        linear.bits.directory, 
        linear.bits.table, 
        linear.bits.offset); */
    
    // CR3レジスタからPML4の先頭アドレスを取得する。
    uint64_t cr3 = GetCR3();
    PageMapEntry *pml4 = reinterpret_cast<PageMapEntry *>(cr3 & ~static_cast<uint64_t>(0xfff));

    // PML4のエントリがページを指し示すことはないので、
    // すぐにPDPTの先頭アドレスを計算。
    PageMapEntry *pdpt = reinterpret_cast<PageMapEntry *>(pml4[linear.bits.pml4].Pointer());
    if (pdpt[linear.bits.pdpt].isPage()) {  // 1GiBのページの時（Huge Page）
        logger->debug("1GiB page!\n");
        return reinterpret_cast<uintptr_t>(pdpt[linear.bits.pdpt].Pointer()) + linear.Offset1GiB();
    }

    // PDPTエントリから指されるページディレクトリ
    // Page Directory
    PageMapEntry *pd = reinterpret_cast<PageMapEntry *>(pdpt[linear.bits.pdpt].Pointer());
    if (pd[linear.bits.directory].isPage()) { // 2MiBのページの時
        logger->debug("2MiB page!\n");
        return reinterpret_cast<uintptr_t>(pd[linear.bits.directory].Pointer()) + linear.Offset2MiB();
    }

    // Page Table
    PageMapEntry *pt = reinterpret_cast<PageMapEntry *>(pd[linear.bits.directory].Pointer());
    if (pt[linear.bits.table].isPage()) { // 4KiBのページの時
        logger->debug("4KiB page!\n");
        return reinterpret_cast<uintptr_t>(pt[linear.bits.table].Pointer()) + linear.Offset4KiB();
    }

    return 0;
}
