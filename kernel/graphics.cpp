#include <new>

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


// 指定したピクセルの座標(x, y)を指定した色で光らせることが可能なオブジェクト。
char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter *pixel_writer;

void InitializeGrapfics(FrameBufferConfig frame_buffer_config)
{
    // ピクセルの扱いがRGBなのかBGRなのか判断し、
    // グローバルなPixelWriterオブジェクトpixel_writerを作成する。
    switch (frame_buffer_config.pixel_format) {
        case kPixelRGBResv8BitPerColor:
            pixel_writer = new(pixel_writer_buf) RGBResv8BitPerColorPixelWriter{&frame_buffer_config};
            break;
        case kPixelBGRResv8BitPerColor:
            pixel_writer = new(pixel_writer_buf) BGRResv8BitPerColorPixelWriter{&frame_buffer_config};
            break;
    }
}