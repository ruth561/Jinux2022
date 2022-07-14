#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <malloc.h>
#include <string.h>

#include "asmfunc.h"
#include "frame_buffer_config.hpp"
#include "console.hpp"
#include "logging.hpp"
#include "memory_map.hpp"
#include "segment.hpp"
#include "paging.hpp"
#include "memory_manager.hpp"
#include "interrupt.hpp"

// OSを停止させる関数。CPUの動きを止めるので、安全。
void Halt(void);

// printfのような使い方ができる。コンソールオブジェクト上で機能する。
int printk(const char *format, ...);

// 指定したピクセルの座標(x, y)を指定した色で光らせることが可能なオブジェクト。
char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter *pixel_writer;

// 文字列の出力がPutStringにより簡単にできる。
char console_buf[sizeof(Console)];
Console *console;

// 出力をログレベルに分けて管理できるようにするためのもの。
char logger_buf[sizeof(logging::Logger)];
logging::Logger *logger;

char memory_manager_buf[sizeof(BitmapMemoryManager)];
BitmapMemoryManager* memory_manager;

// カーネルが使用するスタック領域の確保。
alignas(16) uint8_t kernel_main_stack[1024 * 1024];





extern "C" void KernelMainNewStack(
    const FrameBufferConfig *frame_buffer_config_ref, 
    const MemoryMap *memory_map_ref
)
{
    // UEFIから間接的に呼ばれるので、引数で与えられたデータを
    // 新しいスタック領域内に保存するためのコード。
    FrameBufferConfig frame_buffer_config{*frame_buffer_config_ref};
    MemoryMap memory_map{*memory_map_ref};
    
    // ピクセルの扱いがRGBなのかBGRなのか判断し、
    // グローバルなPixelWriterオブジェクトpixel_writerを作成する。
    switch (frame_buffer_config.pixel_format) {
        case kPixelRGBResv8BitPerColor:
            pixel_writer = new(pixel_writer_buf) RGBResv8BitPerColorPixelWriter{&frame_buffer_config};
            break;
        case kPixelBGRResv8BitPerColor:
            pixel_writer = new(pixel_writer_buf) BGRResv8BitPerColorPixelWriter{&frame_buffer_config};
            break;
    }

    // コンソールオブジェクトを生成する。
    // 背景色と文字色が指定可能。
    PixelColor fg_color = {0, 0, 0}, bg_color = {0xff, 0xff, 0xff};
    for (int x = 0; x < pixel_writer->h_resol(); x++) {
        for (int y = 0; y < pixel_writer->v_resol(); y++) {
            pixel_writer->Write(x, y, &bg_color);
        }
    }
    console = new(console_buf) Console(pixel_writer, &fg_color, &bg_color);

    // ロガーを生成。ログレベルを設定することで、出力の詳しさを制御できる。
    logger = new(logger_buf) logging::Logger();
    logger->set_level(logging::kDEBUG);
    
    logger->debug("kernel_main_stack: %p\n", kernel_main_stack);
    
    // メモリマップの出力。
    logger->info("memory_map: %p\n", &memory_map);
    logger->debug("memory_map_size: %lx\n", memory_map.map_size);
    logger->debug("buffer_address: %lx\n", memory_map.mem_map);
    logger->debug("descriptor_size: %lx\n", memory_map.descriptor_size);

    for (
        uintptr_t iter = reinterpret_cast<uintptr_t>(memory_map.mem_map);
        iter < reinterpret_cast<uintptr_t>(memory_map.mem_map) + memory_map.map_size;
        iter += memory_map.descriptor_size) {
        MemoryDescriptor *desc = reinterpret_cast<MemoryDescriptor *>(iter);
        logger->debug(
            "type=%2u, phys=%08lx-%08lx, pages=%4lu, attr=%016lx\n",
            desc->type,
            desc->physical_start, 
            desc->physical_start + 4096 * desc->number_of_pages - 1, 
            desc->number_of_pages, 
            desc->attribute
        );
    }

    // セグメンテーションの設定
    SetupSegments();

    const uint16_t kernel_cs = 1 << 3;
    const uint16_t kernel_ss = 2 << 3;
    SetDSAll(0);
    SetCSSS(kernel_cs, kernel_ss);

    uint64_t cr0 = GetCR0();
    uint64_t cr2 = GetCR2();
    uint64_t cr3 = GetCR3();
    uint64_t cr4 = GetCR4();
    logger->debug("CR0: %016lx\n", cr0);
    logger->debug("CR2: %016lx\n", cr2);
    logger->debug("CR3: %016lx\n", cr3);
    logger->debug("CR4: %016lx\n", cr4);

    


    // 恒等ページングの設定
    SetupIdentityPageTable();
    logger->info("Identity paging structure mapped!!\n");

    // ビットマップメモリマネージャーの生成し、
    // UEFIでの使用可能領域と使用不可領域をマップに設定。
    memory_manager = new(memory_manager_buf) BitmapMemoryManager;
    uintptr_t available_end = 0;
    for (
        uintptr_t iter = reinterpret_cast<uintptr_t>(memory_map.mem_map);
        iter < reinterpret_cast<uintptr_t>(memory_map.mem_map) + memory_map.map_size;
        iter += memory_map.descriptor_size) {
        MemoryDescriptor *desc = reinterpret_cast<MemoryDescriptor *>(iter);
        // メモリマップで歯抜けになっている部分は使用できないので、マークする。
        if (available_end < desc->physical_start) {
            memory_manager->MarkAllocated(
                FrameID{available_end / kBytesPerFrame}, 
                (desc->physical_start - available_end) / kBytesPerFrame);
        }
        // descが指す領域の最後のアドレス（含まれない最初）
        uintptr_t physical_end = desc->physical_start + desc->number_of_pages * kUEFIPageSize;
        if (isAvailable(static_cast<MemoryType>(desc->type))) { // 使用していいメモリ領域
            available_end = physical_end;
        } else {
            memory_manager->MarkAllocated(
                FrameID{desc->physical_start / kBytesPerFrame}, 
                desc->number_of_pages * kUEFIPageSize / kBytesPerFrame);
        }
    }
    logger->info("Memory allocate map is at %p\n", memory_manager->BitMapAddress());


    // カーネルで使用するmalloc用のヒープ領域の初期化。
    if (InitializeHeap(memory_manager)) {
        logger->error("Failed to allocate heap memory...\n");
        Halt();
    }

    // mallocの使用が可能に！！！
    char *str = reinterpret_cast<char *>(malloc(0x10));
    strcpy(str, "hello, world!");
    logger->debug("malloc %p -> %s\n", str, str);

    // 割り込み・例外ハンドラの設定
    SetupInterruptDescriptorTable();
    logger->info("Complete setup IDT!\n");

    /* ０除算例外を引き起こすコード
    __asm__("divb 0"); */
    uintptr_t p = 0x6234567890;
    logger->debug("memory access %08x(%p)\n", *reinterpret_cast<int *>(p), p);
    /* for (uint64_t i = 0; i < 64; i++) {
        p = (static_cast<uint64_t>(1) << i);
        logger->debug("memory access %08x(%p)\n", *reinterpret_cast<int *>(p), p);
    } */

    logger->info("execute Halt() ...\n");
    Halt();
}



void Halt(void)
{
    while (1) __asm__("hlt");
}

int printk(const char *format, ...) {
    int res;
    va_list ap;
    char s[1024];

    va_start(ap, format);
    res = vsprintf(s, format, ap);
    va_end(ap);

    console->PutString(s);
    return res;
}

