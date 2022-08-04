#pragma once

#include <stdint.h>
#include "trb.hpp"

namespace usb::xhci
{
    union SlotContext {
        uint32_t dwords[8];
        struct {
            volatile uint32_t route_string : 20; // hub用
            volatile uint32_t speed : 4;
            uint32_t : 1;
            volatile uint32_t mtt : 1; // hub用
            volatile uint32_t hub : 1; // 1: hub, 0: USB function
            //  有効化されたエンドポイントの範囲を示す値。
            //  つまり、最後の有効なエンドポイントの番号を表す。（１〜３１）
            //  どのエンドポイントも使われていない場合、この領域は０となる。
            //  この領域は、インプットコンテキストの場合ソフトウェアが記入し、
            //  アウトプットコンテキストの場合、xHCが記入することになっている。
            volatile uint32_t context_entries : 5;

            volatile uint32_t max_exit_latency : 16;
            volatile uint32_t root_hub_port_num : 8; // デバイスが接続しているルートハブポートの番号
            volatile uint32_t num_ports : 8; // hub用

            // TT : Transaction Translator
            volatile uint32_t tt_hub_slot_id : 8;
            volatile uint32_t tt_port_num : 8;
            volatile uint32_t ttt: 2;
            uint32_t : 4;
            volatile uint32_t interrupter_target : 10; // TODO:

            //  xHCによってUSBデバイスに割り当てられたアドレスを表す。
            //  アウトプットコンテキストでは、この領域はreserved扱いとなる。
            //  インプットコンテキストでは、０クリアしておく。
            volatile uint32_t usb_device_address : 8;
            uint32_t : 19;
            //  デバイススロットの状態を表す。
            //  ０：Disabled/Enabled
            //  １：Default
            //  ２：Addressed
            //  ３：Configured
            //  ４〜：reserved
            //  インプットコンテキストもアウトプットコンテキストもこの領域は０で初期化する。
            volatile uint32_t slot_state : 5;
        } __attribute__((packed)) bits;
    } __attribute__((packed));


    struct DeviceContextIndex
    {
        unsigned int value;

        explicit DeviceContextIndex(unsigned int dci)
        : value{dci}
        {}

        DeviceContextIndex(unsigned int ep_num, bool dir_in)
            : value{2 * ep_num + (ep_num == 0 ? 1 : dir_in)} {}

        DeviceContextIndex(const DeviceContextIndex& rhs) = default;
        DeviceContextIndex& operator =(const DeviceContextIndex& rhs) = default;
    };

    union EndpointContext {
        uint32_t dwords[8];
        struct {
            //  ３ビットのデータ
            //  ０：Disabled
            //  １：Running     ドアベルからの通知を待っているか、TDを実行中の状態。
            //  ２：Halted
            //  ３：Stopped
            //  ４：Error
            //  ５〜７：Reserved
            volatile uint32_t ep_state : 3;
            uint32_t : 5;
            volatile uint32_t mult : 2; // typeがSSIsochでない場合はreserved
            //  この値が０の時、このエンドポイントはストリームをサポートしていないことを示す。
            //  この時、tr_dequeue_pointerはTRリングを参照する。
            //  この値が１以上の時、（意味を持つが、よく調べてないので後で）
            volatile uint32_t max_primary_streams : 5;
            volatile uint32_t linear_stream_array : 1;
            //  エンドポイントがデータを転送する時にポーリングする時間の間隔を指定する。
            //  125マイクロ秒を単位として、２の冪で表される。
            //  ここで指定される値は、エンドポイントの種類によって異なる意味を持つ。
            //  Isochタイプの時やInterruptの時では、この値を２の冪で表すのか
            //  そのままの値を使うのか、などである。
            volatile uint32_t interval : 8;
            volatile uint32_t max_esit_payload_hi : 8;

