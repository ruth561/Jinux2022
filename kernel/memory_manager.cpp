#include "memory_manager.hpp"
#include "logging.hpp"

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
    logger->debug("Mark 0x%x frames from %p(ID: %lx)\n", 
        num_frames, start_frame.Frame(), start_frame.ID());

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

// ビットマップのframeにallocatedを設定する。
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