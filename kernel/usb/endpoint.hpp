#pragma once

#include "xhci/context.hpp"


namespace usb
{
    enum EndpointType {
        kControl = 0,
        kIsochronous = 1,
        kBulk = 2,
        kInterrupt = 3,
    };

    inline const char *kEndpointTypeToName[4] = {
        "Control",
        "Isochronous", 
        "Bulk", 
        "Interrupt"
    };

    //  エンドポイントの持つ情報をまとめて管理するクラス。
    class EndpointID
    {
    public:
        EndpointID() : dci_(0) {}
        EndpointID(xhci::DeviceContextIndex dci) : dci_{dci} {}
        EndpointID(uint8_t ep_num, bool dir_in) : dci_{xhci::DeviceContextIndex{ep_num, dir_in}} {}

        //  デバイスコンテキスト内のインデックス（０〜３１）を返す。
        uint8_t DeviceContextID() { return dci_.value; }
        //  エンドポイントの番号を返す。
        uint8_t EndpointNumber() { return dci_.value >> 1; }
        //  このエンドポイントが入力方向ならtrueを返す。
        bool IsIn() { return dci_.value & 1; }
    private:
        xhci::DeviceContextIndex dci_;   //  デバイスコンテキストID
    };

    struct EndpointConfig
    {
        //  エンドポイントID
        EndpointID ep_id;

        //  エンドポイントのタイプ
        EndpointType ep_type;

        // このエンドポイントの最大パケットサイズ（バイト）
        int max_packet_size;

        // このエンドポイントの制御周期（125*2^(interval-1) マイクロ秒）
        int interval;
    };

    const EndpointID kDefaultControlPipeID = EndpointID{xhci::DeviceContextIndex{1}};
}
