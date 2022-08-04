#pragma once 

#include <stdint.h>


namespace usb::xhci
{
//////////////////////// TRB String ////////////////////////
    inline const char* kTRBCompletionCodeToName[37] = {
        "Invalid",
        "Success",
        "Data Buffer Error",
        "Babble Detected Error",
        "USB Transaction Error",
        "TRB Error",
        "Stall Error",
        "Resource Error",
        "Bandwidth Error",
        "No Slots Available Error",
        "Invalid Stream Type Error",
        "Slot Not Enabled Error",
        "Endpoint Not Enabled Error",
        "Short Packet",
        "Ring Underrun",
        "Ring Overrun",
        "VF Event Ring Full Error",
        "Parameter Error",
        "Bandwidth Overrun Error",
        "Context State Error",
        "No ping Response Error",
        "Event Ring Full Error",
        "Incompatible Device Error",
        "Missed Service Error",
        "Command Ring Stopped",
        "Command Aborted",
        "Stopped",
        "Stopped - Length Invalid",
        "Stopped - Short Packet",
        "Max Exit Latency Too Large Error",
        "Reserved",
        "Isoch Buffer Overrun",
        "Event Lost Error",
        "Undefined Error",
        "Invalid Stream ID Error",
        "Secondary Bandwidth Error",
        "Split Transaction Error",
    };

    inline const char* kTRBTypeToName[64] = {
        "Reserved",                             // 0
        "Normal",
        "Setup Stage",
        "Data Stage",
        "Status Stage",
        "Isoch",
        "Link",
        "EventData",
        "No-Op",                                // 8
        "Enable Slot Command",
        "Disable Slot Command",
        "Address Device Command",
        "Configure Endpoint Command",
        "Evaluate Context Command",
        "Reset Endpoint Command",
        "Stop Endpoint Command",
        "Set TR Dequeue Pointer Command",       // 16
        "Reset Device Command",
        "Force Event Command",
        "Negotiate Bandwidth Command",
        "Set Latency Tolerance Value Command",
        "Get Port Bandwidth Command",
        "Force Header Command",
        "No Op Command",
        "Reserved",                             // 24
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Transfer Event",                       // 32
        "Command Completion Event",
        "Port Status Change Event",
        "Bandwidth Request Event",
        "Doorbell Event",
        "Host Controller Event",
        "Device Notification Event",
        "MFINDEX Wrap Event",
        "Reserved",                             // 40
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Vendor Defined",                       // 48
        "Vendor Defined",
        "Vendor Defined",
        "Vendor Defined",
        "Vendor Defined",
        "Vendor Defined",
        "Vendor Defined",
        "Vendor Defined",
        "Vendor Defined",                       // 56
        "Vendor Defined",
        "Vendor Defined",
        "Vendor Defined",
        "Vendor Defined",
        "Vendor Defined",
        "Vendor Defined",
        "Vendor Defined",
    };



    // 全てのTRBが取る大まかな構造。
    // TRBクラスは全てuint32_t data[4]という全体へアクセスする
    // ものを持っている。
    union TRB
    {
        uint32_t data[4];
        struct {
            uint64_t parameter;
            uint32_t status;
            uint32_t cycle_bit : 1;
            uint32_t evaluate_next_trb : 1;
            uint32_t : 8;
            uint32_t trb_type : 6;
            uint32_t control : 16;
        } __attribute__((packed)) bits;

        const char *TypeString() {
            return kTRBTypeToName[bits.trb_type];
        }
    };

    //  TRB Typeは６
    //  TRB Ringの一番うしろについているもの。
    //  次のTRBへのポインタが格納されている。
    union LinkTRB
    {
        static const uint32_t Type = 6;
        uint32_t data[4];
        struct {
            uint64_t : 4;
            uint64_t ring_segment_pointer : 60;

            uint32_t : 22;
            uint32_t interrupter_target : 10;

            uint32_t cycle_bit : 1;
            uint32_t toggle_cycle : 1;
            uint32_t : 2;
            uint32_t chain_bit : 1;
            uint32_t interrupt_on_completion : 1;
            uint32_t : 4;
            uint32_t trb_type : 6;
            uint32_t : 16;
        } __attribute__((packed)) bits;

        LinkTRB(void *next_pointer) : data{} {
            bits.trb_type = Type;
            SetPointer(next_pointer);
        }

        uint64_t Pointer() {
            return bits.ring_segment_pointer << 4;
        }

