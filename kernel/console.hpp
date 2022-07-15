#pragma once

#include "graphics.hpp"
#include "font.hpp"

// 画面をコンソール表示にする。
// PutString(string)で文字を出力する。
class Console
{
public:
    /* 画面の行数と列数の定義
       途中で変更できないようにconst属性をつけている */
    /* static const int kRows = 25, kColumns = 80; */
    static const int kRows = 37, kColumns = 100;

    // fg_colorには、文字の色を指定。
    // bg_colorには、背景の色を指定。
    Console(
        PixelWriter *writer, 
        const PixelColor *fg_color, 
        const PixelColor *bg_color);
    
    // 現在のカーソル位置から文字列を出力する。
    // 列数より飛び出る場合は出力しない。
    // 改行マークの時は、新しい行にカーソルが移動する。
    void PutString(const char *s);

private:
    // カーソルが次の行の先頭に移動する。もしカーソル位置が最下層ならば、
    // buffer_のデータを一行移し替え、画面表示をアップデートする。
    // この時は、カーソルは行頭に移動するだけなので注意する。
    void NewLine();

    PixelWriter *writer_;
    const PixelColor fg_color_, bg_color_;
    char buffer_[kRows][kColumns + 1];          // kColumns + 1なのは最後のNULL文字のため
    int cursor_row_, cursor_column_;
};

void InitializeConsole();
