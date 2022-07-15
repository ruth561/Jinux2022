#include <cstring>
#include <new>

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
        // １行ごとに背景を塗りつぶし、１行下の文字列を表示する。
        for (int row = 0; row < kRows; row++) {
            for (int y = row * 16; y < (row + 1) * 16; y++) {
                for (int x = 0; x < 8 * kColumns; x++) {
                    writer_->Write(x, y, &bg_color_);
                }
            }
            if (row == kRows - 1) break;
            memcpy(buffer_[row], buffer_[row + 1], kColumns + 1);
            WriteString(writer_, 0, 16 * row, buffer_[row], &fg_color_);
        }
        /*  最後の行を0クリアする。 */
        memset(buffer_[kRows - 1], 0, kColumns + 1);
    }
}

// 文字列の出力がPutStringにより簡単にできる。
char console_buf[sizeof(Console)];
Console *console;

extern PixelWriter *pixel_writer;

void InitializeConsole()
{
    // コンソールオブジェクトを生成する。
    // 背景色と文字色が指定可能。
    PixelColor fg_color = {0xff, 0xff, 0xff}, bg_color = {0x00, 0x80, 0xff};
    for (int x = 0; x < pixel_writer->h_resol(); x++) {
        for (int y = 0; y < pixel_writer->v_resol(); y++) {
            pixel_writer->Write(x, y, &bg_color);
        }
    }

    console = new(console_buf) Console(pixel_writer, &fg_color, &bg_color);
}
