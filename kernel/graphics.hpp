#pragma once

#include "frame_buffer_config.hpp"

// 色の指定は、必ずこの構造体を介して行う。
// r: 赤
// g: 緑
// b: 青
// RGBなのかBGRなのかの判断はPixelWriterの実装で実現しており、
// 色の指定はRGBで統一しているようにしている。
struct PixelColor {
    uint8_t r, g, b;
};

 
class PixelWriter
{
public:
    // 引数にUEFIから渡されたFrameBufferConfig構造体のポインタを渡す。
    PixelWriter(
        const FrameBufferConfig *config_
    ) : config{config_} {}

    virtual ~PixelWriter() = default;

    // ピクセル(x, y)を色cで塗りつぶす。
    // RGBなのかBGRなのかで実装が変わるので、小クラスへの仮想関数として実装。
    virtual void Write(int x, int y, const PixelColor *c) = 0;

    // 画面の横(holizontal)方向の解像度を返す。
    uint32_t h_resol()
    {
        return config->holizontal_resolution;
    }

    // 画面の縦(vertical)方向の解像度を返す。
    uint32_t v_resol()
    {
        return config->vertical_resolution;
    }
protected:
    // ピクセル(x, y)のアドレスを返す。
    uint8_t *PixelAt(int x, int y) {
        return config->frame_buffer + 4 * (config->pixels_per_scan_line * y + x);
    }

private:
    const FrameBufferConfig *config;
};


class RGBResv8BitPerColorPixelWriter : public PixelWriter
{
public:
    using PixelWriter::PixelWriter;
    virtual void Write(int x, int y, const PixelColor *c) override;
};

class BGRResv8BitPerColorPixelWriter : public PixelWriter
{
public:
    using PixelWriter::PixelWriter;
    virtual void Write(int x, int y, const PixelColor *c) override;
};


void InitializeGrapfics(FrameBufferConfig frame_buffer_config);
