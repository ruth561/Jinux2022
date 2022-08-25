/* 
ネットワーク位を制御するクラスをここで定義したい
現状、デバイスドライバのrtl8139を色んな所からアクセスしているのが気になる。
 */
#pragma once 
#include <cstdint>

namespace network
{
    // NICドライバにかかわらず抽象化されたパケットの構造体
    // 長さの情報は必須？
    struct Packet
    {
        uint16_t len;

        uint8_t data[];
    };

    class NetworkManager 
    {
    public:

    private:
        uint8_t mac_addr_[6];
        uint8_t ip_addr_[4];

        // std::mapでアドレス変換表を保持したい  
    };

}