#pragma once

#include <cstddef>
#include <cstdint>
#include <array>

#include "memory_map.hpp"
#include "run_application.hpp"

namespace
{
    constexpr unsigned long long operator""_KiB(unsigned long long kib) {
        return kib * 1024;
    }

    constexpr unsigned long long operator""_MiB(unsigned long long mib) {
        return mib * 1024_KiB;
    }

    constexpr unsigned long long operator""_GiB(unsigned long long gib) {
        return gib * 1024_MiB;
    }
}

// 物理メモリフレームの１つの大きさ
static const uint64_t kBytesPerFrame = 4_KiB;


// ページフレームの番号と物理アドレスを管理するクラス。
class FrameID
{
public:
    FrameID(size_t id) : id_{id} {}

    // メンバ関数にconstをつけることで、関数がメンバ変数を変更しないことを明示できる。    
    size_t ID() const 
    { 
        return id_; 
    }
    
    void *Frame() const
    {
        return reinterpret_cast<void *>(id_ * kBytesPerFrame);
    }
private:
    size_t id_;
};

// フレームが確保できなかったときなどに使用されるオブジェクト。
static const FrameID kNullFrame{~static_cast<size_t>(0)};

class BitmapMemoryManager
{
public:
    // このメモリクラスが扱える最大の物理メモリ量（実際の大きさではない）
    static const uint64_t kMaxPhysicalMemoryBytes = 128_GiB;
    // フレームの数は物理メモリの大きさをフレームのバイト数で割ったもの
    static const uint64_t kFrameCount = kMaxPhysicalMemoryBytes / kBytesPerFrame;
    // ビットマップは64bitごとに配列で管理する。配列の１列の大きさをここで決める。
    static const uint64_t kBitsPerMapLine = sizeof(uint64_t) * 8;

    BitmapMemoryManager();

    void *BitMapAddress() {
        return reinterpret_cast<void *>(&alloc_map_);
    }
    // 物理メモリの範囲を指定。
    void SetMemoryRange(FrameID range_begin, FrameID range_end);
    // フレームの開始地点から指定したフレームの数だけallocatedにする。
    void MarkAllocated(FrameID start_frame, size_t num_frames);
    // 要求されたフレーム数の領域を確保して先頭のフレームIDを返す。
    // First Fit方式で探索する。
    // 成功時は先頭のFrameIDオブジェクトを返す。
    // 失敗時はkNullFrameを返す。
    FrameID Allocate(size_t num_frames);

    int Free(FrameID start_frame, size_t num_frames);
private:
    // i番目のフレームは、alloc_map_[i / kBitsPerMapLine]の第i % kBitsPerMapLineビット目に存在。
    std::array<uint64_t, kFrameCount / kBitsPerMapLine> alloc_map_;

    FrameID range_begin_;   // 管理するメモリの開始地点
    FrameID range_end_;     // 管理するメモリの終了地点（最終フレームの次のフレーム）

    bool GetBit(FrameID frame) const;           // ビットマップ上のフレームに立っているビット
    void SetBit(FrameID frame, bool allocated); // フレームをallocated状態にする。

};


// UEFIのメモリマップを駆使して、使用可能領域と不可領域を
// MemoryManagerに反映する。
void InitializeMemoryManager(MemoryMap& memory_map);

// カーネルで使用するためのヒープ領域を確保し初期化する関数。
// 初期化に成功すれば0を返す。
// 初期化に失敗すれば-1を返す。
// 実行成功後、NewLibのmallocが使用可能になる。
int InitializeHeap(BitmapMemoryManager *memory_manager);


/* 
 * Page Faultが起きた時に使う構造体や関数
 * 
 */

// ページフォルト時に渡されるエラーコードの構造体
union PageFaultErrorCode
{
    uint64_t data;
    struct {
        uint32_t caused_by_page_level_protection: 1; // ０：ページが存在していない　１：存在しているが保護されている
        uint32_t w_r: 1; // １：書き込み時の例外　０：読み込み時の例外
        uint32_t u_s: 1; // １：User-modeでのアクセス　０：Suoervisor-modeでのアクセス
        uint32_t rsvd: 1; // １：reserved領域に１が書き込まれていたことによる例外
        uint32_t caused_by_instruction_fetch: 1; // １：フェッチ時の例外
        uint32_t protection_key_violation: 1; 
        uint32_t caused_by_shadowstack_access: 1;
        uint32_t hlat_paging: 1;
        uint32_t : 7;
        uint32_t sgx: 1;
        uint32_t : 16;
        uint32_t : 32;
    } bits;
};

// ページフォルトハンドラから呼び出される関数
// デマンドページングが成功したら０を返す。
// 失敗したら１を返す。
int HandlePageFault(PageFaultErrorCode error_code, uint64_t linear_addr);
