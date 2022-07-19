#pragma once

#include <stdint.h>

extern "C" {
    uint16_t GetCS(void);
    void LoadIDT(uint16_t limit, uint64_t offset);
    void LoadGDT(uint16_t limit, uint64_t offset);
    void SetCSSS(uint16_t cs, uint16_t ss);
    void SetDSAll(uint16_t value);
    void SetCR3(uint64_t value);
    uint64_t GetCR0();
    uint64_t GetCR2();
    uint64_t GetCR3();
    uint64_t GetCR4();

    void SwitchContext(void* next_ctx, void* current_ctx);
    
    // リニアアドレスから8bytesのデータを読み出し返り値にする。
    // ページングの検証のために作成。
    uint64_t ReadFromLinearAddress(void *lin_addr);
}