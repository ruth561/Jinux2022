#include "terminal.hpp"


Terminal::Terminal(PixelWriter *writer, 
                   PixelColor fg_color, 
                   PixelColor bg_color) : 
                   writer_{writer}, fg_color_{fg_color}, bg_color_{bg_color} {}



void Terminal::PutChar(CharData c_data)
{
    buf_[cursor_row_][cursor_column_].val = c_data.val;
    buf_[cursor_row_][cursor_column_].fg_color = c_data.fg_color;
    buf_[cursor_row_][cursor_column_].bg_color = c_data.bg_color;
}

void Terminal::PutChar(char c)
{
    buf_[cursor_row_][cursor_column_].val = c;
    buf_[cursor_row_][cursor_column_].fg_color = fg_color_;
    buf_[cursor_row_][cursor_column_].bg_color = bg_color_;
}