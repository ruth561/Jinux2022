#include <stdlib.h>
#include "terminal.hpp"
extern Console *console;
Terminal *terminal = NULL;



namespace
{
    // 本来は仮想キーコードを利用したほうがいいのだが、ここでは面倒くさいので
    // HIDのキーコードを流用し、USBキーボードから生のデータが流れてくるものとして作る。
    union ModifierKey
    {
        uint8_t data;
        struct {
            uint8_t left_ctrl: 1;
            uint8_t left_shift: 1;
            uint8_t left_alt: 1;
            uint8_t left_gui: 1;
            uint8_t right_ctrl: 1;
            uint8_t right_shift: 1;
            uint8_t right_alt: 1;
            uint8_t right_gui: 1;
        } __attribute__((packed)) bits;
        ModifierKey(uint8_t key) : data{key} {}
    } __attribute__((packed));

    // Shiftキーが押されていない状態でキーを押された時に出力する文字
    // 文字コードとの対応表であり、kKeyCord[key_num]で得られるcharが文字となる
    char kKeyCord[] = "\x00\x00\x00\x00" "abcdef" // 0~9
                      "ghijklmnop" // 10~19
                      "qrstuvwxyz" // 20~29
                      "1234567890" // 30~39
                      "\n\x00\x00\00 -=[]\\" // 40~49
                      "\x00;\'\x00,./\x00\x00\x00" // 50~59
                      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" // 60~69 全てFキー
                      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" // 70~79 
                      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";// 80~89

    // Shiftキーが押されているバージョン
    char kKeyCordShift[] = "\x00\x00\x00\x00" "ABCDEF" // 0~9
                      "GHIJKLMNOP" // 10~19
                      "QRSTUVWXYZ" // 20~29
                      "!@#$%^&*()" // 30~39
                      "\n\x1b\x7f\t _+{}|" // 40~49
                      "~:\"\x00<>?\x00\x00\x00" // 50~59
                      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" // 60~69 全てFキー
                      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" // 70~79 
                      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";// 80~89 

    #define HID_KC_ESC 0x29
    #define HID_KC_BACK_SPACE 0x2a
    #define HID_KC_DELETE 0x4c
    #define HID_KC_RIGHT 0x4f
    #define HID_KC_LEFT 0x50
    #define HID_KC_DOWN 0x51
    #define HID_KC_UP 0x52

}


Terminal::Terminal(ScreenManager *screen_manager) :
        screen_manager_{screen_manager} 
{
    ibuf_len_ = screen_manager_->FrameWidth();
    ibuf_ = reinterpret_cast<char *>(malloc(ibuf_len_));

    strcpy(username_, "user");
    prompt_color_ = PixelColor{0x80, 0xe0, 0x80};

    PutPrompt();
    state_ = TerminalState::kWaitingForInput;
}

void Terminal::OnKeyStroke(uint8_t *keys)
{
    if (state_ == TerminalState::kRunningApplication) {
        /* 
         * アプリ実行時は、Ctfl+Cなどの特殊な命令を受け付けたりするので
         * 別の処理を行う。
         * 後でやる。
         */
        return;
    }
    
    ModifierKey modifier_key{keys[0]};
    if (modifier_key.bits.left_ctrl || modifier_key.bits.right_ctrl) {
        // Ctrlキーが押された場合
        ProcessKeyStrokeWithCtrl(keys);
        return;
    }

    char *key_code_list = kKeyCord;
    if (modifier_key.bits.left_shift || modifier_key.bits.right_shift) {
        // Shiftが押されている場合は表示する文字が大文字になる
        key_code_list = kKeyCordShift;
    }
    
    for (int i = 2; i < 8; i++) {
        if (keys[i] < sizeof(kKeyCord)) {
            uint8_t k = keys[i]; // キーコード
            switch (k) {
                /* カーソルの上下移動はしないよ（テキストエディタでは使えるかも）
                case HID_KC_UP:
                    // カーソルを上へ移動
                    screen_manager_->MoveCursor(CursorMove::Up);
                    break;
                case HID_KC_DOWN:
                    // カーソルを下へ移動
                    screen_manager_->MoveCursor(CursorMove::Down);
                    break; */
                case HID_KC_RIGHT: // カーソルを右へ動かす
                    if (cursor_pos_ < s_len_) {
                        // 文字の置かれていないところへの移動はできない
                        cursor_pos_ = screen_manager_->MoveCursor(CursorMove::Right);
                    }
                    break;

                case HID_KC_LEFT: // カーソルを左へ動かす
                    if (prompt_len_ < cursor_pos_) {
                        // プロンプトは消さないようにする
                        cursor_pos_ = screen_manager_->MoveCursor(CursorMove::Left);
                    }
                    break;
                case HID_KC_BACK_SPACE: // カーソルの一つ左にある文字を消去し、カーソルを左へ移動する
                    HandleBackSpace();
                    break;
                case HID_KC_DELETE: // カーソル上にある文字を消去する。
                    HandleDelete();
                    break;
                default:
                    break;
            }

            char c = key_code_list[k]; // ASCII文字
            if (c) {
                // 対応するASCII文字があれば出力しておく
                PutChar(c);
            }
            if (c == '\n') {
                /* 
                 * ibuf_から文字列を読み込み実行する
                 * ここではとりあえずプロンプトの再表示を行う
                 */
                PutPrompt();
                return;
            }
        }
    }
}


