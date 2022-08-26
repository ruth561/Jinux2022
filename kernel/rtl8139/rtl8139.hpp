#pragma once 

#include <cstdlib>
#include <cstring>
#include <array>

#include "../pci.hpp"
#include "../logging.hpp"
#include "registers.hpp"
#include "packet.hpp"
#include "../network/nic.hpp"
#include "../network/ethernet.hpp"

namespace rtl8139
{
    class Controller : public network::NIC
    {
    public:
        Controller(uint32_t mmio_base);

        // 初期化関数。NICの仮想関数をオーバーライド。
        // ReceiveとTransferができるようにする。
        int Initialize() override;

        uint8_t *MACAddress() override;

        // Rx Bufferからパケットを受け取り処理をする
        // パケットのデータ部分だけが返される。返り値はその先頭ポインタ。
        // パケットが読み込めなかったらnullptrを返す。
        void *ReceivePacket() override;

        // 長さlenの生のパケットpacketを送信する
        void SendPacket(void *packet, uint16_t len) override;

    private:
        const uint32_t mmio_base_; // メモリマップされたレジスタの先頭アドレス
        OperationalRegister *opt_; // オペレーショナルレジスタ

        uint8_t mac_addr_[6];
        
        uint64_t rx_buffer_; // Rx Bufferの先頭アドレス
        size_t rx_buffer_size_; // Rx Bufferの大きさ

        size_t max_packet_size_ = 0x700; // 転送可能なパケットの大きさの最大
        std::array<void *, 4> tx_buffers_; // それぞれ、TSAに書き込む値
        int current_tx_buffer_; // 次使用するtx_buffers_の番号（0->1->2->3->0->..）

        uint16_t rx_offset_; // Rx Ringの次の読み込み位置
    };

    // ネットワークマネージャーの初期化処理から呼び出される
    // rtl8139が存在していれば、そのオブジェクトを作成して返す。
    // 存在していなければ、nullptrを返す。
    Controller *RTL8139Initialize();
}
