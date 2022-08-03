#pragma once

#include <stdint.h>
#include <array>

#include "context.hpp" 
#include "ring.hpp"

namespace usb::xhci
{

/////////////////////// Capability Registers ///////////////////////
    union HCSPARAMS1_Bitmap
    {
        uint32_t data[1];
        struct {
            volatile uint32_t max_device_slots : 8;
            volatile uint32_t max_interrupters : 11;
            uint32_t : 5;   // reserved
            volatile uint32_t max_ports : 8;
        } __attribute__((packed)) bits;
    } __attribute__((packed));
    union HCSPARAMS2_Bitmap
    {
        uint32_t data[1];
        struct {
            volatile uint32_t isochronous_scheduling_threshold : 4;
            volatile uint32_t event_ring_segment_table_max : 4;
            uint32_t : 13;
            volatile uint32_t max_scratchpad_buffers_high : 5;
            uint32_t : 1;
            volatile uint32_t max_scratchpad_buffers_low : 5;
        } __attribute__((packed)) bits;
    } __attribute__((packed));
    union HCSPARAMS3_Bitmap
    {
        uint32_t data[1];
        struct {
            volatile uint32_t u1_device_eixt_latency : 8;
            uint32_t : 8;
            volatile uint32_t u2_device_eixt_latency : 16;
        } __attribute__((packed)) bits;
    } __attribute__((packed));
    union DBOFF_Bitmap
    {
        uint32_t data[1];
        struct {
            uint32_t : 2;
            volatile uint32_t doorbell_array_offset : 30;
        } __attribute__((packed)) bits;

        uint32_t Offset() const { return bits.doorbell_array_offset << 2; }
    } __attribute__((packed));
    union RTSOFF_Bitmap
    {
        uint32_t data[1];
        struct {
            uint32_t : 5;
            volatile uint32_t runtime_register_space_offset : 27;
        } __attribute__((packed)) bits;

        uint32_t Offset() const { return bits.runtime_register_space_offset << 5; }
    } __attribute__((packed));
    struct CapabilityRegisters
    {
        uint8_t CAPLENGTH; // RO
        uint8_t reserved1;
        uint16_t HCIVERSION; // RO
        HCSPARAMS1_Bitmap HCSPARAMS1; // RO
        HCSPARAMS2_Bitmap HCSPARAMS2; // RO
        HCSPARAMS3_Bitmap HCSPARAMS3; // RO
        uint32_t HCCPARAMS1; // RO
        DBOFF_Bitmap DBOFF; // RO
        RTSOFF_Bitmap RTSOFF; // RO
        uint32_t HCCPARAMS2; // RO
    } __attribute__((packed));


/////////////////////// Operational Registers ///////////////////////
    union USBCMD_Bitmap
    {
        uint32_t data[1];
        struct {
            //  １:run ０:stop
            //  ソフトウェアによってこの値が0になった時、
            //  現在のコマンドとTDsを全て消化した後に、停止する。
            //  停止が完了したら、CMDSTS.host_controller_haltedは１にセットされる。
            //  ソフトウェアは、このbitが０クリアされる前に、以下のことを確認しなければならない。
            //      ・全てのエンドポイントは、StoppedかIdle状態でなければならない。
            //      ・全てのCommand Transfer Ringは、StoppedかIdle状態でなければならない。
            volatile uint32_t run_stop : 1;
            //  １になると、ハードウェア的にリセットが行われる。
            //  全てのOprationalRegistersが初期化される。
            //  リセット処理が完了したときに、xHCによって0クリアされる。
            //  ソフトウェアがこのbitを0クリアすることでリセット処理を終了することはできない。
            //  また、このbitが立っている間は、Operational/Runtime Registersへの
            //  アクセスは行ってはならない。
            //  USBSTS.host_controller_haltedが０の時に、このbitを1にすることはできない。
            volatile uint32_t host_controller_reset : 1;
            volatile uint32_t interrupter_enable : 1;
            volatile uint32_t host_system_error_enable : 1;
            uint32_t : 3;
            volatile uint32_t lignt_host_controller_reset : 1;
            volatile uint32_t controller_save_state : 1;
            volatile uint32_t controller_restore_state : 1;
            volatile uint32_t enable_wrap_event : 1;
            volatile uint32_t enable_u3_mfindex_stop : 1;
            volatile uint32_t stopped_short_packet_enable : 1;
            volatile uint32_t cem_enable : 1;
            uint32_t : 18;
        } __attribute__((packed)) bits;
    } __attribute__((packed));
    union USBSTS_Bitmap
    {
        // RW1C属性が混ざっているので注意が必要
        // この属性は、Read時にはその値を返すが、
        // １の書き込みには０クリア
        // ０の書き込みには無視、という動作が起きる
        uint32_t data[1];
        struct {
            //  default:1
            //  USBCMD.run_stopが１の時、この値は必ず０である。
            //  USBCMD.run_stopが０になることで実行が停止した後に、この値は１に設定される。
            volatile uint32_t host_controller_halted : 1;
            uint32_t : 1;
            volatile uint32_t host_system_error : 1; // RW1C
            volatile uint32_t event_interrupt : 1; // RW1C
            volatile uint32_t port_change_detect : 1; // RW1C
            uint32_t : 3;
            volatile uint32_t save_state_status : 1;
            volatile uint32_t restore_state_status : 1;
            volatile uint32_t save_restore_error : 1; // RW1C
            volatile uint32_t controller_not_ready : 1;
            volatile uint32_t host_controller_error : 1;
            uint32_t : 19;
        } __attribute__((packed)) bits;
    } __attribute__((packed));
    union CRCR_Bitmap
    {
        uint64_t data[1];
        struct {
            volatile uint64_t ring_cycle_state : 1;
            volatile uint64_t command_stop : 1; // RW1S
            volatile uint64_t command_abort : 1; // RW1S
            volatile uint64_t command_ring_running : 1; // RO
            uint64_t : 2;
            volatile uint64_t command_ring_pointer : 58;
        } __attribute__((packed)) bits;

