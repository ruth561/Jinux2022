#include <string.h>
#include <algorithm>

#include "task.hpp"
#include "asmfunc.h"
#include "segment.hpp"
#include "timer.hpp"
#include "logging.hpp"
#include "memory_manager.hpp"
extern BitmapMemoryManager* memory_manager;
extern TaskManager* task_manager;
extern TimerManager *timer_manager;
extern logging::Logger *logger;


Task::Task(uint64_t id) : id_{id} {}

Task *Task::InitContext(TaskFunc *f, int64_t data)
{
    const size_t stack_size = kDefultStackBytes / sizeof(stack_[0]);
    stack_.resize(stack_size);
    uint64_t stack_end = reinterpret_cast<uint64_t>(&stack_[stack_size]);

    memset(&context_, 0, sizeof(context_)); // コンテキストを０で初期化
    context_.cr3 = GetCR3(); // カーネルと同じページテーブルを使用する
    context_.rflags = 0x202; // 割り込みを許可する設定
    context_.cs = kKernelCS;
    context_.ss = kKernelSS;

    // １６バイトにアラインメントした後に８を引くことで、
    // １６バイト境界から８バイトずれたアドレスを指す。
    context_.rsp = (stack_end & ~0xflu) - 8; 
    context_.rip = reinterpret_cast<uint64_t>(f);
    context_.rdi = id_;
    context_.rsi = data;

      // MXCSR のすべての例外をマスクする
      // TODO: ここよく分からんが後で、、
    *reinterpret_cast<uint32_t*>(&context_.fxsave_area[24]) = 0x1f80;
    return this;
}

Task *Task::Sleep()
{
    task_manager->Sleep(this);
    return this;
}

Task *Task::Wakeup()
{
    task_manager->Wakeup(this);
    return this;
}

TaskManager::TaskManager()
{
    running_.push_back(NewTask()); // OS用のタスク？
}

Task *TaskManager::NewTask()
{
    latest_id_++;
    tasks_.push_back(new Task(latest_id_));
    return tasks_.back();
}

void TaskManager::SwitchTask(bool current_sleep)
{
    Task *current_task = running_.front();
    running_.pop_front();
    if (!current_sleep) {
        running_.push_back(current_task);
    }
    Task *next_task = running_.front();

    SwitchContext(next_task->Context(), current_task->Context());
}

void TaskManager::Sleep(Task *task)
{
    auto it = std::find(running_.begin(), running_.end(), task);
    if (it == running_.begin()) { // タスクが現在実行中なら
        SwitchTask(true);
        return;
    }

    if (it == running_.end()) { // タスクが実行可能状態でないなら
        return;
    }

    running_.erase(it); // 実行可能状態リストからタスクを削除
}

int TaskManager::Sleep(uint64_t id)
{
    auto it = std::find_if(tasks_.begin(), tasks_.end(), 
        [id](const auto& t){ return t->ID() == id; });
    if (it == tasks_.end()) {
        return -1;
    }

    Sleep(*it);
    return 0;
}

void TaskManager::Wakeup(Task *task) 
{
    auto it = std::find(running_.begin(), running_.end(), task);
    if (it == running_.end()) { // 現在実行可能状態でないならば
        running_.push_back(task);
    }
}

int TaskManager::Wakeup(uint64_t id) 
{
    auto it = std::find_if(tasks_.begin(), tasks_.end(), 
        [id](const auto& t){ return t->ID() == id; });
    if (it == tasks_.end()) {
        return -1;
    }

    Wakeup(*it);
    return 0;
}


void InitializeTask()
{
    task_manager = new TaskManager;

    __asm__("cli");
    timer_manager->AddTimer(
        Timer(timer_manager->CurrentTick() + kTaskTimerPeriod, kTaskTimerValue));
    __asm__("sti");
}
