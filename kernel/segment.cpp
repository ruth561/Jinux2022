#include "segment.hpp"
#include "asmfunc.h"
#include "memory_manager.hpp"
#include "logging.hpp"
extern BitmapMemoryManager* memory_manager;
extern logging::Logger *logger;

namespace {
    std::array<SegmentDescriptor, 7> gdt;
    TaskStateSegment tss;
}


// 引数で指定したとおりにディスクリプタの値を書き込む。
void SetCodeSegment(SegmentDescriptor *desc,
                    DescriptorType type,
                    unsigned int descriptor_privilege_level,
                    uint32_t base,
                    uint32_t limit) {
    desc->data = 0; // 全体を０クリアする。
    
    desc->bits.base_low = base & 0xffffu;
    desc->bits.base_middle = (base >> 16) & 0xffu;
    desc->bits.base_high = (base >> 24) & 0xffu;

    desc->bits.limit_low = limit & 0xffffu;
    desc->bits.limit_high = (limit >> 16) & 0xfu;

    desc->bits.type = type;
    desc->bits.system_segment = 1;
    desc->bits.descriptor_privilege_level = descriptor_privilege_level;
    desc->bits.present = 1;
    desc->bits.available = 0;
    desc->bits.long_mode = 1;   // 64bitモードの設定
    desc->bits.default_operation_size = 0;
    desc->bits.granularity = 1;
}

void SetDataSegment(SegmentDescriptor *desc,
                    DescriptorType type,
                    unsigned int descriptor_privilege_level,
                    uint32_t base,
                    uint32_t limit) {
    SetCodeSegment(desc, type, descriptor_privilege_level, base, limit);
    desc->bits.long_mode = 0;   // データセグメントでは、このフィールドは必ず０にする。
    desc->bits.default_operation_size = 1; // 自動的にこのフィールドは１になる。
}

void SetSystemSegment(SegmentDescriptor *desc,
                      DescriptorType type,
                      unsigned int descriptor_privilege_level,
                      uint32_t base,
                      uint32_t limit) {
    SetCodeSegment(desc, type, descriptor_privilege_level, base, limit);
    desc->bits.system_segment = 0;
    desc->bits.long_mode = 0;
}

void SetupSegments() {
    logger->info("[+] Setup Segments\n");
    logger->debug("gdt: %p\n", &gdt);
    gdt[0].data = 0;
    SetCodeSegment(&gdt[1], DescriptorType::kExecuteRead, 0, 0, 0xfffff);
    SetDataSegment(&gdt[2], DescriptorType::kReadWrite, 0, 0, 0xfffff);
    SetCodeSegment(&gdt[4], DescriptorType::kExecuteRead, 3, 0, 0xfffff); // アプリ用
    SetDataSegment(&gdt[3], DescriptorType::kReadWrite, 3, 0, 0xfffff); // アプリ用
    LoadGDT(sizeof(gdt) - 1, reinterpret_cast<uintptr_t>(&gdt[0]));

    SetDSAll(kKernelDS);
    SetCSSS(kKernelCS, kKernelSS); 
}


void InitializeTSS()
{
    logger->info("[+] Initialize TSS\n");

    const int kRing0Frames = 8;  // ring0のスタック領域のサイズ（フレーム）
    uint64_t ring0_stack_begin = reinterpret_cast<uint64_t>(memory_manager->Allocate(kRing0Frames).Frame());
    uint64_t ring0_stack_end = ring0_stack_begin + kRing0Frames * kBytesPerFrame;
    logger->debug("Ring0 stack: 0x%lx ~ 0x%lx\n", ring0_stack_begin, ring0_stack_end);
    
    const int kISTFrames = 8; 
    // 通常の割り込み時
    uint64_t ist1_begin = reinterpret_cast<uint64_t>(memory_manager->Allocate(kISTFrames).Frame());
    uint64_t ist1_end = ist1_begin + kISTFrames * kBytesPerFrame;
    logger->debug("IST1 : 0x%lx ~ 0x%lx\n", ist1_begin, ist1_end);

    // ＃GP用スタック領域
    uint64_t ist2_begin = reinterpret_cast<uint64_t>(memory_manager->Allocate(kISTFrames).Frame());
    uint64_t ist2_end = ist2_begin + kISTFrames * kBytesPerFrame;
    logger->debug("IST2 : 0x%lx ~ 0x%lx\n", ist2_begin, ist2_end);

    // ＃GP用スタック領域
    uint64_t ist3_begin = reinterpret_cast<uint64_t>(memory_manager->Allocate(kISTFrames).Frame());
    uint64_t ist3_end = ist3_begin + kISTFrames * kBytesPerFrame;
    logger->debug("IST3 : 0x%lx ~ 0x%lx\n", ist3_begin, ist3_end);

    // xHCI用
    uint64_t ist4_begin = reinterpret_cast<uint64_t>(memory_manager->Allocate(kISTFrames).Frame());
    uint64_t ist4_end = ist4_begin + kISTFrames * kBytesPerFrame;
    logger->debug("IST4 : 0x%lx ~ 0x%lx\n", ist4_begin, ist4_end);

    // TSSに値を指定する。
    tss.rsp0 = ring0_stack_end;
    tss.ist1 = ist1_end;
    tss.ist2 = ist2_end;
    tss.ist3 = ist3_end;
    tss.ist4 = ist4_end;
    
    // GDTへの設定
    uint64_t tss_addr = reinterpret_cast<uint64_t>(&tss);
    SetSystemSegment(&gdt[kTSS >> 3], DescriptorType::kTSSAvailable, 0, 
        tss_addr & 0xffffffff, sizeof(tss) - 1);
    gdt[(kTSS >> 3) + 1].data = tss_addr >> 32;

    LoadTR(kTSS);
}
