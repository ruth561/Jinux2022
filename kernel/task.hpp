#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <cstddef>


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

protected:
    uint64_t id_; // タスク固有の値
    std::vector<uint64_t> stack_; // このタスクが使用するスタック領域。
    alignas(16) TaskContext context_; 
};

/* 
 * 特権レベル０で実行されるタスク
 */
class KernelTask : public Task
{
public:
    using Task::Task; // コンストラクタの継承
};


/* 
 * 特権レベル３で実行されるタスク
 */
class UserTask : public Task
{
public:
    using Task::Task; // コンストラクタの継承
    Task *InitContext(TaskFunc *f, int64_t data);
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
    /* 
     * 新しくタスクを追加
     * u_s：UserTaskかKernelTaskか。trueのならUserTask。
     */
    Task *NewTask(bool u_s);
    // コンテキストスウィッチング
    void SwitchTask(); 
private:
    std::vector<Task *> tasks_{};
    uint64_t latest_id_{0};
    size_t current_task_index_{0};
};


// カーネルのmain関数から呼ばれる初期化関数。
void InitializeTask();
