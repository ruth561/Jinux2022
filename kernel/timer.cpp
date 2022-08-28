
#include "timer.hpp"
#include "logging.hpp"
#include "message.hpp"
#include "task.hpp"

extern logging::Logger *logger;
extern TimerManager *timer_manager;
extern TaskManager* task_manager;
extern std::deque<Message> *main_queue;
int printk(const char *format, ...);
void Halt();


namespace
{
    volatile uint32_t *kLVTTimerRegister = reinterpret_cast<uint32_t *>(0xfee00320ul);
    volatile uint32_t *kInitialCountRegister = reinterpret_cast<uint32_t *>(0xfee00380ul);
    volatile uint32_t *kCurrentCountRegister = reinterpret_cast<uint32_t *>(0xfee00390ul);
    volatile uint32_t *kDivideConfigurationRegister = reinterpret_cast<uint32_t *>(0xfee003e0ul);
}

void InitializeLocalAPICTimer()
{
    logger->info("[+] Initialize Local APIC Timer\n");

    // タイマーマネージャーの生成
    // 本来は１サイクルが１msかかるように設定したい。
    // 今回はとりあえず0x100000にしている。
    timer_manager = new TimerManager(main_queue);
    timer_manager->Start(0x1000000); 
    uint64_t start = timer_manager->TotalCount();
    // 100ms待つ
    acpi::WaitMilliseconds(100);
    uint64_t end = timer_manager->TotalCount();
    uint64_t counts_per_ms = (end - start) / 100;
    printk("LAPIC Timer Counts %ld Per 1ms.\n", counts_per_ms);

    timer_manager->Stop();
    timer_manager->Start(counts_per_ms); // １チックにかかる時間を１msにする

}

extern "C" void LAPICTimerOnInterrupt(const TaskContext *ctx_stack)
{
    bool task_timer_timeout = timer_manager->Tick();
    NotifyEndOfInterrupt();

    if (task_timer_timeout) {
        // タスクの入れ替え処理を行う
        task_manager->SwitchTask(ctx_stack);
    }
}

 

TimerManager::TimerManager(std::deque<Message> *msg_queue) : tick_{0}
{
    // 一旦APICタイマを止めておく。
    *kInitialCountRegister = 0;

    // LVTの設定
    LVTTimerRegister lvt_timer;
    lvt_timer.data = 0;
    lvt_timer.bits.vector = InterruptVector::kLAPICTimer;
    lvt_timer.bits.timer_mode = 1;
    *kLVTTimerRegister = lvt_timer.data;
    logger->debug("Set %08xh to LVT Timer\n", *kLVTTimerRegister);

    // 分周比を１に設定
    *kDivideConfigurationRegister = 0b1011;
    logger->debug("Set %08xh to Divide Configuration Register\n", *kDivideConfigurationRegister);

    // 論理タイマーのキューに最大の大きさのタイマーを入れておく
    timers_.push(Timer(0xffffffffu, -1));
}

void TimerManager::Start(uint32_t counts_per_loop)
{
    counts_per_loop_ = counts_per_loop;
    *kInitialCountRegister = counts_per_loop_; // 書き込んだ時点からタイマーはカウントを始める。
}

void TimerManager::Stop()
{
    *kInitialCountRegister = 0;
}

void TimerManager::AddTimer(Timer timer)
{
    timers_.push(timer);
}

bool TimerManager::Tick()
{
    tick_++;
    bool task_timer_timeout = false;
    while (1) {
        Timer timer = timers_.top();
        if (timer.Timeout() > tick_)
            break;
        
        // タスク切り替えに関するタイマーの場合
        if (timer.Value() == kTaskTimerValue) {
            task_timer_timeout = true; // 返り値をtrueにセットする
            timers_.pop(); // Tick()は割り込み時に行われるメンバ関数なので、他の割り込みを想定しなくて良い。
            timers_.push(Timer(tick_ + kTaskTimerPeriod, kTaskTimerValue)); // 次のタイマーをセット
            continue;
        }
        // タイムアウト時刻になったタイマーについてのメッセージをキューに挿入する。
        Message msg;
        msg.type = Message::Type::kTimerTimeout;
        msg.arg.timer.timeout = timer.Timeout();
        msg.arg.timer.value = timer.Value();
        task_manager->SendMessage(1, msg); // メインタスクのタイマーはidが１

        timers_.pop();
    }

    return task_timer_timeout;
}

uint64_t TimerManager::TotalCount()
{
    uint32_t current_count = *kCurrentCountRegister;
    return \
        static_cast<uint64_t>(tick_) * static_cast<uint64_t>(counts_per_loop_) +\
        static_cast<uint64_t>(counts_per_loop_) - static_cast<uint64_t>(current_count);
}
