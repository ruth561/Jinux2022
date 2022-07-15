#pragma once
#include <queue>

#include "interrupt.hpp"
#include "message.hpp"

// lvt timer registerのレイアウト
// 書き込みは32bitで一気にする必要がある。
union LVTTimerRegister
{
    uint32_t data;
    struct {
        uint8_t vector; // 割り込みベクタ番号
        uint8_t : 4;
        uint8_t delivery_status : 1; // １：割り込みが伝達中
        uint8_t : 3;
        uint8_t masked : 1; // １：割り込みを実施しない
        uint8_t timer_mode : 2; // ００：One-shot、０１：Periodic
        uint8_t : 5;
        uint8_t : 8;
    } __attribute__((packed)) bits;
};



void InitializeLocalAPICTimer();


// TimerManagerで管理される論理タイマー
// 
// ＜引数の説明＞
// timeout：　通知を返してほしい時刻を指定する。時刻の単位は、１tick（TimerManagerの）である。
// value：　通知の時に一緒に伝達される値。タイマーの識別なんかに使う。PIDとか？
class Timer
{
public:
    Timer(uint32_t timeout, int value) :
        timeout_{timeout}, value_{value} {};
    uint32_t Timeout() const { return timeout_; }
    int Value() { return value_; }

private:
    uint32_t timeout_;
    int value_;
};

// タイマー同士での比較演算をできるようにするオペレーター
inline bool operator<(const Timer& lhs, const Timer& rhs) {
    return lhs.Timeout() > rhs.Timeout();
}


// Local APIC Timerを使って複数の論理タイマーを制御するためのクラス。
// 一つの物理タイマーを連続して何回も回して、その間に論理的なタイマーを可動させる。
// ＜引数の説明＞
// msg_queue: 論理タイマーがタイムアウトした旨をMessage構造体にしてpushする先のキュー。（カーネルのもの）
// counts_per_loop: １ループあたりのカウント数を指定する。
// 
// ＜メンバ関数の説明＞
// Start()：　物理タイマーを開始させる。これをしないと時刻の計測はできない。
// AddTimer()：　論理タイマーの追加。追加しておけば、指定した時刻にmsg_queueに通知が行く。
// Tick()：　物理タイマーの割り込み時に実行される関数。物理タイマーのループ数をインクリメントする
//         だけでなく、論理タイマーがタイムアウトしていないかのチェックもこの中で行う。
//         割り込みハンドラ内で実行されるので、軽めの実装を心がけるべきである。
// CurrentTick()：　現在のループ数を返す。
// TotalCount()：　物理タイマーが計測を開始してから経過した時間を返す。
//
class TimerManager
{
public:
    // 各種レジスタの設定をする。
    //
    // counts_per_loop: １ループあたりのカウント数を指定する
    TimerManager(std::deque<Message> *msg_queue, 
                 uint32_t counts_per_loop);

    // 物理タイマーの開始
    void Start();

    // 論理タイマーの追加
    void AddTimer(Timer timer);

    // ループした回数をインクリメント。
    void Tick();

    // 現在のループ回数を返す。
    uint32_t CurrentTick() { return tick_; }

    // 開始してからの総カウント数を返す。
    uint64_t TotalCount();  

private:
    uint32_t tick_; // ループした回数を保持
    uint32_t counts_per_loop_; // １ループのカウント数

    std::priority_queue<Timer> timers_; // タイマーを保管する優先度付きキュー
    std::deque<Message> *msg_queue_; // メッセージを入れているデック（main_queueへのポインタを入れる）
};
