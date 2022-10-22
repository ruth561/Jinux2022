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

/* 
 * PUBLICな関数
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

    // フレームの背景をデフォルトの色で塗りつぶす
    for (uint32_t p_x = 0; p_x < frame_->HolizontalResolution(); p_x++) {
        for (uint32_t p_y = 0; p_y < frame_->VerticalResolution(); p_y++) {
            frame_->PixelWrite(p_x, p_y, default_bg_color_);
        }
    }
    // フレームの位置やカーソルの位置を初期化
    base_ = 0;
    cursor_col_ = 0;
    cursor_row_ = 0;
    long_frame_begin_ = 0;
    long_frame_end_ = 0;
    // ロングフレームの最初をバックグラウンドカラーで埋める
    for (int c_y = 0; c_y < frame_rows_; c_y++) {
        FillLine(c_y, default_bg_color_);
        long_frame_end_++;
    }

    // フレームに反映
    CopyAll();
    CursorShow();
}

void ScreenManager::MoveFrameToCursor(bool upper)
{
    if (upper) {
        base_ = cursor_row_;
    } else {
        // max(cursor_row_ - frame_rows_ + 1, long_frame_begin)_)
        if (long_frame_begin_ + frame_rows_ > cursor_row_) {
            // ロングフレームの先頭にフレームを持ってきた時、カーソルがその範囲内にいれば
            base_ = long_frame_begin_;
        } else {
            base_ = cursor_row_ - frame_rows_ + 1;
        }
    }
    CopyAll();
}

void ScreenManager::MoveCursor(CursorMove dir)
{
    // カーソルのいた場所に元々のデータをコピーする
    CopyChar(cursor_col_, cursor_row_);

    switch (dir) {
        case CursorMove::Right:
            if (cursor_col_ + 1 < frame_cols_) {
                cursor_col_++;
            }
            break;
        
        case CursorMove::Left:
            if (cursor_col_ > 0) {
                cursor_col_--;
            }
            break;
    }

    CursorShow();
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
    if (!IsCursorInFrame()) {
        // カーソル外に飛び出した場合は、下にスクロールする
        Scroll();
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
        if (base_ > long_frame_begin_) {
            base_--;
        } 
    } else {
        // 下へスクロール
        // カーソルより下へはスクロールしない
        if (base_ < cursor_row_) {
            base_++;
        }
    }
    CopyAll();
}


/* 
 * PRIVATEな関数
 */
bool ScreenManager::IsCursorInFrame()
{
    return (0 <= cursor_row_ - base_) && \
           (cursor_row_ - base_ < frame_rows_);
}

void ScreenManager::WriteCharToLongFrame(uint32_t c_x, uint32_t c_y, const CharData &c_data)
{
    /* 
     * 文字は縦16ピクセル、横8ピクセルなので文字を置く領域の左上のアドレスは
     */
    long_frame_->WriteChar(c_x * 8, c_y * 16, c_data);
}

void ScreenManager::FillLine(uint32_t c_y, PixelColor &color)
{
    CharData c_data;
    c_data.val = ' ';
    c_data.bg_color = color;
    for (int c_x = 0; c_x < frame_cols_; c_x++) {
        WriteCharToLongFrame(c_x, c_y, c_data);
    }
}

void ScreenManager::CursorShow()
{
    if (!IsCursorInFrame()) {
        return;
    }
    uint32_t c_x = cursor_col_;
    uint32_t c_y = cursor_row_ - base_;
    for (int dx = 0; dx < 8; dx++) {
        for (int dy = 0; dy < 16; dy++) {
            frame_->PixelWrite(c_x * 8 + dx, c_y * 16 + dy, default_fg_color_);
        }
    }
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
        if (base_ + row >= long_frame_end_) {
            FillLine(base_ + row, default_bg_color_);
            long_frame_end_ = base_ + row + 1;
        }
        CopyLine(base_ + row);
    }
    CursorShow();
}
