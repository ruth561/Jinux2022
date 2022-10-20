#include "screen.hpp"
#include <stdlib.h>

extern const uint8_t _binary_hankaku_bin_start;
extern const uint8_t _binary_hankaku_bin_end;
extern const uint8_t _binary_hankaku_bin_size;

const uint8_t* GetFont(char c);

int printk(const char *format, ...);
void Halt(void);

Frame::Frame(const FrameBufferConfig *config) : config_{config} {}

const FrameBufferConfig *Frame::Config()
{
    return config_;
}

bool Frame::IsInFrame(uint32_t p_x, uint32_t p_y)
{
    return (p_x < config_->holizontal_resolution) && \
           (p_y < config_->vertical_resolution);
}

void *Frame::PixelAt(uint32_t p_x, uint32_t p_y) 
{
    if (!IsInFrame(p_x, p_y)) {
        return NULL;
    }

    return config_->frame_buffer + 4 * (config_->pixels_per_scan_line * p_y + p_x);
}

void Frame::PixelWrite(uint32_t p_x, uint32_t p_y, const PixelColor &color)
{
    uint8_t *p = reinterpret_cast<uint8_t *>(PixelAt(p_x, p_y));
    if (p) {
        p[0] = color.r;
        p[1] = color.g;
        p[2] = color.b;
    }
}

void Frame::WriteChar(uint32_t p_x, uint32_t p_y, const CharData &c_data)
{
    const uint8_t *kFont = GetFont(c_data.val);
    if (kFont == NULL) return;
    for (int dx = 0; dx < 8; dx++) {
        for (int dy = 0; dy < 16; dy++) {
            if ((kFont[dy] << dx) & 0x80u) {
                PixelWrite(p_x + dx, p_y + dy, c_data.fg_color);
            } else {
                PixelWrite(p_x + dx, p_y + dy, c_data.bg_color);
            }
        }
    }
}


/* =============================================================================================================================
 *
 *
 * 
 * 
 * 
 * 
 * 
 * =============================================================================================================================
 */


ScreenManager::ScreenManager(const FrameBufferConfig *config, PixelColor &default_fg_color, PixelColor &default_bg_color) :
    default_fg_color_{default_fg_color}, default_bg_color_{default_bg_color}
{
    frame_ = new Frame{config};
    frame_rows_ = frame_->VerticalResolution() / 16;
    frame_cols_ = frame_->HolizontalResolution() / 8;

    FrameBufferConfig *long_frame_config = new FrameBufferConfig {
        (uint8_t *) malloc(4 * config->holizontal_resolution * config->vertical_resolution * kBufferSize), 
        config->holizontal_resolution, 
        config->holizontal_resolution, 
        config->vertical_resolution * kBufferSize, 
        config->pixel_format
    };
    long_frame_ = new Frame{long_frame_config};
    long_frame_rows_ = long_frame_->VerticalResolution() / 16;
    long_frame_cols_ = long_frame_->HolizontalResolution() / 8;
}


void ScreenManager::CopyChar(uint32_t c_x, uint32_t c_y) {
    if (!long_frame_->IsInFrame(8 * c_x, 16 * c_y)) {
        // 指定した座標がロングフレームに含まれない時
        return;
    }
    uint32_t src_x = c_x;
    uint32_t src_y = c_y;
    uint32_t dst_x = c_x;
    uint32_t dst_y = c_y - base_;
    if (!frame_->IsInFrame(8 * dst_x, 16 * dst_y)) {
        // 指定した文字がフレーム外の時
        return;
    }

    for (int i = 0; i < 16; i++) {
        memcpy(
            frame_->PixelAt(8 * dst_x, 16 * dst_y + i), 
            long_frame_->PixelAt(8 * src_x, 16 * src_y + i), 
            8 * BYTES_PER_PIXEL
        );
    }
    return;
}

void ScreenManager::CopyLine(uint32_t c_y)
{
    if (!long_frame_->IsInFrame(0, 16 * c_y)) {
        // 指定した行がロングフレームに含まれない時
        return;
    }
    uint32_t src_y = c_y;
    uint32_t dst_y = c_y - base_;
    if (!frame_->IsInFrame(0, 16 * dst_y)) {
        // 指定した行がフレーム外の時
        return;
    }

    for (int i = 0; i < 16; i++) {
        memcpy(
            frame_->PixelAt(0, 16 * dst_y + i), 
            long_frame_->PixelAt(0, 16 * src_y + i), 
            BYTES_PER_PIXEL * frame_cols_ * 8
        );
    }
    return;
}

