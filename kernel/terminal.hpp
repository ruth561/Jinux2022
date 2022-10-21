#pragma once

#include <array>

#include "screen.hpp"
#include "console.hpp"

class Terminal
{
public:
    Terminal(ScreenManager *screen_manager) :
        screen_manager_{screen_manager} {}
    
    // キー入力があった時に非同期で実行される。
    // USBキーボードのキー入力処理から呼ばれる。
    void OnKeyStroke(uint8_t *keys);

private:
    ScreenManager *screen_manager_;

};


// ターミナルを実行する関数（mainから呼ぶ）
// これまでのconsoleの出力を終了し、新たにターミナルの出力を画面に出力するようにする
void RunTerminal(const FrameBufferConfig *config);
