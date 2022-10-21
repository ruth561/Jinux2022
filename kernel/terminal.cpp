#include "terminal.hpp"
extern Console *console;
Terminal *terminal;

void RunTerminal(const FrameBufferConfig *config)
{
    console->Deactivate();
    
    PixelColor fg_color = PixelColor{0xff, 0xff, 0xff};
    PixelColor bg_color = PixelColor{0x40, 0x40, 0x40};
    ScreenManager *screen_manager = new ScreenManager{config, fg_color, bg_color};

    terminal = new Terminal{screen_manager};

}
