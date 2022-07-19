#include <string.h>

#include "task.hpp"
#include "asmfunc.h"
#include "segment.hpp"
#include "timer.hpp"


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

TaskContext *Task::Context()
{
    return &context_;
}


TaskManager::TaskManager()
{
    NewTask(); // OS用のタスク？
}

Task *TaskManager::NewTask()
{
    latest_id_++;
    tasks_.push_back(new Task(latest_id_));
    return tasks_.back();
}

void TaskManager::SwitchTask()
{
    size_t next_task_index = current_task_index_ + 1;
    if (next_task_index >= tasks_.size()) {
        next_task_index = 0;
    }

    Task *current_task = tasks_[current_task_index_];
    Task *next_task = tasks_[next_task_index];
    current_task_index_ = next_task_index;

    SwitchContext(next_task->Context(), current_task->Context());
}


extern TaskManager* task_manager;
extern TimerManager *timer_manager;

void InitializeTask()
{
    task_manager = new TaskManager;

    __asm__("cli");
    timer_manager->AddTimer(
        Timer(timer_manager->CurrentTick() + kTaskTimerPeriod, kTaskTimerValue));
    __asm__("sti");
}
