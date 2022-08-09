#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <deque>

#include "message.hpp"

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
    static const uint64_t kDefaultLevel = 1; // タスクの優先度レベルのデフォルト値

    Task(uint64_t id); 
    // コンテキストを０で初期化した後、関数fの実行に必要なレジスタの初期値を与える。
    Task *InitContext(TaskFunc *f, int64_t data);
    TaskContext *Context(); // 現在のコンテキストの構造体へのポインタを返す。

    uint64_t ID() const; // タスクのidを返す
    Task *Sleep(); // タスクをスリープする
    Task *Wakeup(int level = -1); // タスクを実行可能状態に遷移させる。優先度をlevelで指定できる。level<0の時、優先度は変えない。

    int Level() { return level_; } // このタスクの優先度
    bool Running() { return running_; } // 実行可能常態か？
    Task *SetLevel(int level);
    Task *SetRunning(bool running);
    uint64_t os_stack_pointer_; // アプリケーション実行後OSの処理に戻ってくる時に用いる
    uint64_t GetOSStackPointer() { return os_stack_pointer_; }

    void SendMessage(const Message msg); // このタスクの持つメッセージキューにプッシュし、実行可能状態へ遷移
    Message ReceiveMessage(); // メッセージキューからポップする。何も入っていない場合、kNullMessageタイプのメッセージを返す。
    
private:
    uint64_t id_; // タスク固有の値
    std::vector<uint64_t> stack_; // このタスクが使用するスタック領域。
    alignas(16) TaskContext context_; 
    std::deque<Message> msgs_;
    int level_{kDefaultLevel}; // 実行優先度レベル
    bool running_{false}; // 実行状態・実行可能状態の時にtrueになる
};


/* 
 * Taskクラスをまとめて管理するクラス
 * NewTask()で新しくタスクを生成する。
 * SwitchTask()でコンテキストスウィッチを行う。（タイマー割り込み上で呼ばれることを想定）
 */
class TaskManager
{
public:
    static const int kMaxLevel = 3; // 実行優先度の幅

    TaskManager(); // NewTask()を１回だけ実行する。
    Task *NewTask(); // 新しくタスクを追加（コンテキストの設定や実行可能状態への遷移は行わない）
    void SwitchTask(const TaskContext *current_ctx); // current_ctxを現在のタスクのコンテキストへ格納し、別の処理に制御を移す
    // 引数にtrueを渡せば現在実行中のタスクをスリープさせる。
    // 現在のcurrent_level_を必要があれば変更する。返り値は変更前の実行タスク。
    Task *RotateCurrentRunQueue(bool current_sleep); 

    void Sleep(Task *task);
    int Sleep(uint64_t id); // 成功したら０、失敗したら−１
    void Wakeup(Task *task, int level = -1); // 寝ていたら起こす。levelで実行優先度を変更できる。変更したくない場合はlevel<0とする
    int Wakeup(uint64_t id, int level = -1); // 成功したら０、失敗したら−１

    int SendMessage(uint64_t id, Message msg); // タスクidのメッセージキューにmsgをpushする。成功０、失敗−１
    Task *CurrentTask(); // 現在実行中のTaskオブジェクトへのポインタを返す
    int NumRunningTasks(); // runningのタスクの数を返す

private:
    std::vector<Task *> tasks_{}; // 作成したタスクを全て格納するもの
    uint64_t latest_id_{0}; // 次に作成するタスクのid
    std::array<std::deque<Task *>, kMaxLevel + 1> running_{}; // 実行可能状態タスクの配列。先頭のタスクが現在実行中。
    int current_level_{kMaxLevel}; // running_に入っているタスクの中で最高の優先度を保持する

    void ChangeLevelRunning(Task *task, int level); // 実行可能状態のtaskの優先度レベルを変更する
};


// カーネルのmain関数から呼ばれる初期化関数。
void InitializeTask();
