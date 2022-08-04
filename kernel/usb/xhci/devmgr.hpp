#pragma once 

#include <stdint.h>
#include <cstring>
#include <new>
#include "context.hpp"
#include "device.hpp"
#include "../memory.hpp"
#include "registers.hpp"


namespace usb::xhci
{
    class DeviceManager
    {
    public:
        void Initialize(uint8_t max_slots); // デバイス管理領域を確保し０クリアする

        DeviceContext **DeviceContexts(); //  DCBAAのポインタ

        void DisableSlot(uint8_t slot_id); // スロットを解放する
        Device *FindByPort(uint8_t port_num); // ポート番号からデバイスを検索する
        Device *FindBySlot(uint8_t slot_id); // スロット番号からデバイスを検索する

        // Deviceオブジェクトを生成し、このポインタをdevices_[slot_id]に格納する。
        // Deviceオブジェクトの初期化をし、コンテキストの０クリアも行う。
        // DCBAA_にデバイスコンテキストのロードも行う 
        // 成功時は０を失敗時は−１を返す。
        int AllocDevice(uint8_t slot_id, DoorbellRegister* doorbell);

        //  指定したスロット番号のDCBAAエントリにデバイスコンテキストの
        //  先頭ポインタを格納する。
        //  デバイスコンテキストが存在しない場合などのエラー時には−１を返す。
        //  成功したら０を返す。
        int LoadDCBAA(uint8_t slot_id);
        
    private:
        uint8_t max_slots_; // xHCが扱うスロットの最大数
        DeviceContext **DCBAA_;
        //  各スロットに紐付けられたデバイスへのポインタ配列。
        //  devices_[i]はスロットIDがiにつながっているデバイスオブジェクトを表す。
        //  デバイスオブジェクトには、デバイスコンテキストやインプットコンテキストや
        //  ドアベルへのポインタなどが保持されている。
        Device **devices_;

    };
}