        void SetPointer(void *p) {
            bits.ring_segment_pointer = reinterpret_cast<uint64_t>(p) >> 4;
        }
    };

    //  Transfer TRB
    //  TRB Typeは１
    union NormalTRB
    {
        static const uint32_t Type = 1;
        uint32_t data[4];
        struct {
            uint64_t data_buffer_pointer;

            uint32_t trb_transfer_length : 17;
            uint32_t td_size : 5;
            uint32_t interrupter_target : 10;

            uint32_t cycle_bit : 1;
            uint32_t evaluate_next_trb : 1;
            uint32_t interrupt_on_short_packet : 1;
            uint32_t no_snoop : 1;
            uint32_t chain_bit : 1;
            uint32_t interrupt_on_completion : 1;
            uint32_t immediate_data : 1;
            uint32_t : 2;
            uint32_t block_event_interrupt : 1;
            uint32_t trb_type : 6;
            uint32_t : 16;
        } __attribute__((packed)) bits;

        NormalTRB() : data{} {
            bits.trb_type = Type;
        }
    };




    /* 
    
    
    
    
    
    
    STOP
    
    
    
    
    
    
    
     */

    //  TRB Typeは２
    struct SetupStageTRB
    {
        //  リクエストの種類を指定するビットフィールド。
        //
        //  第７ビットは、データ転送の流れがhost->deviceなのかdevice-<hostなのかを指定する。
        //  ０：host->device    １：device->host
        //
        //  第５〜６ビットは、Typeを表す。このTypeとは、
        //  ０：Standard（全てのUSBデバイスがサポートしているリクエスト）
        //  １〜３：Additional（各デバイスごとに追加で指定するリクエスト）
        //
        //  第０〜４ビットは、このリクエストを受信する相手（recipient）を指定する。
        //  リクエストは、ここで指定した相手に直接届けられる。
        //  ０：Device
        //  １：Interface
        //  ２：Endpoint
        //  ３：Other
        uint32_t request_type : 8;
        //  リクエストの種類をここで指定する。例として、
        //  ６：GET_DESCRIPTOR
        //  ０：GET_STATUS
        //  などがある。
        uint32_t request : 8;
        //  リクエストの種類によって使われ方が様々。
        uint32_t value : 16;

        //  リクエストの種類によって使われ方が様々。
        //  パラメータを渡す時に用いられる。
        //  エンドポイントの番号を指定する場合にも用いられる。その場合、
        //  第０〜３ビットは、エンドポイントの番号を指定するのに用いられる。
        //
        //  第７ビットは、方向を指定するのに用いられる。
        //  ０：OUT
        //  １：IN
        uint32_t index : 16;
        //  コントロール転送時のデータサイズを指定する。
        //  この値が０の時、Data Stageは存在しないことを示す。
        uint32_t length : 16;

        //  常に８
        uint32_t trb_transfer_length : 17;
        uint32_t : 5;
        //  割り込みの指定。
        uint32_t interrupter_target : 10;

        uint32_t cycle_bit : 1;
        uint32_t : 4;
        //  このビットが１にセットされていれば、このTRBの完了を通知するイベントが作成され、
        //  割り込みも生成されるようになる。
        uint32_t interrupt_on_completion : 1;
        //  SetupTRBでは、ここは１にセットしておくと良い。
        uint32_t immediate_data : 1;
        uint32_t : 3;
        //  Setup Stageは２
        uint32_t trb_type : 6;
        //  このコントロール転送のデータの向きを指定する。
        //  ０：No Data Stage 
        //  ２：OUT Data Stage
        //  ３：IN Data Stage
        uint32_t transfer_type : 2;
        uint32_t : 14;
    };

    //  TRB Typeは３
    struct DataStageTRB
    {
        //  データバッファへのポインタ。
        //  アラインメントは１バイトであるが、ユーザは６４バイトや
        //  １２８バイトに知ることでパフォーマンスを上げることができる。
        uint64_t data_buffer_pointer;

        //  OUTの時は、xHCがこのTRBを実行中に送るデータのバイト数を示す。
        //  INの時は、データバッファのサイズを指定する。
        uint32_t trb_transfer_length : 17;
        uint32_t td_size : 5;
        uint32_t interrupter_target : 10;

