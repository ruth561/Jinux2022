#pragma once

#include <stdint.h>

extern "C" {
    // 指定したIOアドレス空間のアドレスにvalueを書き込む
    void WriteIOAddressSpace32(uint16_t address, uint32_t value);
    // 指定したIOアドレス空間のアドレスから４bytes分読み出す
    uint32_t ReadIOAddressSpace32(uint16_t address);

    uint16_t GetCS(void);
    void LoadIDT(uint16_t limit, uint64_t offset);
    void LoadGDT(uint16_t limit, uint64_t offset);
    void SetCSSS(uint16_t cs, uint16_t ss);
    void SetDSAll(uint16_t value);
    void SetCR3(uint64_t value);
    void LoadTR(uint16_t sel);
    uint64_t GetCR0();
    uint64_t GetCR2();
    uint64_t GetCR3();
    uint64_t GetCR4();

    void SwitchContext(void* next_ctx, void* current_ctx);
    // 現在のコンテキストは保存せずに、指定したタスクのコンテキストの復帰のみ行う関数。
    void RestoreContext(void* task_context);
    // LAPICタイマーの割り込み時に呼ばれる関数
    void IntHandlerLAPICTimer();

    // msrで指定した番号にvalueを格納する命令
    void WriteMSR(uint32_t msr, uint64_t value);
    // syscallを実行された時に呼び出されるOS側の関数
    void SyscallEntry(void);

    // argc, argvはアプリへの引数
    // csはアプリを実行するときのcsとssの値を格納
    // ripはアプリのエントリポイント、rspはアプリのスタックポインタ
    // os_stack_pointerは、OSのrsp値を格納しておくための変数へのポインタ。ここに格納した値は、アプリから帰って来る時にOSへ制御を戻す時に使う
    int64_t CallApp(int argc, char** argv, uint16_t ss, uint64_t rip, uint64_t rsp, uint64_t *os_stack_pointer);
    
    // リニアアドレスから8bytesのデータを読み出し返り値にする。
    // ページングの検証のために作成。
    uint64_t ReadFromLinearAddress(void *lin_addr);
}