/* 
 * PRIVATE
 */
void Terminal::PutChar(char c)
{
    if (s_len_ == ibuf_len_ - 1) {
        // 表示できる文字列の長さはフレームの横の長さ-1である
        return;
    }

    if (cursor_pos_ == s_len_) { 
        // カーソルから右に文字がない場合は一文字出力
        ibuf_[cursor_pos_] = c;
        cursor_pos_ = screen_manager_->PutChar(c);
        s_len_++;
        ibuf_[s_len_] = '\x00';
    } else {
        // カーソルから右に文字がある場合文字の挿入を実装
        for (int idx = s_len_; cursor_pos_ < idx; idx--) {
            ibuf_[idx] = ibuf_[idx - 1];
        }
        ibuf_[cursor_pos_] = c;
        cursor_pos_ = screen_manager_->PutChar(c);
        s_len_++;
        ibuf_[s_len_] = '\x00';

        // カーソルより右側を表示し直す。
        screen_manager_->UpdateRightFromCursor(ibuf_ + cursor_pos_);
    }
}

void Terminal::HandleBackSpace()
{
    if (cursor_pos_ == prompt_len_) {
        // カーソルの左側に文字がない場合
        return;
    }
    cursor_pos_--;
    screen_manager_->MoveCursor(CursorMove::Left);
    for (int idx = cursor_pos_; idx < s_len_ - 1; idx++) {
        ibuf_[idx] = ibuf_[idx + 1];
    }
    s_len_--;
    ibuf_[s_len_] = '\x00';
    screen_manager_->UpdateRightFromCursor(ibuf_ + cursor_pos_);
}

void Terminal::HandleDelete()
{
    if (cursor_pos_ == s_len_) {
        // カーソルの位置に文字がない場合は何もしない
        return;
    }
    for (int idx = cursor_pos_; idx < s_len_ - 1; idx++) {
        ibuf_[idx] = ibuf_[idx + 1];
    }
    s_len_--;
    ibuf_[s_len_] = '\x00';
    screen_manager_->UpdateRightFromCursor(ibuf_ + cursor_pos_);
}

void Terminal::PutPrompt()
{
    screen_manager_->PutString(username_, prompt_color_);
    screen_manager_->PutString("@jinux-2022: ", prompt_color_);
    /* 
     * ここにカレントディレクトリを書きたい。
     */
    cursor_pos_ = screen_manager_->PutString("$ ", prompt_color_);
    prompt_len_ = cursor_pos_;
    s_len_ = prompt_len_;
}

void Terminal::ProcessKeyStrokeWithCtrl(uint8_t *keys)
{

    for (int i = 2; i < 8; i++) {
        if (keys[i] < sizeof(kKeyCord)) {
            uint8_t k = keys[i]; // キーコード
            switch (k) {
                case HID_KC_UP:
                    // 上スクロール
                    screen_manager_->Scroll(true);
                    break;
                case HID_KC_DOWN:
                    // 下スクロール
                    screen_manager_->Scroll();
                    break;
                case HID_KC_RIGHT:
                    break;
                case HID_KC_LEFT:
                    break;
                default:
                    break;
            }
        }
    }
}



void RunTerminal(const FrameBufferConfig *config)
{
    console->Deactivate();
    
    PixelColor fg_color = PixelColor{0xff, 0xff, 0xff};
    PixelColor bg_color = PixelColor{0x40, 0x40, 0x40};
    ScreenManager *screen_manager = new ScreenManager{config, fg_color, bg_color};

    terminal = new Terminal{screen_manager};

}
