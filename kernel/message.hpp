#pragma once


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
            unsigned long timeout;
            int value;
        } timer;
    } arg;
};

