#include "network.hpp"

int printk(const char *format, ...);
void Halt();
extern logging::Logger *logger;



namespace network
{
    NetworkManager *manager;

    void NetworkManager::Initialize()
    {
        nic_ = rtl8139::RTL8139Initialize();
        if (nic_ == nullptr) {
            logger->info("RTL8139 Is Not Exist.\n");
            return;
        }

        // NICの初期化
        // この後から、パケットの受信転送ができるようになる
        nic_->Initialize();

        // 各種アドレスの設定
        memcpy(mac_addr_, nic_->MACAddress(), 6);
        ip_addr_[0] = 10;
        ip_addr_[1] = 0;
        ip_addr_[2] = 2;
        ip_addr_[3] = 15;

        /* 
        本来は、ここでDHCPによってIPを取得したい
         */
    }

    void NetworkManager::SendPacket(void *pkt, uint16_t pkt_len)
    {
        nic_->SendPacket(pkt, pkt_len);
    }

    void NetworkManager::ReceivePacket()
    {
        void *pkt = nic_->ReceivePacket();
        if (pkt == nullptr) {
            return;
        }

        printk("[Receive Packet]\n");
        // EthernetHandlerへ渡す
        ethernet::HandlePacket(reinterpret_cast<ethernet::EthernetFrame *>(pkt)); // データ部分だけを渡す
        
        // パケット領域の解放
        free(pkt);
    }

    uint8_t *NetworkManager::MACAddress()
    {
        return nic_->MACAddress();
    }
    
    uint8_t *NetworkManager::IPAddress()
    {
        return ip_addr_;
    }



    void Initialize()
    {
        manager = new NetworkManager{};
        manager->Initialize();

        uint8_t dst_ip_addr[] = {10, 0, 2, 3};
        arp::SendRequest(dst_ip_addr);

        while (1) {
            manager->ReceivePacket();
        }
    }
}

