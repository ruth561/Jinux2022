#include "graphics.hpp"

/* graphics.hpp で宣言した関数の実装
   分割コンパイルの利点を活かすために、宣言と定義を分けている。
   オブジェクトファイルで保存し、リンク時に main.o と組み合わさる */



void RGBResv8BitPerColorPixelWriter::Write(int x, int y, const PixelColor *c) {
    uint8_t *p = PixelAt(x, y);
    p[0] = c->r;
    p[1] = c->g;
    p[2] = c->b;
}

void BGRResv8BitPerColorPixelWriter::Write(int x, int y, const PixelColor *c) {
    uint8_t *p = PixelAt(x, y);
    p[0] = c->b;
    p[1] = c->g;
    p[2] = c->r;
}