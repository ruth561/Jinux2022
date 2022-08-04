#include "devmgr.hpp"


namespace usb::xhci
{
    void DeviceManager::Initialize(uint8_t max_slots)
    {
        max_slots_ = max_slots;

        // DCBAAの領域の確保
        DCBAA_ = (DeviceContext **) usb::AllocMem(8 * (max_slots_ + 1), 64, 4096); // アラインメント・境界に注意
        memset(DCBAA_, 0, 8 * (max_slots_ + 1)); // DCBAA領域を０クリア

        devices_ = (Device **) usb::AllocMem(8 * (max_slots_ + 1), 0, 0);
        memset(devices_, 0, 8 * (max_slots_ + 1)); 
    }

    DeviceContext **DeviceManager::DeviceContexts()
    {
         return DCBAA_;
    }

    void DeviceManager::DisableSlot(uint8_t slot_id)
    {
        devices_[slot_id] = NULL;
        DCBAA_[slot_id] = NULL;

        //  malloc関数が使えれば、デバイスコンテキストやインプットコンテキストの領域を
        //  解放する必要がある。今回はやらないの。
    }

    Device *DeviceManager::FindByPort(uint8_t port_num)
    {
        Device *dev;
        for (int i = 1; i <= max_slots_; i++) {
            dev = devices_[i];
            if (dev == NULL) continue;
            if (dev->DeviceContextPtr()->slot_context.bits.root_hub_port_num == port_num) {
                return dev; 
            }
        }
        return NULL;
    }

    Device *DeviceManager::FindBySlot(uint8_t slot_id)
    {
        if (slot_id > max_slots_) return NULL;

        return devices_[slot_id]; // デバイスがない場合NULL
    }

    int DeviceManager::AllocDevice(uint8_t slot_id, DoorbellRegister* doorbell)
    {
        if (slot_id > max_slots_) {
            logger->error("[DeviceManager::AllocDevice] SlotID%02hhd Is Out Of Range.\n", slot_id);
            return -1;
        }
        if (devices_[slot_id] != NULL) { // 指定したスロットがすでに使用されている場合は−１を返す。
            logger->error("[DeviceManager::AllocDevice] Slot%02hhd Is Not Free.\n", slot_id);
            return -1;
        }

        logger->debug("[DeviceManager::AllocDevice] SlotID: %02hhdd\n", slot_id);
        devices_[slot_id] = new Device(slot_id, doorbell);
        devices_[slot_id]->Initialize();

        DCBAA_[slot_id] = devices_[slot_id]->DeviceContextPtr();
        return 0;
    }   

    int DeviceManager::LoadDCBAA(uint8_t slot_id)
    {
        if (slot_id > max_slots_) return -1;
        if (devices_[slot_id] == NULL) return -1;

        DCBAA_[slot_id] = devices_[slot_id]->DeviceContextPtr();
        return 0;
    }
}
 