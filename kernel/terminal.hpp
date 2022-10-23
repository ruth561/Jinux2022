#pragma once

#include <array>
#include <string>

#include "screen.hpp"
#include "console.hpp"

enum class TerminalState {
    kWaitingForInput,
    kRunningApplication
};

class Terminal
{
public:
    Terminal(ScreenManager *screen_manager);
    
    // キー入力があった時に非同期で実行される。
    // USBキーボードのキー入力処理から呼ばれる。
    void OnKeyStroke(uint8_t *keys);

    // プロンプトを表示
    void PutPrompt();

private:
    ScreenManager *screen_manager_;
    TerminalState state_; // この値次第でキー入力の挙動などが変わる

    char username_[20]; // ユーザー名（最大19文字）
    PixelColor prompt_color_; // プロンプトの色（Terminalで初期化される）

};


// ターミナルを実行する関数（mainから呼ぶ）
// これまでのconsoleの出力を終了し、新たにターミナルの出力を画面に出力するようにする
void RunTerminal(const FrameBufferConfig *config);
