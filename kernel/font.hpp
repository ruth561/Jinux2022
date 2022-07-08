#pragma once

#include <stdint.h>
#include "graphics.hpp"



/* (x, y) の位置に文字cを書き込む。文字の色は、colorで指定できる。 */
void WriteAscii(PixelWriter* writer, int x, int y, char c, const PixelColor *color);


/* (x, y)から文字列sを出力する関数 */
void WriteString(PixelWriter *writer, int x, int y, const char* s, const PixelColor *color);



