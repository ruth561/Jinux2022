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

namespace {
    // アイドルタスク（ずっと何もせずCPUを休ませてくれるタスク）
    void IdleTask(uint64_t id, int64_t data)
    {
        while (1) __asm__("hlt");
    }

    // 第一引数で指定したdequeから要素valueを消去する関数（必ずしもdequeである必要はない）
    template <class T, class U>
    void Erase(T& c, const U& value) {
        auto it = std::remove(c.begin(), c.end(), value);
        c.erase(it, c.end());
    }
}

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

uint64_t Task::ID() const
{
    return id_;
}

Task *Task::Sleep()
{
    task_manager->Sleep(this);
    return this;
}

Task *Task::Wakeup(int level)
{
    task_manager->Wakeup(this, level);
    return this;
}

void Task::SendMessage(const Message msg)
{
    msgs_.push_back(msg);
    Wakeup();
}

Message Task::ReceiveMessage()
{
    Message msg;
    if (msgs_.empty()) {
        msg.type = Message::Type::kNullMessage;
    } else {
        msg = msgs_.front();
        msgs_.pop_front();
    }

    return msg;
}

Task *Task::SetLevel(int level)
{
    level_ = level;
    return this;
}

Task *Task::SetRunning(bool running)
{
    running_ = running;
    return this;
}


TaskManager::TaskManager()
{
    // OS用のタスクを最大優先度で現在実行中とする
    Task *task = NewTask()
        ->SetLevel(current_level_)
        ->SetRunning(true);
    running_[current_level_].push_back(task);

    // 何もしないタスクを入れておく
    Task *idle_task = NewTask()
        ->InitContext(IdleTask, 0)
        ->SetLevel(0)
        ->SetRunning(true);
    running_[0].push_back(idle_task);
}

Task *TaskManager::NewTask()
{
    latest_id_++;
    tasks_.push_back(new Task(latest_id_));
    return tasks_.back();
}

void TaskManager::SwitchTask(const TaskContext *current_ctx)
{
    TaskContext *current_task_ctx = task_manager->CurrentTask()->Context();
    memcpy(current_task_ctx, current_ctx, sizeof(TaskContext));

    Task *current_task = RotateCurrentRunQueue(false);
    if (CurrentTask() != current_task) { // 次に実行すべきタスクが変更している場合
        RestoreContext(CurrentTask()->Context());
    }
}

Task *TaskManager::RotateCurrentRunQueue(bool current_sleep)
{
    Task *current_task = running_[current_level_].front();
    running_[current_level_].pop_front();
    if (!current_sleep) {
        running_[current_level_].push_back(current_task);
    }

    // 優先度の高い方から順に溜まっているタスクを探し始める。
    for (uint64_t lv = kMaxLevel; lv >= 0; lv--) {
        if (!running_[lv].empty()) {
            current_level_ = lv;
            break;
        }
    }

    return current_task;
}

void TaskManager::Sleep(Task *task)
{
    if (!task->Running()) { // すでにスリープ状態の時
        return;
    }
    task->SetRunning(false); 

    if (task == running_[current_level_].front()) { // タスクが現在実行中なら
        Task* current_task = RotateCurrentRunQueue(true);
        SwitchContext(CurrentTask()->Context(), current_task->Context());
        return;
    }

    Erase(running_[task->Level()], task);
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

void TaskManager::Wakeup(Task *task, int level) 
{
    if (task->Running()) { // タスクがすでに起きているならlevelの変更だけを行う
        ChangeLevelRunning(task, level);
        return;
    }
    // タスクがスリープ状態の時
    if (level < 0) { // levelが負の時は値を変えない
        level = task->Level();
    }

    task->SetLevel(level);
    task->SetRunning(true);

    running_[level].push_back(task); // 新しいレベルのrunningキューにプッシュする
    return;
}

int TaskManager::Wakeup(uint64_t id, int level) 
{
    auto it = std::find_if(tasks_.begin(), tasks_.end(), 
        [id](const auto& t){ return t->ID() == id; });
    if (it == tasks_.end()) {
        return -1;
    }

    Wakeup(*it, level);
    return 0;
}

int TaskManager::SendMessage(uint64_t id, Message msg)
{
    auto it = std::find_if(tasks_.begin(), tasks_.end(), 
        [id](const auto& t){ return t->ID() == id; });
    if (it == tasks_.end()) { // タスクが存在しない場合
        return -1;
    }

    (*it)->SendMessage(msg);
    return 0;
}

Task *TaskManager::CurrentTask()
{
    return running_[current_level_].front();
}

int TaskManager::NumRunningTasks()
{
    int res = 0;
    for (int i = 0; i < kMaxLevel + 1; i++) {
        res += running_[i].size();
    }
    return res;
}

void TaskManager::ChangeLevelRunning(Task *task, int level)
{
    if (level < 0 || level == task->Level()) { // levelが変わっていない場合や変更しない場合は無視する
        return;
    }

    // level変更有りの時
    if (task != running_[current_level_].front()) { // 現在実行中ではないならば
        Erase(running_[task->Level()], task);
        running_[level].push_back(task);
        task->SetLevel(level);
        return;
    }

    // 実行状態の場合
    // このプログラムを実行しているタスクの優先度を変更するので
    // 変更した先でも実行中でなければならない。
    running_[current_level_].pop_front();
    running_[level].push_front(task);
    task->SetLevel(level);
    current_level_ = level;
}


void InitializeTask()
{
    task_manager = new TaskManager;

    __asm__("cli");
    timer_manager->AddTimer(
        Timer(timer_manager->CurrentTick() + kTaskTimerPeriod, kTaskTimerValue));
    __asm__("sti");
}
