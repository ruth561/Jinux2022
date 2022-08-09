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

        //  エンドポイントの設定が終わった後に呼び出される関数。
        //  デバイスに対し、プロトコルをブートプロトコルを使うように指示する。
        //  EP０を用いて行われる。
        void OnEndpointsConfigured() override;

        // デバイスからの返信がxHC->USBドライバを経由して
        void OnControlCompleted(EndpointID ep_id, SetupData setup_data, void *buf, int len) override;

        // デフォルトコントロールパイプにGET_REPORTリクエストを投げかける関数
        void GetKeyInControlPipe();

        // Interrupt Endpointで受け取れるようにリクエストを投げる
        void RequestKeyViaIntEP();

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


