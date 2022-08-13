#pragma once

#include <cstddef>
#include "asmfunc.h"

// リニアアドレスが取る構造体。
// tableやdirectoryなどは、各ページング構造体のエントリ番号を表す。
union LinearAddress4Level
{
    uint64_t data;
    struct {
        uint64_t offset : 12;
        uint64_t table : 9;
        uint64_t directory : 9;
        uint64_t pdpt : 9;
        uint64_t pml4 : 9;
        uint64_t reserved : 16;
    } bits;

    uint64_t Offset4KiB() const { return bits.offset; }
    uint64_t Offset2MiB() const { return (bits.table << 12) + bits.offset; }
    uint64_t Offset1GiB() const { return (bits.directory << 21) + (bits.table << 12) + bits.offset; }
};

// ページング構造体の各エントリが共通して取りうる構造体
union PageMapEntry
{
    uint64_t data;
    struct {
        uint64_t present : 1;   // PDPTを参照するなら必ず1をセット
        uint64_t writable : 1;  // 書き込み可能か？
        uint64_t user : 1;      // userモードか？
        uint64_t pwt : 1;
        uint64_t pcd : 1;
        uint64_t accessed : 1;  // アドレス変換に一度でも使用されたか？
        uint64_t dirty : 1;     // OSがこの領域に書き込んだか？
        uint64_t page_size : 1; // アドレスがページを指しているか？（0の時はPDテーブルを指す）
        uint64_t global : 1;    // TODO: 理解してない
        uint64_t : 2;
        uint64_t r : 1;         // HLAT pagingのときのみ1をセットする（基本0）
        uint64_t addr : 40;     // 次のテーブルへのアドレスorページへのアドレス
        uint64_t : 11;
        uint64_t execute_disable : 1;   // 実行制御が行われている時に1なら対象のページ内は実行不可領域となる
    } bits;

    bool isPage() const {   // このエントリがページを指し示していればtrueを返す。
        return bits.page_size == 1;
    }
    void *Pointer() const { // アドレスを返す
        return reinterpret_cast<void *>(bits.addr << 12);
    }
    void SetPointer(void *address) {
        bits.addr = reinterpret_cast<uint64_t>(address) >> 12;
    }
};

/* cppで定義したほうが良いのでは？ 
// ページングの最大の大きさをGBで指定する。
const size_t kPageDirectoryCount = 300; */




// 恒等変換のページングを行う。
void SetupIdentityPageTable();

// 指定したメモリ領域をIDマップのエントリに追加する
int SetIDMapEntry(LinearAddress4Level linear_address);

// リニアアドレスから物理アドレスを算出する関数
// 4level paging方式であることを前提にする。
// CR3->pml4->pdpt->pd->ptの順で参照する。
// 内部でページの大きさを出力する（デバッグレベル）。
uintptr_t Translate4LevelPaging(uintptr_t linear);
