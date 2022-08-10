#pragma once
#include <stdint.h>

// カーネルのmain内で処理される割り込み一つあたりのデータ。
struct Message
{
    enum class Type {
        kNullMessage, 
        kTimerTimeout,
        kInterruptXHCI,
    } type;

    union {
        struct {
            uint64_t timeout;
            int value;
        } timer;
        struct {
            uint64_t port_num;
            int value;
        } xhc_port_init;
    } arg;
};

