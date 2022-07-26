#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <deque>


/* 
 * CPUのレジスタを格納する構造体。
 * taskのコンテキストのこと。
 */
struct TaskContext {
    uint64_t cr3, rip, rflags, reserved1; // offset 0x00
    uint64_t cs, ss, fs, gs; // offset 0x20
    uint64_t rax, rbx, rcx, rdx, rdi, rsi, rsp, rbp; // offset 0x40
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15; // offset 0x80
    std::array<uint8_t, 512> fxsave_area; // offset 0xc0
} __attribute__((packed));


/* 
 * Taskが実行する関数の型のusing宣言 
 * 第１引数はタスクのidを渡す。
 * 代２引数には初期化時に渡されるデータを渡す。
 */
using TaskFunc = void (uint64_t, int64_t);

/* 
 * マルチタスクを実現する上で１つのタスクを表すクラス
 * 実行する関数やスタック領域などを個別に持つ。
 * タスクがどの特権レベルで実行されるかに応じて派生クラスを持つ。
 */
class Task
{
public:
    static const size_t kDefultStackBytes = 4096;

    Task(uint64_t id); 
    // コンテキストを０で初期化した後、関数fの実行に必要なレジスタの初期値を与える。
    virtual Task *InitContext(TaskFunc *f, int64_t data);
    // 現在のコンテキストの構造体へのポインタを返す。
    TaskContext *Context() { return &context_; }

    uint64_t ID() const { return id_; }
    Task *Sleep();
    Task *Wakeup();

protected:
    uint64_t id_; // タスク固有の値
    std::vector<uint64_t> stack_; // このタスクが使用するスタック領域。
    alignas(16) TaskContext context_; 
};


/* 
 * Taskクラスをまとめて管理するクラス
 * NewTask()で新しくタスクを生成する。
 * SwitchTask()でコンテキストスウィッチを行う。（タイマー割り込み上で呼ばれることを想定）
 */
class TaskManager
{
public:
    // NewTask()を１回だけ実行する。
    TaskManager();
    // 新しくタスクを追加
    Task *NewTask();
    // running_の先頭をポップし次の先頭のタスクとコンテキストスウィッチする
    void SwitchTask(bool current_sleep = false); 

    void Sleep(Task *task);
    int Sleep(uint64_t id); // 成功したら０、失敗したら−１
    void Wakeup(Task *task);
    int Wakeup(uint64_t id); // 成功したら０、失敗したら−１

private:
    std::vector<Task *> tasks_{}; // 作成したタスクを全て格納するもの
    uint64_t latest_id_{0}; // 次に作成するタスクのid
    std::deque<Task *> running_{}; // 実行可能状態タスクの配列。先頭のタスクが現在実行中。
};


// カーネルのmain関数から呼ばれる初期化関数。
void InitializeTask();
