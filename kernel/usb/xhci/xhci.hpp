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
        // opt_.USBCMDとopt_.USBSTSを用いて初期化する関数。
        // 成功０、失敗−１
        int Initializer();
    
    private:
        const uintptr_t mmio_base_;
        CapabilityRegisters* const cap_;
        OperationalRegisters* const opt_;
        RuntimeRegisters* const run_;
        Doorbell_Bitmap* const doorbell_;
        class DeviceManager devmgr_; // デバイスの管理オブジェクト

        Ring cr_; // コマンドリング
        EventRing er_; // イベントリング

    }; 




    void Initialize();
}