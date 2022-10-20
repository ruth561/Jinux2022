#pragma once 


/* 
 * ターミナル用にグラフィックを管理するクラスを定義する。
 * これまでは、一つのピクセルごとにデータを書き込んで出力していたが、
 * まとめてmemcpyによってframe_bufferにデータをコピーし、
 * 高速化を狙う
 * 
 * 
 */

#include <stdint.h>
#include <string.h>
#include <new>

#include "frame_buffer_config.hpp"
#include "graphics.hpp"

#define BYTES_PER_PIXEL 4
/* 
 * フレームにおいて文字が持つデータをまとめたもの。
 * 文字は、文字の色と背景の色のデータももつ。
 */
struct CharData {
    char val; // 文字
    PixelColor fg_color; // 文字色
    PixelColor bg_color; // 背景色
};

/* 
 * ピクセルの位置を特定したり、その位置へ指定した色を書き込んだりするクラス
 */
class Frame 
{
public:
    Frame(const FrameBufferConfig *config);

    const FrameBufferConfig *Config(); 
    void *Buffer() { return config_->frame_buffer; }
    uint32_t HolizontalResolution() { return config_->holizontal_resolution; }
    uint32_t VerticalResolution() { return config_->vertical_resolution; }

    bool IsInFrame(uint32_t p_x, uint32_t p_y); // ピクセル(p_x, p_y)がこのフレームに収まっているか？

    void *PixelAt(uint32_t p_x, uint32_t p_y); // (p_x, p_y)に位置するピクセルのアドレス
    void PixelWrite(uint32_t p_x, uint32_t p_y, const PixelColor &color); // (p_x, p_y)のピクセルに色ｃを書き込む
    
    void WriteChar(uint32_t p_x, uint32_t p_y, const CharData &c_data); // ピクセル(p_x, p_y)を左上にしてに1文字書き込む

private:
    const FrameBufferConfig *config_;

};



class ScreenManager
{
public:
    static const int kBufferSize = 20; // buffer_はFrameのkBufferSize枚数分の領域を確保する

    ScreenManager(const FrameBufferConfig *config);
    Frame *LongFramePtr() { return long_frame_; }
    Frame *FramePtr() { return frame_; }

    // 第c_y行目の文字列をコピーする。
    void CopyLine(uint32_t c_y);
    // フレームへロングフレームバッファのデータを全体にコピーする。
    void CopyAll();
    
    // 現在のカーソルに1文字書き込む。カーソルの位置も変化し、画面への出力も行う。
    void PutChar(const CharData &c_data);
    // 文字列を出力する。文字色と背景色を指定できる。
    void PutString(const char *s, PixelColor *fg_color, PixelColor *bg_color);

private:
    Frame *frame_; // 画面に出力する部分のフレーム
    Frame *long_frame_; // これまでの出力結果なども保存してある巨大なフレーム

    uint32_t frame_rows_; // フレームの行数（文字）
    uint32_t frame_cols_; // フレームの列数（文字）
    uint32_t long_frame_rows_; // ロングフレームの行数（文字）
    uint32_t long_frame_cols_; // ロングフレームの列数（文字）

    uint32_t base_{0}; // ロングフレームにおけるフレームの位置
    uint32_t cursol_row_{0}; // カーソル位置（ロングフレーム・行）
    uint32_t cursol_col_{0}; // カーソル位置（ロングフレーム・列）

    // ロングフレームの(c_x, c_y)に文字を書き込む。(c_x, c_y)はピクセルではなく文字の行と列である。
    void WriteCharToLongFrame(uint32_t c_x, uint32_t c_y, const CharData &c_data);
    // ロングフレームの(c_x, c_y)にある文字をフレームに書き込む。
    // 書き込む先がフレーム内でなければ何もしない。
    void CopyChar(uint32_t c_x, uint32_t c_y);
};


// 仮の関数
void ScreenInit(const FrameBufferConfig *config);
