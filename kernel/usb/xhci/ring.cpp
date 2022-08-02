#include <cstring>

#include "ring.hpp"  
#include "../../logging.hpp"
extern logging::Logger *logger;
int printk(const char *format, ...);



namespace usb::xhci
{
    
    int Ring::Initialize(uint32_t ring_size)
    {
        logger->debug("[+] Ring Initialize\n");
        ring_size_ = ring_size;
        
        //  リングのアラインメント制約と境界制約を守りメモリを確保
        ring_buf_ = (TRB *) usb::AllocMem(sizeof(TRB) * ring_size_, 64, 64 * 1024);
        enqueue_pointer_ = ring_buf_;
        logger->debug("Ring.ring_buf_: %p\n", ring_buf_);

        //  リング全体を０クリア
        memset(ring_buf_, 0, ring_size_ * sizeof(TRB));

        cycle_bit_ = true;

        //  リングの最後にリンクTRBを配置
        LinkTRB link_trb(ring_buf_);
        link_trb.bits.toggle_cycle = 1;
        TRB *dst_trb = ring_buf_ + ring_size_ - 1;
        for (int i = 0; i < 4; i++) { // 最後尾にコピー
            dst_trb->data[i] = link_trb.data[i];
        }
        logger->debug("LinkTRB: %08x %08x %08x %08x\n", 
            link_trb.data[0], link_trb.data[1], link_trb.data[2], link_trb.data[3]);
        return 0;
    }

    template <typename TRBType>
    TRB *Ring::Push(TRBType *trb)
    {
        //  エンキューポインタの指す場所は、LinkTRBではないことが保証されている。

        TRB *trb_ptr = enqueue_pointer_;
        for (int i = 0; i < 4; i++) {
            enqueue_pointer_->data[i] = trb->data[i];
        }
        enqueue_pointer_->cycle_bit = cycle_bit_;
        logger->debug("PushTRB %p: %08x %08x %08x %08x\n", 
            enqueue_pointer_, 
            enqueue_pointer_->data[0], 
            enqueue_pointer_->data[1], 
            enqueue_pointer_->data[2], 
            enqueue_pointer_->data[3]);
        enqueue_pointer_++;

        if (enqueue_pointer_->trb_type == 6) { // LinkTRBに達した場合
            LinkTRB *link_trb = reinterpret_cast<LinkTRB *>(enqueue_pointer_);
            link_trb->cycle_bit = cycle_bit_;
            enqueue_pointer_ = reinterpret_cast<TRB *>(link_trb->Pointer());

            if (link_trb->toggle_cycle) { // サイクルビットを反転する必要がある場合
                cycle_bit_ = !cycle_bit_;
            }
        }

        return trb_ptr;
    }

    void *Ring::Buffer() {
        return ring_buf_;
    }










    

    int EventRing::Initialize(uint32_t ring_size, struct InterrupterRegisterSet *interrupter)
    {
        cycle_bit_ = true;

        //  リングの大きさと領域を確保する
        ring_size_ = ring_size;
        ring_ = (TRB *) usb::AllocMem(sizeof(TRB) * ring_size_, 64, 64 * 1024);

        //  リング全体を０クリアする。
        char *ptr = (char *) ring_;
        for (int i = 0; i < ring_size_ * sizeof(TRB); i++) *(ptr++) = 0;
        
        //  イベントリングセグメントテーブルの領域を確保する。
        erst_ = (EventRingSegmentTableEntry *) usb::AllocMem(sizeof(EventRingSegmentTableEntry), 64, 64 * 1024);
        
        //  ０クリアする。
        ptr = (char *) erst_;
        for (int i = 0; i < sizeof(EventRingSegmentTableEntry); i++) *(ptr++) = 0;

        erst_[0].ring_segment_size = ring_size_;
        erst_[0].ring_segment_base_address = (uint64_t) ring_ >> 6;



       
        //  ランタイムレジスタのERSTSZに１を格納する。
        //  ランタイムレジスタのERDPに１つ目のセグメントの先頭ポインタを渡す。
        //  ランタイムレジスタのERSTAにerst_のアドレスを渡す。
        interrupter_ = interrupter;
        interrupter_->ERSTSZ.event_ring_segment_table_size = 1;
        interrupter_->ERDP.event_ring_dequeue_pointer = (uint64_t) ring_ >> 4;
        interrupter_->ERSTBA.event_ring_segment_table_base_address = (uint64_t) erst_ >> 6;
        
        return 0;
    }

    void EventRing::Pop()
    {
        TRB *trb_ptr = ReadDequeuePointer() + 1;
        //  printk("[DEBUG EventRing::Pop] Deq ptr: %lx\n", trb_ptr);

        TRB* segment_begin = (TRB *) (erst_[0].ring_segment_base_address << 6);
        TRB* segment_end = segment_begin + erst_[0].ring_segment_size;

        //  次のデキューポインタがLink TRBの場合
        /* printk("[DEBUG] segment_begin: %lx  segment_end: %lx\n", segment_begin, segment_end);
        printk("[DEBUG] TRB(%lx) TRB Type is %s\n", trb_ptr, kTRBTypeToName[trb_ptr->trb_type]); */
        if (trb_ptr == segment_end) {
            printk("[EventRing::Pop] cycle_bit %d ->", cycle_bit_);
            trb_ptr = segment_begin;
            cycle_bit_ = !cycle_bit_;
            printk("%d\n", cycle_bit_);
        }
        
        event_count++;

        interrupter_->ERDP.event_ring_dequeue_pointer = (uint64_t) trb_ptr >> 4;
    }

    TRB *EventRing::ReadDequeuePointer()
    {
        return (TRB *) (interrupter_->ERDP.event_ring_dequeue_pointer << 4);
    }


}