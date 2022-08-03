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

    void *Ring::Buffer() { 
        return ring_buf_;
    }

    TRB *Ring::Push_(TRB *trb)
    {
        // エンキューポインタの指す場所は、LinkTRBではないことが保証されている。
        // TODO: Pushする時、Ringが満杯であるかどうかの確認は必要ないのか？？
 
        TRB *trb_ptr = enqueue_pointer_;
        for (int i = 0; i < 4; i++) {
            enqueue_pointer_->data[i] = trb->data[i]; 
        }
        enqueue_pointer_->bits.cycle_bit = cycle_bit_;
        logger->debug("PushTRB %p: %08x %08x %08x %08x\n", 
            enqueue_pointer_, 
            enqueue_pointer_->data[0], 
            enqueue_pointer_->data[1],  
            enqueue_pointer_->data[2], 
            enqueue_pointer_->data[3]);
        enqueue_pointer_++;

        if (enqueue_pointer_->bits.trb_type == 6) { // LinkTRBに達した場合
            logger->debug("[Ring::Push] Enqueue Pointer Reached To Link TRB.\n");
            LinkTRB *link_trb = reinterpret_cast<LinkTRB *>(enqueue_pointer_);
            link_trb->bits.cycle_bit = cycle_bit_;
            enqueue_pointer_ = reinterpret_cast<TRB *>(link_trb->Pointer());

            if (link_trb->bits.toggle_cycle) { // サイクルビットを反転する必要がある場合
                cycle_bit_ = !cycle_bit_;
            }
        }

        return trb_ptr;
    }










    

    int EventRing::Initialize(uint32_t ring_size, struct InterrupterRegisterSet *interrupter)
    {
        logger->debug("[+] Initialize Event Ring\n");
        cycle_bit_ = true;

        // リング領域の確保＆０クリア
        ring_size_ = ring_size;
        ring_ = reinterpret_cast<TRB *>(usb::AllocMem(ring_size_ * sizeof(TRB), 64, 64 * 1024));
        memset(ring_, 0, ring_size_ * sizeof(TRB));
        logger->debug("Event Ring Segment: %p\n", ring_);

        //  イベントリングセグメントテーブルの領域を確保する。
        erst_ = reinterpret_cast<EventRingSegmentTableEntry *>(usb::AllocMem(sizeof(EventRingSegmentTableEntry), 64, 64 * 1024));
        memset(erst_, 0, sizeof(EventRingSegmentTableEntry));
        logger->debug("Event Ring Segment Table: %p\n", erst_);

        erst_[0].ring_segment_size = ring_size_;
        erst_[0].ring_segment_base_address = reinterpret_cast<uint64_t>(ring_);

        interrupter_ = interrupter;
        interrupter_->ERSTSZ.SetSize(1); // セグメントサイズは１
        interrupter_->ERDP.SetPointer(ring_);
        interrupter_->ERSTBA.SetPointer(erst_);
        
        return 0;
    }

    TRB *EventRing::ReadDequeuePointer() const
    {
        return reinterpret_cast<TRB *>(interrupter_->ERDP.Pointer());
    }

    void EventRing::WriteDequeuePointer(TRB *ptr)
    {
        interrupter_->ERDP.SetPointer(ptr);
    }

    void EventRing::Pop()
    {
        TRB *trb_ptr = ReadDequeuePointer() + 1;
        logger->debug("[EventRing::Pop] Dequeue Pointer: %p\n", trb_ptr);

        TRB* segment_begin = reinterpret_cast<TRB *>(erst_[0].ring_segment_base_address);
        TRB* segment_end = segment_begin + erst_[0].ring_segment_size;

        if (trb_ptr == segment_end) { // セグメントの終端へ達した場合
            logger->debug("[EventRing::Pop] Dequeue Pointer Reached To The End Of Event Ring Segment.\n");
            trb_ptr = segment_begin;
            cycle_bit_ = !cycle_bit_;
        }

        WriteDequeuePointer(trb_ptr);
    }




}  