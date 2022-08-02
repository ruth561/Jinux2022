#pragma once

#include <stdint.h>

#include "trb.hpp" 
#include "registers.hpp"
#include "../memory.hpp" 
 
namespace usb::xhci
{
    //  Ringの大きさに応じてmallocするのを実装するのはめんどくさいので、
    //  予め最大数を定めておき、classの中で確保しておく。
    // const uint32_t kMaxRingSize = 100;
    const uint32_t kEventRingSegmentTableSize = 1;


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
        template <typename TRBType>
        TRB *Push(TRBType *trb);

        //  リングバッファの先頭アドレス
        void *Buffer();
    
    private:
        bool cycle_bit_;        //  プロデューササイクルステート
        TRB *enqueue_pointer_;  //  次、TRBを挿入する位置  

        uint32_t ring_size_;    //  Ringに収まるTRBの数
        TRB *ring_buf_;         //  ringのメモリ領域の先頭アドレスを定義

        //  trb_ptrで指されたポインタから１６バイト分を読み込み、
        //  エンキューポインタの指す場所へコピーする。
        TRB *Push16(char *trb_ptr);
    };








    //  イベントリングセグメントテーブルのエントリ。
    //  イベントリングセグメントテーブルとは、複数のイベントリングの
    //  セグメントをまとめるテーブルのこと。
    //  ランタイムレジスタのERSTAから参照される。
    //  簡単のため、ERSTSZは１とする。
    struct EventRingSegmentTableEntry
    {
        uint64_t : 6;   // reserved
        //  イベントリングセグメントのベースアドレスを指すポインタ。
        //  イベントリングセグメントは、６４バイトでアラインメントされているので、
        //  使うときには、左に６ビットシフトする必要がある。
        //  格納するときには、右に６ビットシフトする。
        uint64_t ring_segment_base_address : 58;
        //  イベントリングのサイズ（格納できるTRBの数）。
        //  この値は、１６以上４０９６未満である必要がある。
        uint64_t ring_segment_size : 16;
        uint64_t : 48;    // reserved
    };
    

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


        //  デキューポインタが指しているTRBのサイクルビットが自身の状態と等しい時はtrue
        //  を返す関数。サイクルビットが等しい時、それはまだ読み出していないTRBであることを示唆する。
        bool HasFront()
        {
            // return ReadDequeuePointer()->cycle_bit == cycle_bit_;
            return 1;
        }

        //  次のTRBへのポインタを返す関数。
        TRB *Front()
        {
            return ReadDequeuePointer();
        }

        //  デキューポインタを一つ進める。
        //  取り出した値を返したりなどはしない。ただ、進めるだけである。
        //  動作が終われば、デキューポインタの値をxHCのレジスタに記録する。
        //  リングセグメントの最後尾に到達したら、先頭に戻ってくる。
        //  最後尾の判定は、Segment Sizeでも可能であるが、xHCがLink TRBをおいてくれるはずなので、
        //  それに従っても良い。
        void Pop();

        //  現在のデキューポインタの値を取り出し、TRBのポインタにキャストして返す関数。
        TRB* ReadDequeuePointer();
    
    private:
        bool cycle_bit_;

        //  ただ一つのイベントリングセグメントのベースアドレス
        TRB *ring_;
        //  リングセグメントの大きさ（kMaxRingSize以下である必要がある）。
        uint32_t ring_size_;

        EventRingSegmentTableEntry *erst_;

        //  ランタイムレジスタ内のIRの先頭ポインタ
        struct InterrupterRegisterSet *interrupter_;

        //  イベントTRBを読み込んだ回数を記録する。デバッグ用。
        int event_count = 0;

    };

}
