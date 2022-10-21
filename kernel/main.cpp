#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <malloc.h>
#include <string.h>
#include <deque>

#include "asmfunc.h"
#include "frame_buffer_config.hpp"
#include "console.hpp"
#include "logging.hpp"
#include "memory_map.hpp"
#include "segment.hpp"
#include "paging.hpp"
#include "memory_manager.hpp"
#include "interrupt.hpp"
#include "timer.hpp"
#include "message.hpp" 
#include "task.hpp"
#include "run_application.hpp"
#include "syscall.hpp"
#include "pci.hpp"
#include "usb/xhci/xhci.hpp"
#include "rtl8139/rtl8139.hpp"
#include "ioapic.hpp"
#include "network/network.hpp"
#include "acpi.hpp"
#include "screen.hpp"
#include "terminal.hpp"

void Halt(void);
int printk(const char *format, ...);

extern PixelWriter *pixel_writer;
extern Console *console;
extern BitmapMemoryManager* memory_manager;
TaskManager* task_manager;
TimerManager *timer_manager; // LAPICタイマーの管理をするもの。


alignas(16) uint8_t kernel_main_stack[1024 * 1024]; // カーネル用スタック領域
char logger_buf[sizeof(logging::Logger)]; // ロガー用領域
logging::Logger *logger; 


extern "C" void KernelMainNewStack(
    const FrameBufferConfig *frame_buffer_config_ref, 
    const MemoryMap *memory_map_ref,
    const acpi::RSDP *acpi_table
)
{
    // UEFIから間接的に呼ばれるので、引数で与えられたデータを
    // 新しいスタック領域内に保存するためのコード。
    FrameBufferConfig frame_buffer_config{*frame_buffer_config_ref};
    MemoryMap memory_map{*memory_map_ref};
    
    InitializeGrapfics(frame_buffer_config);
    InitializeConsole(); // コンソールの初期化
    printk("Hello, JINUX!\n\n");
    logger = new(logger_buf) logging::Logger();
    logger->set_level(logging::kERROR); // ログレベルの変更・設定

    console->Deactivate(); // コンソール出力を抑制する

    SetupSegments(); // UEFIの設定を更新し直す
    SetupIdentityPageTable(); // ページングの設定
    InitializeMemoryManager(memory_map); // メモリ管理の開始
    InitializeTSS(); // TSSをGDTに設定

    SetupInterruptDescriptorTable(); // 割り込み・例外ハンドラの設定

    acpi::Initialize(acpi_table); // ACPIの設定

    // Halt();

    InitializeLocalAPICTimer(); // タイマの設定

    ioapic::Initialize();

    InitializeTask(); // マルチタスクの開始
    Task *main_task = task_manager->CurrentTask();
    InitializeSyscall(); // システムコールを使用可能にする

    logger->set_level(logging::kERROR); // 出力減らす
    InitializePCI();

    console->Activate();

    logger->set_level(logging::kDEBUG); // 出力減らす
    // network::Initialize();

    // Halt();

    usb::xhci::Initialize(); // xHCの初期化 

    RunTerminal(&frame_buffer_config);

    // logger->set_level(logging::kINFO); // 文字出力を制限
    /* task_manager->NewTask()
        ->InitContext(RunApplication, 0xefefefef)
        ->Wakeup();
    task_manager->NewTask()
        ->InitContext(RunApplication, 0xefefefef)
        ->Wakeup(); */

    while (1) {
        // main_queueの処理中は割り込みを受け付けないようにする
        __asm__("cli");
        Message msg = main_task->ReceiveMessage();
        if (msg.type == Message::Type::kNullMessage) { // メインキューにメッセージが入っていない時
            main_task->Sleep();
            __asm__("sti");
            continue;
        }
        __asm__("sti");

        // msgの処理
        switch (msg.type) {
            case Message::Type::kTimerTimeout:
                logger->debug("Type: kTimerTimeout, Arg.timeout: %lx, Arg.value: %d\n", 
                    msg.arg.timer.timeout, msg.arg.timer.value);
                break;
            case Message::Type::kInterruptXHCI:
                usb::xhci::ProcessEvents();
                break;
            default:
                break;
        }
    }

    // ここには到達しない
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

