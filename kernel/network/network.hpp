/* 
ネットワーク位を制御するクラスをここで定義したい
現状、デバイスドライバのrtl8139を色んな所からアクセスしているのが気になる。
 */
#pragma once 
#include <cstdint>
#include "nic.hpp"
#include "ethernet.hpp"
#include "ip.hpp"
#include "arp.hpp"
#include "../rtl8139/rtl8139.hpp"
#include "../logging.hpp"

namespace network
{
    class NetworkManager 
    {
    public:
        // NICの初期化などを行う
        void Initialize();

        // パケットをNICを用いて送信する
        void SendPacket(void *pkt, uint16_t pkt_len);

        // 送られてきたパケットをNICから読み出し、この中で処理をする
        void ReceivePacket();

        uint8_t *MACAddress();
        uint8_t *IPAddress();

    private:
        NIC *nic_;

        uint8_t mac_addr_[6];
        uint8_t ip_addr_[4];

        // std::mapでアドレス変換表を保持したい  
    };

    // 諸々の初期化を行う
    void Initialize();
}