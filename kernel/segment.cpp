#include "segment.hpp"
#include "asmfunc.h"

#include "logging.hpp"
extern logging::Logger *logger;

namespace {
    std::array<SegmentDescriptor, 3> gdt;
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


void SetupSegments() {
    logger->debug("gdt: %p\n", &gdt);
    gdt[0].data = 0;
    SetCodeSegment(&gdt[1], DescriptorType::kExecuteRead, 0, 0, 0xfffff);
    SetDataSegment(&gdt[2], DescriptorType::kReadWrite, 0, 0, 0xfffff);
    LoadGDT(sizeof(gdt) - 1, reinterpret_cast<uintptr_t>(&gdt[0]));

    const uint16_t kernel_cs = 1 << 3;
    const uint16_t kernel_ss = 2 << 3;
    SetDSAll(0);
    SetCSSS(kernel_cs, kernel_ss);
}

