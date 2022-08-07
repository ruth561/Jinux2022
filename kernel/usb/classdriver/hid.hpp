#pragma once 

#include "../device.hpp"
#include "../endpoint.hpp"
#include "../setupdata.hpp"
#include "base.hpp"

namespace usb
{
    class Device; // includeの関係上

    class HIDKeyboardDriver : public ClassDriver
    {
    public:
        HIDKeyboardDriver(Device *dev, uint8_t interface_number) : 
            dev_{dev}, interface_number_{interface_number} {}
        
        // デバイスがConfiguredになったらUSBデバイスドライバから呼び出される
        void Run() override;

        // デバイスからの返信
        void OnControlCompleted(EndpointID ep_id, SetupData setup_data, void *buf, int len) override;

        void GetKeyInControlPipe();

        // 使用するプロトコルをブートプロトコルに設定するリクエストを発行
        void SetBootProtocol();

    protected:
        Device *dev_; // このクラスが存在しているUSBデバイスのドライバ
        uint8_t interface_number_; // USBデバイスの中で、このクラスに対応しているInterfaceの番号

        uint32_t last_tick_; // キー入力が最後に検知されたときのタイマーのチック数
        uint32_t key_stroke_interval_; // キー入力の検知間隔（Ticks）


        char buf_[256]; // データの受け渡しに使われるバッファ
    };
}


