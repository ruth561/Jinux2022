#pragma once

#include <stdint.h>

#include "trb.hpp" 
#include "registers.hpp"
#include "../memory.hpp" 
 
namespace usb::xhci 
{    
    //  Command/Transfer Ringのクラス
    //  サイクルビットやエンキューポインタの値を保持するため、
    //  リングにTRBを挿入する時に、それらを気にする必要はない。
    class Ring
    {
    public:
        //  引数の値は、100以下にする。（kMaxRingSizeで定義してある）
        //  まずRingの領域を確保し、先頭ポインタをenqueue_pointer_に格納する。
        //  その後、Ring全体を０で初期化する。
        //  cycle_bit_を１に初期化する。
        //  リングの最後にリンクTRBを配置する。
        int Initialize(uint32_t ring_size);

        //  TRBをエンキューポインタの指す位置に挿入する。
        //  大きさが１６バイトのTRBであれば、引数には好きなTRBを渡すことができる。
        //  例えば、NormalTRB, NoOpCommandTRB, etc..
        //  挿入後、エンキューポインタを一つ進める。もし、進めた先がLinkTRBなら、
        //  LinkTRBの指すアドレスへ、エンキューポインタを進める。
        //  LinkTRBのtoggle_bitが１を示していたら、cycle_bit_も内部で変更しておく。
        //  サイクルビットを指定する必要はない。
        template <typename TRBType> // templateはヘッダファイルに実装しなければならない
        TRB *Push(TRBType *trb) {
            return Push_(reinterpret_cast<TRB *>(trb));
        }

        TRB *Buffer(); //  リングバッファの先頭アドレス
    
    private:
        bool cycle_bit_;        //  プロデューササイクルステート
        TRB *enqueue_pointer_;  //  次、TRBを挿入する位置  

        uint32_t ring_size_;    //  Ringに収まるTRBの数
        TRB *ring_buf_;         //  ringのメモリ領域の先頭アドレスを定義

        TRB *Push_(TRB *trb); // TRBをリングへPushする
    };








    //  イベントリングセグメントテーブルのエントリ。
    //  イベントリングセグメントテーブルとは、複数のイベントリングの
    //  セグメントをまとめるテーブルのこと。
    //  ランタイムレジスタのERSTAから参照される。
    //  簡単のため、ERSTSZは１とする。
    struct EventRingSegmentTableEntry
    {
        uint64_t ring_segment_base_address; // ６４バイトアラインメント
        uint64_t ring_segment_size : 16;
        uint64_t : 48;
    } __attribute__((packed));
    
    //  Ringの大きさに応じてmallocするのを実装するのはめんどくさいので、
    //  予め最大数を定めておき、classの中で確保しておく。
    // const uint32_t kMaxRingSize = 100;
    const uint32_t kEventRingSegmentTableSize = 1;

    //  注意点
    //  Popという関数があるが、これはデキューポインタを１進めるだけの命令であり、
    //  現在のデキューポインタの位置にあるTRBを読み取るメンバ関数は別にある。
    //  このPopを実行しないと、デキューポインタは一向に進まないので、注意する。
    class EventRing
    {
    public:
        //  ring_sizeには、イベントリングセグメントの大きさを渡す。
        //  interruperには、このイベントリングと関連する
        //  ランタイムレジスタのインタラプタへのポインタを渡す。
        //  今回は、プライマリ・インタラプタだけを使うので、
        //  RuntimeRegisters->IP[0]へのポインタを渡せば良い。
        int Initialize(uint32_t ring_size, struct InterrupterRegisterSet *interrupter);

        //  デキューポインタが指しているTRBのサイクルビットが自身の状態と等しい時はtrueを返す。
        //  サイクルビットが等しい時、それはまだ読み出していないTRBであることを示唆する。
        bool HasFront() const {
            return ReadDequeuePointer()->bits.cycle_bit == cycle_bit_;
        }

        TRB *Front();

        //  デキューポインタを一つ進める。
        //  取り出した値を返したりなどはしない。ただ、進めるだけである。
        //  動作が終われば、デキューポインタの値をxHCのレジスタに記録する。
        //  リングセグメントの最後尾に到達したら、先頭に戻ってくる。
        //  最後尾の判定は、Segment Sizeでも可能であるが、xHCがLink TRBをおいてくれるはずなので、
        //  それに従っても良い。
        void Pop();
    
    private:
        bool cycle_bit_;
        TRB *ring_; // Event Ring Segmentのアドレス。簡単のため１個しか持たない。
        uint32_t ring_size_; // Event Ring Segment Size
        EventRingSegmentTableEntry *erst_; // Event Ring Segment Table
        struct InterrupterRegisterSet *interrupter_; //  ランタイムレジスタ内のIRの先頭ポインタ

        TRB* ReadDequeuePointer() const; //  現在のデキューポインタを返す。

        void WriteDequeuePointer(TRB *ptr); // デキューポインタの値を更新する。
    };

}
