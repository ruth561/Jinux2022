#pragma once

#include <stdint.h>

#include "ring.hpp"
#include "devmgr.hpp"
#include "port.hpp"
#include "../../pci.hpp"
#include "../../logging.hpp"
#include "../../task.hpp"



namespace usb::xhci
{
    class Controller
    {
    public:
        Controller(uintptr_t mmio_base);

        int Initializer(); // 成功０、失敗−１

        void Run();

        void AllPortsInit(); //  全てのポートの初期化を始める関数。

        Ring *CommandRing();

        EventRing *PrimaryEventRing();

        InterrupterRegisterSet *PrimaryInterruptRegs();

        uint8_t MaxPorts() const; // コントローラーの持つルートポートの数

        Port *PortAt(uint8_t port_num); // １ ≦ port_num ≦ MaxPorts()

        void SendNoOpCommand(); //  コントローラーにNoOpCommandTRBを送りドアベルを鳴らす

        //  現在イベントリングに溜まっている次のイベントを処理する。
        //  無事実行が完了したら０、失敗したら−１を返す。
        int ProcessEvent();

        //  指定したデバイスの各エンドポイントの設定を行う。
        //  デバイスの初期化処理が完了した後に呼び出される。
        //  デバイスのオブジェクトには、このOSで使用可能なインターフェースの
        //  使用するエンドポイントが列挙されている。
        int ConfigureEndpoints(Device *dev);
    
    // private:
        uint8_t device_size_; // デバイススロットの有効化数 
        std::vector<uint8_t> slot_to_port_;

        const uintptr_t mmio_base_;
        CapabilityRegisters* const cap_;
        OperationalRegisters* const opt_;
        RuntimeRegisters* const run_;
        DoorbellRegister* const doorbell_; // doorbell_[i].Ring()でi番目のドアベルを鳴らす
        class DeviceManager devmgr_; // デバイスの管理オブジェクト
        std::array<Port *, 256> ports_;

        Ring cr_; // コマンドリング
        EventRing er_; // イベントリング

                
        void RingCommandRing(); // CommandRingのドアベルを鳴らす

        int OnEvent(CommandCompletionEventTRB *trb);
        int OnEvent(PortStatusChangeEventTRB *trb);
        int OnEvent(TransferEventTRB *trb);

        int ResetPort(Port *port); // port initialize fase 1

        //  ポートをリセットした後に呼び出される関数。
        //  EnableSlotCommandを発行して、スロットを有効にする。
        //  ポートがEnable状態ではない場合またはリセット処理が完了していない場合は−１を返す。
        //  コマンドを無事に送れれば０を返す。コマンド完了イベントの処理は別の関数が行う。
        int EnableSlot(Port *port);  // port initialize fase 1
        
        //  デバイスがデタッチされた時に、使用されていたスロットを
        //  Disableにする関数。
        int DisableSlot(uint8_t slot_id);

        //  EnableSlotが完了した後に呼び出される関数。
        //  ルートハブポートに接続されているデバイスにアドレスを割り当てる。
        //  具体的には、デバイスを使用可能なスロットにつなげて初期化を行う。
        //  デバイス固有のデバイスコンテキストとインプットコンテキストを初期化し、
        //  エンドポイント０を初期化し、Transfer Ringを用意する。
        //  最後に「Address Device Command」を発行し、終了する。
        //  無事終了すれば、０を返す。
        //  失敗した場合は、−１を返す。
        //  コマンド完了イベントの処理は、他の関数に任せている。
        int AddressDevice(uint8_t port_id, uint8_t slot_id);

        int CompleteConfiguration(uint8_t port_id, uint8_t slot_id);
    }; 



    // 初期化する（カーネルから呼び出す）
    void Initialize(); 
    // Event Ringに溜まっているものを処理する
    void ProcessEvents();

    // USBデバイスの初期化を行う
    // Portにデバイスが接続されていることを確認後タスクとして実行される
    void InitUSBDeviceTask(uint64_t id, int64_t port);

}