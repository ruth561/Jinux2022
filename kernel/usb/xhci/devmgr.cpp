#include "devmgr.hpp"


namespace usb::xhci
{
    
    void DeviceManager::Initialize(uint8_t max_slots)
    {
        max_slots_ = max_slots;

        // DCBAAの領域の確保
        DCBAA_ = (DeviceContext **) usb::AllocMem(8 * (max_slots_ + 1), 64, 4096); // アラインメント・境界に注意
        memset(DCBAA_, 0, 8 * (max_slots_ + 1)); // DCBAA領域を０クリア

        // devices_ = (Device **) usb::AllocMem(8 * (max_slots_ + 1), 0, 0);
        // memset(devices_, 0, 8 * (max_slots_ + 1)); 
    }
/* 
    Device *DeviceManager::FindByPort(uint8_t port_num)
    {
        Device *dev;
        for (int i = 1; i <= max_slots_; i++) {
            dev = devices_[i];
            if (dev == NULL) continue;
            if (dev->DeviceContext()->slot_context.root_hub_port_num == port_num) {
                return dev;
            }
        }
        return NULL;
    }

    Device *DeviceManager::FindBySlot(uint8_t slot_id)
    {
        if (slot_id > max_slots_) return NULL;

        return devices_[slot_id];
    }

    int DeviceManager::AllocDevice(uint8_t slot_id, Doorbell_Bitmap* dbreg)
    {
        if (slot_id > max_slots_) return -1;
        //  指定したスロットがすでに使用されている場合は−１を返す。
        if (devices_[slot_id] != NULL) {
            return -1;
        }

        // printk("[DEBUG DevMgr::AllocDevice()] slot_id=%d\n", slot_id);
        devices_[slot_id] = (Device *) AllocMem(sizeof(Device), 0, 0);
        new(devices_[slot_id]) Device(slot_id, dbreg);

        devices_[slot_id]->Initialize();
        return 0;
    }

    void DeviceManager::DisableSlot(uint8_t slot_id)
    {
        devices_[slot_id] = NULL;
        DCBAA_[slot_id] = NULL;

        //  malloc関数が使えれば、デバイスコンテキストやインプットコンテキストの領域を
        //  解放する必要がある。今回はやらないの。
    }

    int DeviceManager::LoadDCBAA(uint8_t slot_id)
    {
        if (slot_id > max_slots_) return -1;
        if (devices_[slot_id] == NULL) return -1;

        DCBAA_[slot_id] = devices_[slot_id]->DeviceContext();
        return 0;
    } 
     */

    
}


