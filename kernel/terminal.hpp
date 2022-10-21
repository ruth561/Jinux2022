#pragma once

#include <array>

#include "screen.hpp"
#include "console.hpp"

class Terminal
{
public:
    Terminal(ScreenManager *screen_manager) :
        screen_manager_{screen_manager} {}

private:
    ScreenManager *screen_manager_;
};


// ターミナルを実行する関数（mainから呼ぶ）
// これまでのconsoleの出力を終了し、新たにターミナルの出力を画面に出力するようにする
void RunTerminal(const FrameBufferConfig *config);
