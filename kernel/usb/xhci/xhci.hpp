#pragma once

#include <stdint.h>

#include "ring.hpp"
#include "devmgr.hpp"
#include "../../pci.hpp"
#include "../../logging.hpp"



namespace usb::xhci
{
    class Controller
    {
    public:
        Controller(uintptr_t mmio_base);

        int Initializer(); // 成功０、失敗−１

        void Run();

        Ring *CommandRing();

        EventRing *PrimaryEventRing();

        InterrupterRegisterSet *PrimaryInterruptRegs();


        void SendNoOpCommand(); //  コントローラーにNoOpCommandTRBを送りドアベルを鳴らす
    
    private:
        const uintptr_t mmio_base_;
        CapabilityRegisters* const cap_;
        OperationalRegisters* const opt_;
        RuntimeRegisters* const run_;
        DoorbellRegister* const doorbell_;
        class DeviceManager devmgr_; // デバイスの管理オブジェクト

        Ring cr_; // コマンドリング
        EventRing er_; // イベントリング


    }; 




    void Initialize();
}