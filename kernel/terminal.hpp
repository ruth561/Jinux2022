#pragma once

#include <array>

#include "graphics.hpp"
#include "font.hpp"
#include "screen.hpp"

class Terminal
{
public:
    static const int kBufferRows = 400; // これまでの出力を保持する列の数
    static const int kScreenRows = 37, kScreenColumns = 100;
    
    Terminal(PixelWriter *writer, 
             PixelColor fg_color, 
             PixelColor bg_color);

    void DrawAll(); // スクリーン全体に現在のバッファを描画する

    void PutChar(CharData c_data); // （色指定あり）現在のカーソルに1文字置く処理
    void PutChar(char c); // （色指定なし）現在のカーソルに1文字置く処理

private:
    PixelWriter *writer_;

    std::array<std::array<CharData, kScreenColumns>, kBufferRows> buf_;
    int screen_{0}; // スクリーンの1行目に表示するbuf_の行
    int cursor_row_ = 0, cursor_column_ = 0;

    PixelColor fg_color_{0xff, 0xff, 0xff}; // デフォルトの文字色（白）
    PixelColor bg_color_{0x00, 0x00, 0x00}; // デフォルトの背景色（黒）

};