        void *Pointer() const {
            return reinterpret_cast<void *>(bits.command_ring_pointer << 6);
        }

        void SetPointer(void *p) {
            bits.command_ring_pointer = reinterpret_cast<uint64_t>(p) >> 6;
        }
    } __attribute__((packed));
    union DCBAAP_Bitmap
    {
        uint64_t data[1];
        struct {
            uint64_t : 6;
            volatile uint64_t device_context_base_address_array_pointer : 26;
        } __attribute__((packed)) bits;

        void *Pointer() const {
            return reinterpret_cast<void *>(bits.device_context_base_address_array_pointer << 6);
        }

        void SetPointer(void *p) {
            bits.device_context_base_address_array_pointer = reinterpret_cast<uint64_t>(p) >> 6;
        }
    } __attribute__((packed));
    union CONFIG_Bitmap
    {
        uint32_t data[1];
        struct {
            volatile uint32_t max_device_slots_enabled : 8;
            volatile uint32_t u3_entry_enable : 1;
            volatile uint32_t configuration_information_enable : 1;
            uint32_t : 22;
        } __attribute__((packed)) bits;
    } __attribute__((packed));
    struct OperationalRegisters
    {
        USBCMD_Bitmap USBCMD; // RO, RW
        USBSTS_Bitmap USBSTS; // RO, RW1C
        uint32_t PAGESIZE; // RO
        uint32_t reserved1[2];
        uint32_t DNCTRL; // RW
        CRCR_Bitmap CRCR; // RO, RW, RW1S
        uint32_t reserved2[4];
        DCBAAP_Bitmap DCBAAP; // RW
        CONFIG_Bitmap CONFIG; // RW
    };


/* ==================== Port Status and Control Register ==================== */