        uint32_t cycle_bit : 1;
        //  このビットが立っている時、エンドポイントの状態は保存されず、
        //  即座に次のTRBを読み込む。
        uint32_t evaluate_next_trb : 1;
        uint32_t interrupt_on_short_packet : 1;
        uint32_t no_snoop : 1;
        uint32_t chain_bit : 1;
        uint32_t interrupt_on_completion : 1;
        uint32_t immediate_data : 1;
        uint32_t : 3;
        uint32_t trb_type : 6;
        uint32_t direction : 1;
        uint32_t : 15;
    };
    
    //  TRB Typeは４
    struct StatusStageTRB
    {
        uint64_t : 64;

        uint32_t : 22;
        uint32_t interrupter_target : 10;

        uint32_t cycle_bit : 1;
        uint32_t evaluate_next_trb : 1;
        uint32_t : 2;
        uint32_t chain_bit : 1;
        uint32_t interrupt_on_completion : 1;
        uint32_t : 4;
        uint32_t trb_type : 6;
        uint32_t direction : 1;
        uint32_t : 15;
    };
    




//////////////////////// Event TRB ////////////////////////
    union TransferEventTRB { // TODO; コピーしただけ
        static const uint32_t Type = 32;
        uint32_t data[4];
        struct {
            uint64_t trb_pointer : 64;

            uint32_t trb_transfer_length : 24;
            uint32_t completion_code : 8;

            uint32_t cycle_bit : 1;
            uint32_t : 1;
            uint32_t event_data : 1;
            uint32_t : 7;
            uint32_t trb_type : 6;
            uint32_t endpoint_id : 5;
            uint32_t : 3;
            uint32_t slot_id : 8;
        } __attribute__((packed)) bits;

        TransferEventTRB() : data{} {
            bits.trb_type = Type;
        }

        TRB* Pointer() const {
            return reinterpret_cast<TRB*>(bits.trb_pointer);
        }

        void SetPointer(const TRB* p) {
            bits.trb_pointer = reinterpret_cast<uint64_t>(p);
        }
/* 
        EndpointID EndpointID() const {
            return usb::EndpointID{bits.endpoint_id};
        } */
    };
    
    union CommandCompletionEventTRB {
        static const uint32_t Type = 33;
        uint32_t data[4];
        struct {
            uint64_t : 4;
            volatile uint64_t command_trb_pointer : 60;

            volatile uint32_t command_completion_parameter : 24;
            volatile uint32_t completion_code : 8;

            volatile uint32_t cycle_bit : 1;
            uint32_t : 9;
            volatile uint32_t trb_type : 6;
            volatile uint32_t vf_id : 8;
            volatile uint32_t slot_id : 8;
        } __attribute__((packed)) bits;

        CommandCompletionEventTRB() : data{} {
            bits.trb_type = Type;
        }

        TRB* Pointer() const {
            return reinterpret_cast<TRB*>(bits.command_trb_pointer << 4);
        }

        void SetPointer(TRB* p) {
            bits.command_trb_pointer = reinterpret_cast<uint64_t>(p) >> 4;
        }

        const char *CompletionCodeName() {
            return kTRBCompletionCodeToName[bits.completion_code];
        }
    };

    union PortStatusChangeEventTRB {
        static const unsigned int Type = 34;
        uint32_t data[4];
        struct {
            uint32_t : 24;
            volatile uint32_t port_id : 8; // root hub port number

            uint32_t : 32;

            uint32_t : 24;
            volatile uint32_t completion_code : 8;

            volatile uint32_t cycle_bit : 1;
            uint32_t : 9;
            volatile uint32_t trb_type : 6;
        } __attribute__((packed)) bits;

        PortStatusChangeEventTRB() : data{} {
            bits.trb_type = Type;
        }

        const char *CompletionCodeName() {
            return kTRBCompletionCodeToName[bits.completion_code];
        }
    };






//////////////////////// Command TRB ////////////////////////
    union NoOpCommandTRB
    {
        static const uint32_t Type = 23;
        uint32_t data[4];
        struct {
            uint32_t : 32;
            uint32_t : 32;
            uint32_t : 32;

            volatile uint32_t cycle_bit : 1;
            uint32_t : 9;
            volatile uint32_t trb_type : 6;
            uint32_t : 16;
        } __attribute__((packed)) bits;

        NoOpCommandTRB() : data{} {
            bits.trb_type = Type;
        }
    } __attribute__((packed));

    union EnableSlotCommandTRB // xHCに利用可能なデバイススロットを選び割り当ててもらうコマンド
    {
        static const uint32_t Type = 9;
        uint32_t data[4];
        struct {
            uint32_t : 32;
            uint32_t : 32;
            uint32_t : 32;

