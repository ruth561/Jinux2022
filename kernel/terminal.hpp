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

private:
    ScreenManager *screen_manager_;
    TerminalState state_; // この値次第でキー入力の挙動などが変わる

    char *ibuf_; // 入力バッファ（スクリーンの横の長さ分の領域をmallocで確保する）
    uint32_t ibuf_len_; // スクリーンの横の長さ
    uint32_t s_len_; // カーソルのいる行に出力している文字列の長さ

    char username_[20]; // ユーザー名（最大19文字）
    PixelColor prompt_color_; // プロンプトの色（Terminalで初期化される）

    uint32_t prompt_len_; // プロンプトの長さ（この空間にはカーソルは進出してはならない）
    uint32_t cursor_pos_; // カーソルの位置（左端からの距離）

    void ProcessKeyStrokeWithCtrl(uint8_t *keys); // Ctrlキーが押された場合の処理はこの関数が行う

    // 入力バッファに文字を書き込み、スクリーンにも表示させる関数
    void PutChar(char c);
    void HandleBackSpace(); // バックスペースキーを押されたときに呼ばれる
    void HandleDelete(); // Deleteキーを押されたときに呼ばれる
    // プロンプトを表示。返り値は表示した後のカーソル位置を示す。内部でcursor_pos_やprompt_lenを決める。
    void PutPrompt();
};


// ターミナルを実行する関数（mainから呼ぶ）
// これまでのconsoleの出力を終了し、新たにターミナルの出力を画面に出力するようにする
void RunTerminal(const FrameBufferConfig *config);
