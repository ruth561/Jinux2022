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

void Halt(void);
int printk(const char *format, ...);

extern PixelWriter *pixel_writer;
extern Console *console;
extern BitmapMemoryManager* memory_manager;
TaskManager* task_manager;
TimerManager *timer_manager; // LAPICタイマーの管理をするもの。
std::deque<Message> *main_queue; // 割り込み通知を溜めるキュー


alignas(16) uint8_t kernel_main_stack[1024 * 1024]; // カーネル用スタック領域
char logger_buf[sizeof(logging::Logger)]; // ロガー用領域
logging::Logger *logger;


void TaskA(uint64_t id, int64_t data)
{
    while (1) {
        printk("TaskA!!\n");
    }
}

extern "C" void KernelMainNewStack(
    const FrameBufferConfig *frame_buffer_config_ref, 
    const MemoryMap *memory_map_ref
)
{
    // UEFIから間接的に呼ばれるので、引数で与えられたデータを
    // 新しいスタック領域内に保存するためのコード。
    FrameBufferConfig frame_buffer_config{*frame_buffer_config_ref};
    MemoryMap memory_map{*memory_map_ref};
    
    InitializeGrapfics(frame_buffer_config);
    InitializeConsole(); // コンソールの初期化
    logger = new(logger_buf) logging::Logger();
    logger->set_level(logging::kDEBUG); // ログレベルの変更・設定

    /* uint64_t cr0 = GetCR0();
    uint64_t cr2 = GetCR2();
    uint64_t cr3 = GetCR3();
    uint64_t cr4 = GetCR4();
    logger->debug("CR0: %016lx\n", cr0);
    logger->debug("CR2: %016lx\n", cr2);
    logger->debug("CR3: %016lx\n", cr3);
    logger->debug("CR4: %016lx\n", cr4); */
    
    SetupSegments(); // UEFIの設定を更新し直す
    SetupIdentityPageTable(); // ページングの設定
    InitializeMemoryManager(memory_map); // メモリ管理の開始
    InitializeTSS(); // TSSをGDTに設定

    main_queue = new std::deque<Message>; // 割り込み用キューの生成
    SetupInterruptDescriptorTable(); // 割り込み・例外ハンドラの設定

    InitializeLocalAPICTimer(); // タイマの設定
    InitializeTask(); // マルチタスクの開始

    uint64_t task_a_id = task_manager->NewTask()
        ->InitContext(TaskA, 0xdeadbeef)
        ->Wakeup()
        ->ID();
    logger->debug("TaskA id is %ld\n", task_a_id);

    // logger->set_level(logging::kINFO); // 文字出力を制限
    // task_manager->NewTask()->InitContext(RunApplication, 0xcafebabe);



    int cnt = 0;
    while (1) {
        cnt++;
        if (cnt % 500 == 0) {
            task_manager->Sleep(task_a_id);
        } else if (cnt % 10001 == 0) {
            task_manager->Wakeup(task_a_id);
        }
        // main_queueの処理中は割り込みを受け付けないようにする
        __asm__("cli");
        if (main_queue->size() == 0) {
            __asm__("sti\n\thlt");
            continue;
        }
        Message msg = main_queue->front();
        main_queue->pop_front();
        __asm__("sti");

        // msgの処理
        switch (msg.type) {
            case Message::Type::kTimerTimeout:
                logger->debug("Type: kTimerTimeout, Arg.timeout: %lx, Arg.value: %d\n", 
                    msg.arg.timer.timeout, msg.arg.timer.value);
                break;
            default:
                break;
        }
    }








    // ０除算例外を引き起こすコード
    __asm__("divb 0");
    uintptr_t p = 0x6234567890;
    logger->debug("memory access %08x(%p)\n", *reinterpret_cast<int *>(p), p);
    /* for (uint64_t i = 0; i < 64; i++) {
        p = (static_cast<uint64_t>(1) << i);
        logger->debug("memory access %08x(%p)\n", *reinterpret_cast<int *>(p), p);
    } */

    logger->info("execute Halt() ...\n");
    Halt();
}



void Halt(void)
{
    while (1) __asm__("cli\n\thlt");
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