    struct PORTSC_Bitmap
    {
        //  default: 0
        //  この値が１の時、デバイスがこのポートに接続されていることを示す。
        //  port_powerが０の時、この値は必ず０になる。
        volatile uint32_t current_connect_status : 1;
        //  default: 0
        //  １の時有効、０の時無効を表す。
        //  ポートを有効にできるのはxHCだけである。（ソフトウェアがこの値を１にすることはできない。）
        volatile uint32_t port_enabled_disabled : 1;
        uint32_t : 1;
        volatile uint32_t over_current_active : 1;
        //  default: 0
        //  ソフトウェアがこの値を０から１にセットすることで、
        //  USBバスのリセット手続きが開始される？
        volatile uint32_t port_reset : 1;
        //  default:５
        //  このフィールドに値を書き込むためには、LWS（port_link_state_write_strobe）
        //  が１にセットされている必要がある。
        //  読み出すときの値と書き込む値で少し変わっているので注意する。
        volatile uint32_t port_link_state : 4;
        //  default: １
        //  この値が０の時、このポートは電源がオフ状態になっている。
        //  この値が０から１に変わる時：
        //  デバイスが接続されていなければ「Disconnected状態」になる。
        //  デバイスが接続されていれば、「Polling状態（USB３の場合）」になる。
        volatile uint32_t port_power : 1;
        volatile uint32_t port_speed : 4;
        volatile uint32_t port_indicator_control : 2;
        //  default: ０
        //  port_link_stateに書き込みを行うには、このフィールドが１にセットされている必要がある。
        volatile uint32_t port_link_state_write_strobe : 1;
        //  default: ０
        volatile uint32_t connect_status_change : 1;
        volatile uint32_t port_enabled_disabled_change : 1;
        volatile uint32_t warm_port_reset_change : 1;
        volatile uint32_t over_current_change : 1;
        volatile uint32_t port_reset_change : 1;
        volatile uint32_t port_link_state_change : 1;
        volatile uint32_t port_config_error_change : 1;
        volatile uint32_t cold_attach_status : 1;
        volatile uint32_t wake_on_connect_enable : 1;
        volatile uint32_t wake_on_disconnect_enable : 1;
        volatile uint32_t wake_on_over_current_enable : 1;
        uint32_t : 2;
        volatile uint32_t device_removable : 1;
        //  default: ０
        //  USB３でのみ有効なフィールド。
        //  ソフトウェアがこの値を１にセットすることで、ワームリセット手続きが開始される。
        //  詳しくは、USB３の仕様書を確認。
        volatile uint32_t warm_port_reset : 1;
    };

    struct PORTPMSC_Bitmap
    {
        volatile uint32_t u1_timeout : 8;
        volatile uint32_t u2_timeout : 8;
        volatile uint32_t force_link_pm_accept : 1;
        uint32_t : 15;
    };
    
    struct PORTLI_Bitmap
    {
        volatile uint32_t link_error_count : 16;
        volatile uint32_t rx_lane_count : 4;
        volatile uint32_t tx_lane_count : 4;
        uint32_t : 8;
    };
    
    struct PORTHLPMC_Bitmap
    {
        volatile uint32_t host_initiated_resume_duration_mode : 2;
        volatile uint32_t l1_timeout : 8;
        volatile uint32_t best_effort_service_latency_deep : 4;
        uint32_t : 18;
    };
    
    struct PortRegisterSet {
        PORTSC_Bitmap PORTSC;
        PORTPMSC_Bitmap PORTPMSC;
        PORTLI_Bitmap PORTLI;
        PORTHLPMC_Bitmap PORTHLPMC;
    };


/////////////////////// Runtime Registers ///////////////////////
    struct MFINDEX_Bitmap
    {
        volatile uint32_t microframe_index: 14; // 現在のmicroframe(✕125μs)
    } __attribute__((packed));
    union IMAN_Bitmap // 割り込みの有効・無効など
    {
        uint32_t data[1];
        struct {
            volatile uint32_t interrupt_pending : 1; // RW1C, このbitが立っている時、割り込み待ち状態であることを示す。
            volatile uint32_t interrupt_enable : 1; // RW, このbitが立っている時、割り込みが有効であることを示す。
            uint32_t : 30;
        } __attribute__((packed)) bits;
    } __attribute__((packed));
    union IMOD_Bitmap // 割り込みペースの設定
    {
        uint32_t data[1];
        struct {
            // 割り込みの間隔を設定（✕250μs）
            // default: 4000　すなわち１秒
            volatile uint32_t interrupt_moderation_interval : 16;
            volatile uint32_t interrupt_moderation_counter : 16;
        } __attribute__((packed)) bits;
    } __attribute__((packed));
    union ERSTSZ_Bitmap 
    {
    //  イベントリングセグメントテーブルのサイズを定義
    //  この値は、デフォルトでは0となっており、ソフトウェアが設定できる。
    //  この値の上限は決められており、それは
    //  CapabilityRegisters->HCSPARAMS2内の要素で定義されている。 
        uint32_t data[1];
        struct {
            volatile uint32_t event_ring_segment_table_size : 16;
            uint32_t : 16;
        } __attribute__((packed)) bits;

