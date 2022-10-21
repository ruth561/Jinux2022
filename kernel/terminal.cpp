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


}



void Terminal::OnKeyStroke(uint8_t *keys)
{
    screen_manager_->PutString("Hello, World! ");
}

void RunTerminal(const FrameBufferConfig *config)
{
    console->Deactivate();
    
    PixelColor fg_color = PixelColor{0xff, 0xff, 0xff};
    PixelColor bg_color = PixelColor{0x40, 0x40, 0x40};
    ScreenManager *screen_manager = new ScreenManager{config, fg_color, bg_color};

    terminal = new Terminal{screen_manager};

}
