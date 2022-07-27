#pragma once

#include <cstdint>


/* 
 * Macine Specific Registerの構造体定義
 */

union IA32_EFER // 0xc000'0080
{
    uint64_t data;
    struct {
        uint64_t syscall_enable: 1; // システムコールを有効する
        uint64_t : 7;
        uint64_t ia32e_mode_enable: 1; // IA32eモードにする
        uint64_t : 1;
        uint64_t ia32e_mode_active: 1;
        uint64_t execute_disable_bit_enable: 1; // ページングのNXビットを有効化し、実行制限できるようにする
        uint64_t : 52;
    } bits;
};

union IA32_STAR // 0xc000'0081
{
    uint64_t data;
    struct {
        uint32_t : 32;
        uint16_t syscall_cs_ss; // syscall実行時にcsに設定されるセレクタ値（ssにはこの値に８を足したものが格納される）
        uint16_t sysret_cs_ss; // sysret実行時にcsに設定されるセレクタ値-16。ssにはこの値に８を足したものが格納される）
    } __attribute__((packed)) bits;
    
};

union IA32_LSTAR // 0xc000'0082
{
    uint64_t data;
    uint64_t syscall_target_address; // システムコール時に実行される手続きのエントリポイント
};

union IA32_FMASK // 0xc000'0084
{
    uint64_t data;
    uint64_t syscall_rflags; // システムコール時にセットされるRFLAGSの値を定義
};

