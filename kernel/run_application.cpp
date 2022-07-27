#include "run_application.hpp"

extern BitmapMemoryManager* memory_manager;
extern logging::Logger *logger;
/* 
 * 本来はterminal.hppなどを作りたかったが、時間もないのでとりあえず
 * 手書きアプリを実行できるようにする。
 */


// 現在のPML4テーブルの前半部分を複製した新しいPML4テーブルをメモリに作成する
// その後、CR3を上書きする。
// 成功すれば０を返す。
// 失敗すれば−１を返す。
int SetupPML4()
{
    FrameID frame = memory_manager->Allocate(1); // １フレーム分を割り当てる
    if (frame.ID() == kNullFrame.ID()) {
        return -1;
    }
    memset(frame.Frame(), 0, sizeof(PageMapEntry) * 512);   // PML4の領域を０で初期化

    PageMapEntry *pml4 = reinterpret_cast<PageMapEntry *>(frame.Frame());
    PageMapEntry *current_pml4 = reinterpret_cast<PageMapEntry *>(GetCR3());
    memcpy(pml4, current_pml4, sizeof(PageMapEntry) * 256); // 現在のPML4の前半部分をコピーする
    logger->info("[+] Setup PML4 for application at %p\n", pml4);

    uint64_t new_cr3 = reinterpret_cast<uint64_t>(pml4);
    SetCR3(new_cr3);
    return 0;
}

// 実行したいアプリケーションの機械語（16進数）
// アセンブラで書いたプログラムをnasmで機械語に変換し、
// 作成したバイナリファイルを「xxd -i」で出力してあげたもの。
unsigned char app[] = {
  0xb8, 0x00, 0x00, 0x00, 0x00, 0x49, 0x89, 0xca, 0x48, 0xbf, 0x47, 0x46,
  0x45, 0x44, 0x43, 0x42, 0x41, 0x00, 0x0f, 0x05, 0xb8, 0xef, 0xbe, 0xad,
  0xde, 0xbb, 0xbe, 0xba, 0xfe, 0xca, 0x50, 0x53, 0x5b, 0x58, 0xeb, 0xf0
};

uint64_t app_base_addr = 0xffff800000000000lu;
uint64_t app_rsp = 0xffffc00000000000lu;

// OS用のタスクとして呼び出されることを想定
// 内部で独自のPML4を作成するなどした後、CallAppでアプリケーションを呼び出す。
void RunApplication(uint64_t a, int64_t data)
{
    SetupPML4();
    /* LinearAddress4Level linear_address;
    linear_address.data = app_base_addr;
    SetupPageMapForApp(linear_address, true); */

    memcpy((void *) app_base_addr, &app[0], sizeof(app));

 
    CallApp(0, reinterpret_cast<char **>(data), kUserCS | 3, kUserSS | 3, app_base_addr, app_rsp); 
    while (1);
} 


int SetupPageMapForApp(LinearAddress4Level linear_address, bool writable)
{
    if (linear_address.data < 0xffff'8000'0000'0000) {
        return 0;
    }
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
        entry->bits.user = 1;
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
        entry->bits.user = 1;
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
        entry->bits.user = 1;
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
        entry->bits.writable = writable; // 書き込み可能か
        entry->bits.user = 1; // 設定しているのはユーザー権限のページのみ
        entry->bits.page_size = 1; // このポインターが指す領域がページである
    }

    logger->debug("Successed set page map at 0x%lx\n", linear_address.data);
    return 1;
}