void ScreenManager::CopyAll()
{
    for (int row = 0; row < frame_rows_; row++) {
        CopyLine(base_ + row);
    }
}

void ScreenManager::WriteCharToLongFrame(uint32_t c_x, uint32_t c_y, const CharData &c_data)
{
    /* 
     * 文字は縦16ピクセル、横8ピクセルなので文字を置く領域の左上のアドレスは
     */
    long_frame_->WriteChar(c_x * 8, c_y * 16, c_data);
}

void ScreenManager::PutChar(const CharData &c_data)
{
    if (c_data.val == '\n') {
        // 改行を出力する時は行を変える
        CopyChar(cursor_col_, cursor_row_);
        cursor_col_ = 0;
        cursor_row_++;
    } else {
        WriteCharToLongFrame(cursor_col_, cursor_row_, c_data);
        CopyChar(cursor_col_, cursor_row_);

        cursor_col_++;
        if (cursor_col_ >= long_frame_cols_) {
            // カーソルが画面の右端に到達したら行を変える
            cursor_col_ = 0;
            cursor_row_++;
        }
    }
    CursorShow();
}

void ScreenManager::PutChar(char c)
{
    CharData c_data;
    c_data.val = c;
    c_data.fg_color = default_fg_color_;
    c_data.bg_color = default_bg_color_;
    PutChar(c_data);
}

void ScreenManager::PutString(const char *s, PixelColor &fg_color, PixelColor &bg_color)
{
    CharData c_data;
    c_data.fg_color = fg_color;
    c_data.bg_color = bg_color;
    while (*s) { // NULL文字に当たるまで出力し続ける
        c_data.val = *s;
        PutChar(c_data);
        s++;
    }
}

void ScreenManager::PutString(const char *s)
{
    PutString(s, default_fg_color_, default_bg_color_);
}

void ScreenManager::Scroll(bool reverse)
{
    if (reverse) {
        // 上にスクロール
        if (base_ > 0) {
            base_--;
            CopyAll();
        } 
    } else {
        // 下へスクロール
        // カーソルより下へはスクロールしない
        if (base_ < cursor_row_) {
            base_++;
            CopyAll();
        }
    }
    CursorShow();
}

void ScreenManager::CursorShow()
{
    uint32_t c_x = cursor_col_;
    uint32_t c_y = cursor_row_ - base_;
    for (int dx = 0; dx < 8; dx++) {
        for (int dy = 0; dy < 16; dy++) {
            frame_->PixelWrite(c_x * 8 + dx, c_y * 16 + dy, default_fg_color_);
        }
    }
}

void ScreenInit(const FrameBufferConfig *config)
{
    PixelColor fg_color = PixelColor{0x00, 0x00, 0x00};
    PixelColor bg_color = PixelColor{0xff, 0xff, 0xff};

    ScreenManager *screen_manager = new ScreenManager(config, fg_color, bg_color);
    printk("long frame buffer: %p\n", screen_manager->LongFramePtr()->Buffer());
    printk("frame buffer  %p\n", screen_manager->FramePtr()->Buffer());

    printk("(long) holizontal: %d\n", screen_manager->LongFramePtr()->Config()->holizontal_resolution);
    printk("(long) vertical: %d\n", screen_manager->LongFramePtr()->Config()->vertical_resolution);

    printk("holizontal: %d\n", screen_manager->FramePtr()->Config()->holizontal_resolution);
    printk("vertical: %d\n", screen_manager->FramePtr()->Config()->vertical_resolution);

    CharData c_data;
    c_data.val = 'A';
    c_data.fg_color = {0x00, 0x00, 0xff};
    c_data.bg_color = {0xff, 0xff, 0xff};
    screen_manager->PutChar(c_data);


    screen_manager->PutString("Hello, world.\n\nSee You..\n\n\n\nOhh\n");

    for (int i = 0; i < 3; i++) {
        screen_manager->Scroll();
    }
    
}