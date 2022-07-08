#include "frame_buffer_config.hpp"
#include "console.hpp"
#include <cstddef>
#include <cstdio>

// OSを停止させる関数。CPUの動きを止めるので、安全。
void Halt(void);

// new演算子を使用するのに必要な宣言。
void *operator new(size_t size, void *buf)
{
    return buf;
}
void operator delete(void * obj) noexcept {}

// printfのような使い方ができる。コンソールオブジェクト上で機能する。
int printk(const char *format, ...);

//  ピクセル描画のためのオブジェクトを格納する領域。
char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];

// 指定したピクセルの座標(x, y)を指定した色で光らせることが可能なオブジェクト。
PixelWriter *pixel_writer;

// コンソールオブジェクトを格納するための領域。
char console_buf[sizeof(Console)];

// 文字列の出力がPutStringにより簡単にできる。
Console *console;



extern "C" void KernelMain(
    const FrameBufferConfig *frame_buffer_config
)
{
    // ピクセルの扱いがRGBなのかBGRなのか判断し、
    // グローバルなPixelWriterオブジェクトpixel_writerを作成する。
    switch (frame_buffer_config->pixel_format) {
        case kPixelRGBResv8BitPerColor:
            pixel_writer = new(pixel_writer_buf) RGBResv8BitPerColorPixelWriter{frame_buffer_config};
            break;
        case kPixelBGRResv8BitPerColor:
            pixel_writer = new(pixel_writer_buf) BGRResv8BitPerColorPixelWriter{frame_buffer_config};
            break;
    }

    // コンソールオブジェクトを生成する。
    // 背景色と文字色が指定可能。
    PixelColor fg_color = {0, 0, 0}, bg_color = {0xff, 0xff, 0xff};
    for (int x = 0; x < pixel_writer->h_resol(); x++) {
        for (int y = 0; y < pixel_writer->v_resol(); y++) {
            pixel_writer->Write(x, y, &bg_color);
        }
    }
    console = new(console_buf) Console(pixel_writer, &fg_color, &bg_color);
    
    printk("HELLO, WORLD!\n");

    Halt();
}



void Halt(void)
{
    while (1) __asm__("hlt");
}

int printk(const char *format, ...) {
    int res;
    va_list ap;
    char s[1024];

    va_start(ap, format);
    res = vsprintf(s, format, ap);
    va_end(ap);

    console->PutString(s);
    return res;
}