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

    ScreenManager(const FrameBufferConfig *config, PixelColor &default_fg_color, PixelColor &default_bg_color);
    Frame *LongFramePtr() { return long_frame_; }
    Frame *FramePtr() { return frame_; }

    // 現在のカーソルがフレーム内にいればtrueを返す
    bool IsCursorInFrame();
    // カーソルが見える位置に行くまでフレームを移動する。
    // upperがtrueの時、カーソルがフレームの一番上になるように移動する
    // 基本は一番下になる。
    void MoveFrameToCursor(bool upper = false);
    
    // 現在のカーソルに1文字書き込む。カーソルの位置も変化し、画面への出力も行う。
    void PutChar(const CharData &c_data);
    void PutChar(char c); // 色はデフォルトのものを使う
    // 文字列を出力する。文字色と背景色を指定できる。
    void PutString(const char *s, PixelColor &fg_color, PixelColor &bg_color);
    void PutString(const char *s); // 色はデフォルトのものを使う
    // 画面を1行下に（上に）移動する
    void Scroll(bool reverse = false); // reverse=trueの時は上に移動する

    // フレームへロングフレームバッファのデータを全体にコピーする。
    // その後カーソルを表示する
    void CopyAll();
private:
    Frame *frame_; // 画面に出力する部分のフレーム
    Frame *long_frame_; // これまでの出力結果なども保存してある巨大なフレーム

    uint32_t frame_rows_; // フレームの行数（文字）
    uint32_t frame_cols_; // フレームの列数（文字）
    uint32_t long_frame_rows_; // ロングフレームの行数（文字）
    uint32_t long_frame_cols_; // ロングフレームの列数（文字）

    /* 
     * 各メンバ変数の関係は以下のようになっている必要がある
     * long_frame_begin_ <= cursor_row_, base_
     * long_frame_end_ > cursor_row_, base_ 
     */
    uint32_t long_frame_begin_; // ロングフレームで有効な範囲の最初。バッファをリングにするときなどに使えそう。
    uint32_t long_frame_end_; // ロングフレームで背景色を埋めたライン(long_frame_begin_ <= row < long_frame_end_の範囲だけ背景が塗りつぶされているので、スクロール時など注意)

    uint32_t base_{0}; // ロングフレームにおけるフレームの位置
    uint32_t cursor_row_{0}; // カーソル位置（ロングフレーム・行）
    uint32_t cursor_col_{0}; // カーソル位置（ロングフレーム・列）

    PixelColor default_fg_color_;
    PixelColor default_bg_color_;

    // ロングフレームの(c_x, c_y)に文字を書き込む。(c_x, c_y)はピクセルではなく文字の行と列である。
    void WriteCharToLongFrame(uint32_t c_x, uint32_t c_y, const CharData &c_data);
    // ロングフレームの(c_x, c_y)にある文字をフレームに書き込む。
    // 書き込む先がフレーム内でなければ何もしない。
    void CopyChar(uint32_t c_x, uint32_t c_y);
    // 現在のカーソル位置にカーソルを表示する。色は指定できる。
    void CursorShow();
    // 指定したロングフレームの行c_yをデフォルトのバックグラウンドカラーで塗りつぶす
    void FillLine(uint32_t c_y, PixelColor &color);
    // 第c_y行目の文字列をコピーする。
    void CopyLine(uint32_t c_y);

};