            volatile uint32_t cycle_bit : 1;
            uint32_t : 9;
            volatile uint32_t trb_type : 6;
            volatile uint32_t slot_type : 5; //  選んでもらうスロットのタイプを指示する
            uint32_t : 11;
        } __attribute__((packed)) bits;

        EnableSlotCommandTRB() : data {} {
            bits.trb_type = Type;
        }
    } __attribute__((packed));

    union DisableSlotCommandTRB // 指定したスロットをDisableにするコマンド
    {
        static const uint32_t Type = 10;
        uint32_t data[4];
        struct {
            uint32_t : 32;
            uint32_t : 32;
            uint32_t : 32;

            volatile uint32_t cycle_bit : 1;
            uint32_t : 9;
            volatile uint32_t trb_type : 6;
            uint32_t : 8;
            volatile uint32_t slot_id : 8; // DisableにしたいスロットのIDを書き込む。
        } __attribute__((packed)) bits;

        DisableSlotCommandTRB(uint8_t slot_id) : data{} {
            bits.slot_id = slot_id;
            bits.trb_type = Type;
        }
    } __attribute__((packed));

    union AddressDeviceCommandTRB
    {
        static const uint32_t Type = 11;
        uint32_t data[4];
        struct {
            uint64_t : 4;
            volatile uint64_t input_context_pointer : 60;

            uint32_t : 32;

            volatile uint32_t cycle_bit : 1;
            uint32_t : 8;
            //  この値が０の時は、「USB SET_ADDRESS」リクエストを生成する。
            //  １の時は、生成しない。
            //  もっと詳しく書きたいが、後でにしておく。
            volatile uint32_t block_set_address_request : 1;
            volatile uint32_t trb_type : 6;
            uint32_t : 8;
            volatile uint32_t slot_id : 8;
        } __attribute__((packed)) bits;

        void *Pointer() const {
            return reinterpret_cast<void *>(bits.input_context_pointer << 4);
        }

        void SetPointer(void *p) {
            bits.input_context_pointer = reinterpret_cast<uint64_t>(p) >> 4;
        }

  /*       AddressDeviceCommandTRB(InputContext* input_context, uint8_t slot_id) : data{} {
            bits.trb_type = Type;
            bits.slot_id = slot_id;
            SetPointer(input_context);
        } */
    } __attribute__((packed));

    union ConfigureEndpointCommandTRB //  エンドポイントの帯域幅や必要な資源を評価する（よく分からん）
    {
        static const uint32_t Type = 12;
        uint32_t data[4];
        struct {
            uint64_t : 4;
            volatile uint64_t input_context_pointer : 60;

            uint32_t : 32;

            volatile uint32_t cycle_bit : 1;
            uint32_t : 8;
            volatile uint32_t deconfigure : 1;
            volatile uint32_t trb_type : 6;
            uint32_t : 8;
            volatile uint32_t slot_id : 8;
        } __attribute__((packed)) bits;

        void *Pointer() const {
            return reinterpret_cast<void *>(bits.input_context_pointer << 4);
        }

        void SetPointer(void *p) {
            bits.input_context_pointer = reinterpret_cast<uint64_t>(p) >> 4;
        }

/*         ConfigureEndpointCommandTRB(InputContext* input_context, uint8_t slot_id) : data{} {
            bits.trb_type = Type;
            bits.slot_id = slot_id;
            SetPointer(input_context);
        } */
    } __attribute__((packed));
    
    union StopEndpointCommandTRB //  xHCがTDを実行するのを一時停止させるコマンド？
    {
        static const uint32_t Type = 15;
        uint32_t data[4];
        struct {
            uint32_t : 32;
            uint32_t : 32;
            uint32_t : 32;

            volatile uint32_t cycle_bit : 1;
            uint32_t : 9;
            volatile uint32_t trb_type : 6;
            volatile uint32_t endpoint_id : 5;
            uint32_t : 2;
            volatile uint32_t suspend : 1;
            volatile uint32_t slot_id : 8;
        } __attribute__((packed)) bits;

/*         StopEndpointCommandTRB(DeviceContextIndex endpoint_id, uint8_t slot_id) : data{} {
            bits.trb_type = Type;
            bits.endpoint_id = endpoint_id.value;
            bits.slot_id = slot_id;
        } */
    } __attribute__((packed));
    


//  OTHER TRB
    ///////////////////////////OK
    

}
