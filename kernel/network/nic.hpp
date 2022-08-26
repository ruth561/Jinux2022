#pragma once 

#include <cstdint>

namespace network
{
    // 全てのNICが持つべき関数をまとめた基底クラス
    // rtl8139などもこれの子クラスとなっている。
    class NIC
    {
    public:
        // NICの初期化関数
        virtual int Initialize() { return 0; }

        // MACアドレスをuint8_t[6]という形式で返す
        virtual uint8_t *MACAddress() { return nullptr; }

        // パケットを受け取り処理をする
        // パケットは、mallocで生成したメモリ領域に格納されているので、freeを忘れないように！
        virtual void *ReceivePacket() { return nullptr; }

        // パケットの送信処理をする
        virtual void SendPacket(void *pkt, uint16_t pkt_len) {}
    };

}