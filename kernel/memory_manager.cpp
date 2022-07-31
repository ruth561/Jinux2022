#include <sys/types.h>

#include "memory_manager.hpp"
#include "logging.hpp"
void Halt();
 
extern logging::Logger *logger;

BitmapMemoryManager::BitmapMemoryManager() : 
    alloc_map_{}, range_begin_{FrameID{0}}, range_end_{FrameID{kFrameCount}} {
}

void BitmapMemoryManager::SetMemoryRange(FrameID range_begin, FrameID range_end)
{
    range_begin_ = range_begin;
    range_end_ = range_end;
}

void BitmapMemoryManager::MarkAllocated(FrameID start_frame, size_t num_frames)
{
    /* logger->debug("[Mark 0x%x frames from %p (ID: %lx)]\n", 
       num_frames, start_frame.Frame(), start_frame.ID()); */

    for (size_t i = 0; i < num_frames; i++) {
        SetBit(FrameID{start_frame.ID() + i}, true);
    }
}


FrameID BitmapMemoryManager::Allocate(size_t num_frames)
{
    size_t start_frame_id = range_begin_.ID();
    while (true) {
        size_t i = 0;
        for (; i < num_frames; i++) {
            if (start_frame_id + i >= range_end_.ID()) {
                return kNullFrame;
            }
            if (GetBit(FrameID{start_frame_id + i})) {
                break;
            }
        }
        // 連続してnum_frames個の空きフレームが見つかった！
        if (i == num_frames) {
            MarkAllocated(FrameID{start_frame_id}, num_frames);
            return FrameID{start_frame_id};
        }
        start_frame_id += i + 1;
    }
}

int BitmapMemoryManager::Free(FrameID start_frame, size_t num_frames)
{
    for (size_t i = 0; i < num_frames; i++) {
        SetBit(FrameID{start_frame.ID() + i}, false);
    }
    return 0;
}

bool BitmapMemoryManager::GetBit(FrameID frame) const
{
    size_t line_id = frame.ID() / kBitsPerMapLine;
    size_t bit_id = frame.ID() % kBitsPerMapLine;

    return ((alloc_map_[line_id] >> bit_id) & 1) != 0;
}

void BitmapMemoryManager::SetBit(FrameID frame, bool allocated)
{
    size_t line_id = frame.ID() / kBitsPerMapLine;
    size_t bit_id = frame.ID() % kBitsPerMapLine;

    if (allocated) {
        alloc_map_[line_id] |= (static_cast<uint64_t>(1) << bit_id);
    } else {
        alloc_map_[line_id] &= ~(static_cast<uint64_t>(1) << bit_id);
    }
}


char memory_manager_buf[sizeof(BitmapMemoryManager)];
BitmapMemoryManager* memory_manager;

void InitializeMemoryManager(MemoryMap& memory_map)
{
    logging::LoggingLevel log_level = logger->current_level();
    logger->set_level(logging::kINFO);
    logger->info("[+] Initialize Memory Manager\n");

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
    // logger->info("Memory allocate map is at %p\n", memory_manager->BitMapAddress());

    if (InitializeHeap(memory_manager)) { // カーネルで使用するmalloc用のヒープ領域の初期化。
        logger->error("Failed to allocate heap memory...\n");
        Halt();
    }
    logger->set_level(log_level);
}

extern "C" caddr_t program_break, program_break_end;

int InitializeHeap(BitmapMemoryManager *memory_manager)
{
    const int kHeapFrames = 64 * 512;   // ヒープ領域に使うカーネルのメモリ領域の大きさ（KiB）
    const FrameID heap_start = memory_manager->Allocate(kHeapFrames); // 連続した空き領域を確保する。

    if (heap_start.ID() == kNullFrame.ID()) {  // 確保に失敗した時
        return -1;
    }

    // sbrk()で使用するグローバル変数の定義。
    // これによりmallocが使えるようになる。
    program_break = reinterpret_cast<caddr_t>(heap_start.Frame());
    program_break_end = program_break + kHeapFrames * kBytesPerFrame;
    logger->debug("Heap memory is mapped from %p to %p\n", program_break, program_break_end);
    
    return 0;
}


int HandlePageFault(PageFaultErrorCode error_code, uint64_t addr)
{
    LinearAddress4Level linear_addr;
    linear_addr.data = addr;

    if (!error_code.bits.caused_by_page_level_protection) { // ページが存在していないなら
        if (SetupPageMapForApp(linear_addr, 1)) {
            return 0;
        }
    }
    return -1;
}
