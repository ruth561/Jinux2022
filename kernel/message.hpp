#pragma once
#include <stdint.h>
#include "usb/xhci/trb.hpp"

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
            bool success; // 成功した場合
        } xhc_port_init;
    } arg;
};

