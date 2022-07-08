#include <cstring>
#include "console.hpp"


Console::Console(
    PixelWriter *writer, 
    const PixelColor *fg_color, 
    const PixelColor *bg_color
) : writer_(writer), fg_color_(*fg_color), bg_color_(*bg_color),
    buffer_(), cursor_row_(0), cursor_column_(0) {
    
    /* // コンソールの縦横の大きさをピクセルライターの画面に応じた値に変更。
    int *kRows_p    = const_cast<int *>(&kRows);
    int *kColumns_p = const_cast<int *>(&kColumns);
    *kRows_p    = writer_->v_resol() / 16;
    *kColumns_p = writer_->h_resol() / 8; */

    // コンストラクタで背景を塗りつぶす。
    for (int y = 0; y < 16 * kRows; y++) {
        for (int x = 0; x < 8 * kColumns; x++) {
            writer_->Write(x, y, &bg_color_);
        }
    }
}


void Console::PutString(const char *s)
{
    while (*s) {
        if (*s == '\n') {
            NewLine();
        } else if (cursor_column_ < kColumns - 1) {
            WriteAscii(writer_, 8 * cursor_column_, 16 * cursor_row_, *s, &fg_color_);
            buffer_[cursor_row_][cursor_column_] = *s;
            cursor_column_++;
        }
        s++;
    }
}

void Console::NewLine()
{
    cursor_column_ = 0;
    if (cursor_row_ < kRows - 1) {
        cursor_row_++;
    } else {
        /*  まず、背景色をbg_color_でクリアする。 */
        for (int y = 0; y < 16 * kRows; y++) {
            for (int x = 0; x < 8 * kColumns; x++) {
                writer_->Write(x, y, &bg_color_);
            }
        }

        /*  buffer_のデータを一行ずつ移し替える。 */
        for (int row = 0; row < kRows - 1; row++) {
            memcpy(buffer_[row], buffer_[row + 1], kColumns + 1);
            WriteString(writer_, 0, 16 * row, buffer_[row], &fg_color_);
        }
        /*  最後の行を0クリアする。 */
        memset(buffer_[kRows - 1], 0, kColumns + 1);
    }
}
