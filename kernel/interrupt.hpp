#pragma once


#include <cstdint>
#include <cstddef>
#include <array>

#include "asmfunc.h"


// 割り込みベクタ番号の列挙体
enum InterruptVector
{
    kDivideError = 0,
    kInvalidOpecode = 6, 
    kSegmentNotPresent = 11,
    kGeneralProtection = 13, 
    kPageFault = 14,
    kLAPICTimer = 0x41,
};

// 各割り込みが用いるISTの値を定義
const int kISTForTimer = 1; 
const int kISTForGP = 2;

// IDTのエントリ
struct InterruptDescriptor 
{
    uint16_t offset_low : 16;
    uint16_t segment_selector : 16; // このハンドラを実行するときのセレクタ値（csの値でいい？）
    uint16_t interrupt_stack_table : 3;
    uint16_t : 5;
    uint16_t type : 4; // 14:割り込みゲート、15:トラップゲート
    uint16_t : 1;
    uint16_t descriptor_priviledge_level : 2;
    uint16_t segment_present_flag : 1;
    uint16_t offset_middle : 16;
    uint32_t offset_high : 32;
    uint32_t : 32;

    uintptr_t GetOffset() const {
        return (static_cast<uint64_t>(offset_high) << 32) +\
               (static_cast<uint64_t>(offset_middle) << 16) +\
                static_cast<uint64_t>(offset_low);
    }

    void SetOffset(uintptr_t offset) {
        offset_low = offset & 0xfffful;
        offset_middle = (offset >> 16) & 0xfffful;
        offset_high = (offset >> 32) & 0xfffffffful;
    }
} __attribute__((packed));


// 割り込みハンドラ内でrspが指す構造体
// （割り込みハンドラの第一引数に渡されるポインタが指す構造体）
struct InterruptFrame
{
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

// vectorで指定したエントリの設定をする。
// ＝＝＝＝＝＝＝＝　引数の説明　＝＝＝＝＝＝＝＝
// offset：　手続きのアドレス
// segment_selector：　この手続きを実行するセグメントのセレクタ値（csの値を書き込めば良い？）
// type：　14（ハードウェア割り込み）or 15（CPU命令時割り込み）
// interrupt_stack_table：　あまり理解していない（とりあえず０で）
// descriptor_priviledge_level：　このゲートの特権レベル
// segment_present_flag：　ディスクリプタを有効化するか？（基本１）
void SetIDTEntry(InterruptVector vector_number, 
                 uintptr_t offset, 
                 uint16_t segment_selector, 
                 uint16_t type, 
                 uint16_t interrupt_stack_table = 0, 
                 uint16_t descriptor_priviledge_level = 0, 
                 uint16_t segment_present_flag = 1);


// 割り込みが起きたことを通知する関数。
// 割り込み処理の最後に必ず必要。
void NotifyEndOfInterrupt();


// idtへの設定をここで行う。
void SetupInterruptDescriptorTable();
