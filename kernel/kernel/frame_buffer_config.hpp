#pragma once

#include <stdint.h>

enum PixelFormat {
    kPixelRGBResv8BitPerColor,
    kPixelBGRResv8BitPerColor
};

struct FrameBufferConfig {
    uint8_t             *frame_buffer;          // フレームバッファの先頭アドレス
    uint32_t            pixels_per_scan_line;   // １行あたりのピクセルの数。横方向の解像度とは限りない。
    uint32_t            holizontal_resolution;  // 横方向の解像度
    uint32_t            vertical_resolution;    // 縦方向の解像度
    enum PixelFormat    pixel_format;           // RGBかBGRか
};
