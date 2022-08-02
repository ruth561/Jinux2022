#pragma once 

#include <stdint.h>
#include <cstring>
#include <new>
#include "context.hpp"
#include "../memory.hpp"
#include "registers.hpp"


namespace usb::xhci
{
    class DeviceManager
    {
    public:
        void Initialize(uint8_t max_slots);

        //  DCBAAP（DCBAAへのポインタ）を返す。
        DeviceContext **DeviceContexts() {
            return DCBAA_;
        }


        //  Deviceクラスのオブジェクトを一つ生成し、このポインタを
        //  devices_[slot_id]に格納する。
        //  成功時は０を失敗時は−１を返す。
        //  デバイスの初期化も行うので、デバイスオブジェクトには０クリアされた
        //  ２種類のコンテキストメンバが存在することになる。
        int AllocDevice(uint8_t slot_id, Doorbell_Bitmap* dbreg);

        void DisableSlot(uint8_t slot_id);

        //  指定したスロット番号のDCBAAエントリにデバイスコンテキストの
        //  先頭ポインタを格納する。
        //  デバイスコンテキストが存在しない場合などのエラー時には−１を返す。
        //  成功したら０を返す。
        int LoadDCBAA(uint8_t slot_id);

    private:
        uint8_t max_slots_; // xHCが扱うスロットの最大数

        //  この配列は、max_slots_＋１までのみ有効である。
        //  DCBAAの取りうる値の最大数が２５６なのでこのように領域を確保しているが、全て使うわけではない。
        DeviceContext **DCBAA_;

        //  各スロットに紐付けられたデバイスへのポインタ配列。
        //  devices_[i]はスロットIDがiにつながっているデバイスオブジェクトを表す。
        //  デバイスオブジェクトには、デバイスコンテキストやインプットコンテキストや
        //  ドアベルへのポインタなどが保持されている。
        // Device **devices_;

    };
}