        uint16_t Size() const {
            return bits.event_ring_segment_table_size;
        }

        void SetSize(uint16_t value) {
            bits.event_ring_segment_table_size = value;
        }
    } __attribute__((packed));
    union ERSTBA_Bitmap // Event Ringのセグメントテーブルの先頭アドレス
    {
        uint64_t data[1];
        struct {
            uint64_t : 6;
            volatile uint64_t event_ring_segment_table_base_address : 58;
        } __attribute__((packed)) bits;

        void *Pointer() const {
            return reinterpret_cast<void *>(bits.event_ring_segment_table_base_address << 6);
        }

        void SetPointer(void *value) {
            bits.event_ring_segment_table_base_address = reinterpret_cast<uint64_t>(value) >> 6;
        }
    } __attribute__((packed));
    union ERDP_Bitmap // Event Ringのデキューポインタ
    {
        uint64_t data[1];
        struct {
            volatile uint64_t dequeue_erst_segment_index : 3;
            volatile uint64_t event_handler_busy : 1; // RW1C
            volatile uint64_t event_ring_dequeue_pointer : 60;
        } __attribute__((packed)) bits;

        void *Pointer() const {
            return reinterpret_cast<void *>(bits.event_ring_dequeue_pointer << 4);
        }

        void SetPointer(void *p) {
            // ERDPへ書き込む時にEHBフラグをクリアする必要がある。
            // EHBフラグの属性はRW1Cなため、１が立っている所へ１を書き込むことで０クリアされる。
            // したがって、現在の値をそのまま流用すれば良い。
            ERDP_Bitmap erdp;
            erdp.data[0] = data[0];
            erdp.bits.event_ring_dequeue_pointer = reinterpret_cast<uint64_t>(p) >> 4;
            data[0] = erdp.data[0];
        }
    } __attribute__((packed));
    struct InterrupterRegisterSet 
    {
        IMAN_Bitmap IMAN; // RW, RW1C
        IMOD_Bitmap IMOD; // RW
        ERSTSZ_Bitmap ERSTSZ; // RW
        uint32_t reserved;
        ERSTBA_Bitmap ERSTBA; // RW
        ERDP_Bitmap ERDP; // RW, RW1C
    };
    struct RuntimeRegisters
    {
        uint32_t MFINDEX; // RO
        uint32_t reserved[7];
        InterrupterRegisterSet IR[1024];
    };


/////////////////////// Doorbell Registers ///////////////////////
    //  ドアベルレジスタの構造体。
    //  値の読み書きをする時は、DWORDアクセスでなければならないらしい。
    union Doorbell_Bitmap
    {
        volatile uint32_t data[1];
        struct {
            //  エンドポイントの番号を指定する。（１〜３１）
            //  コマンドリングへのドアベルの場合、ここの値は０とする。
            volatile uint32_t db_target : 8;        
            uint32_t : 8;   //  reserved
            //  ストリームID（よく分からん）
            //  コマンドドアベルの場合は、この領域は０でクリアする。
            volatile uint32_t db_stream_id : 16;
        } __attribute__((packed)) bits;
    } __attribute__((packed));
    class DoorbellRegister
    {
    public:
        void Ring(uint8_t target, uint16_t stream_id) {
            Doorbell_Bitmap value{};
            value.data[0] = 0;
            value.bits.db_target = target;
            value.bits.db_stream_id = stream_id; 
            reg_.data[0] = value.data[0];
        }
    private:
        Doorbell_Bitmap reg_;
    }; 
    
}