            uint32_t : 1;
            //  ２ビットで表されるフィールド。
            //  Transfer Descripterを処理中に許容されるBus Errorの回数を定義する。
            //  TDを処理中にBus Errorがこの回数生じた場合、xHCはTRBの実行を終了し、
            //  エンドポイントをHalted状態にする。
            //  この領域が０の場合、Bus Errorの回数に制限はなくなる。
            volatile uint32_t error_count : 2;
            //  ３ビットのデータ
            //  ０：Not Valid
            //  １：Isoch Out
            //  ２：Bulk Out
            //  ３：Interrupt Out
            //  ４：Control Bidirectional
            //  ５：Isoch In
            //  ６：Bulk In
            //  ７：Interrupt In
            volatile uint32_t ep_type : 3;
            uint32_t : 1;
            volatile uint32_t host_initiate_disable : 1;
            volatile uint32_t max_burst_size : 8;
            volatile uint32_t max_packet_size : 16;

            volatile uint32_t dequeue_cycle_state : 1;
            uint32_t : 3;
            //  デキューポインタの値を格納するフィールド。
            //  読み出す時は４ビット左シフト、
            //  書き込む時は４ビット右シフトする必要がある。
            //  つまり、１６ビットでアラインメントされたアドレスを受け付ける。
            volatile uint64_t tr_dequeue_pointer : 60;

            volatile uint32_t average_trb_length : 16;
            volatile uint32_t max_esit_payload_lo : 16;
        } __attribute__((packed)) bits;

        TRB* TransferRingBuffer() const {
            return reinterpret_cast<TRB*>(bits.tr_dequeue_pointer << 4);
        }

        void SetTransferRingBuffer(TRB* buffer) {
            bits.tr_dequeue_pointer = reinterpret_cast<uint64_t>(buffer) >> 4;
        }
    } __attribute__((packed));

    struct DeviceContext
    {
        SlotContext slot_context;
        EndpointContext endpoint_context[31];

        EndpointContext *GetEndpoint(DeviceContextIndex dci) {
            return &endpoint_context[dci.value - 1];
        }
    } __attribute__((packed));


    struct InputControlContext
    {
        //  これらの各ビットは、デバイスコンテキストの番号に対応しており、
        //  コマンドによってDisabled状態に変更するエンドポイントを指定する。
        //  そのため、SlotContextとEP０は指定できないようになっている。
        //  下位２ビットはreserved状態だと思って良い。
        //  第ｎビットが１の時、DCIがｎのEPはコマンドによって処理される。
        //  第ｎビットが０の時、無視される。
        volatile uint32_t drop_context_flags;
        //  ３２ビットの各ビットは、デバイスコンテキストの番号に対応しており、
        //  コマンドによって使用可能状態にする時に用いられる。
        //  第ｎビットが１の時、DCIがｎのEPはコマンドによって処理される。
        //  第ｎビットが０の時、無視される。
        volatile uint32_t add_context_flags;
        uint32_t reserved1[5];
        //  Configure Endpoint Commandで使用される。
        volatile uint8_t configuration_value;
        //  Configure Endpoint Commandで使用される。
        volatile uint8_t interface_number;
        //  Configure Endpoint Commandで使用される。
        volatile uint8_t alternate_setting;
        volatile uint8_t reserved2;
    } __attribute__((packed));
    
    struct InputContext {
        InputControlContext input_control_context;
        SlotContext slot_context;
        EndpointContext ep_contexts[31];

        //  スロットコンテキストをEnable状態にする。
        //  具体的には、InputContorolContextのadd_context_flagの第０ビットを１にする関数。
        //  初期化時に呼び出される。
        SlotContext* EnableSlotContext() {
            input_control_context.add_context_flags |= 1;
            return &slot_context;
        }

        //  dci（Device Context Index）で指定したエンドポイントをEnable状態にする。
        //  具体的には、InputContorolContextのadd_context_flagの第dciビットを１にする関数。
        //  エンドポイントを指定する必要があるため、１≦dci≦３１とする。
        EndpointContext* EnableEndpoint(int dci) {
            input_control_context.add_context_flags |= 1u << dci;
            return &ep_contexts[dci - 1];
        }
    } __attribute__((packed));
}

