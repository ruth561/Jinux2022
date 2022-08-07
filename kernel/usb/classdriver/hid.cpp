#include "hid.hpp"
#include "../../logging.hpp"
extern logging::Logger *logger;

namespace
{
    union ModifierKey
    {
        uint8_t data;
        struct {
            uint8_t left_ctrl: 1;
            uint8_t left_shift: 1;
            uint8_t left_alt: 1;
            uint8_t left_gui: 1;
            uint8_t right_ctrl: 1;
            uint8_t right_shift: 1;
            uint8_t right_alt: 1;
            uint8_t right_gui: 1;
        } __attribute__((packed)) bits;
        ModifierKey(uint8_t key) : data{key} {}
    } __attribute__((packed));

    char kKeyCord[] = "\x00\x00\x00\x00" "abcdef" // 0~9
                      "ghijklmnop" // 10~19
                      "qrstuvwxyz" // 20~29
                      "1234567890" // 30~39
                      "\n\x1b\x7f\t -=[]\\" // 40~49
                      "\x00;\'\x00,./\x00\x00\x00" // 50~59
                      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" // 60~69 全てFキー
                      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" // 70~79 
                      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";// 80~89

    char kKeyCordShift[] = "\x00\x00\x00\x00" "ABCDEF" // 0~9
                      "GHIJKLMNOP" // 10~19
                      "QRSTUVWXYZ" // 20~29
                      "!@#$%^&*()" // 30~39
                      "\n\x1b\x7f\t _+{}|" // 40~49
                      "~:\"\x00<>?\x00\x00\x00" // 50~59
                      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" // 60~69 全てFキー
                      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" // 70~79 
                      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";// 80~89 
}

namespace usb
{
    void HIDKeyboardDriver::Run()
    {
        logger->debug("HID Class Driver RUN!!\n");
        SetBootProtocol();
    }

    void HIDKeyboardDriver::OnControlCompleted(EndpointID ep_id, SetupData setup_data, void *buf, int len)
    {
        if (setup_data.request == request::kSetProtocol) {
            logger->debug("Set Boot Protocol Done.\n");
            GetKeyInControlPipe();
            return;
        } else if (setup_data.request == request::kGetReport) {
            logger->debug("Received Data From HID Device!!\n");
            uint8_t *data;
            data = reinterpret_cast<uint8_t *>(buf);
/*             logger->info("BUF: %02hhx, %02hhx, %02hhx, %02hhx, %02hhx, %02hhx, %02hhx, %02hhx\n", 
                data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]); */
            
            ModifierKey modifier_key{data[0]};
            char key[2] = {0, 0};
            if (modifier_key.bits.left_shift || modifier_key.bits.right_shift) {
                for (int i = 2; i < 8; i++) {
                    if (data[i] < sizeof(kKeyCordShift)) {
                        key[0] = kKeyCordShift[data[i]];
                        printk(key);
                    }
                }
            } else {
                for (int i = 2; i < 8; i++) {
                    if (data[i] < sizeof(kKeyCord)) {
                        key[0] = kKeyCord[data[i]];
                        printk(key);
                    }
                }
            }

            GetKeyInControlPipe();
        }
    }

    void HIDKeyboardDriver::GetKeyInControlPipe()
    {
        SetupData setup_data = {};
        setup_data.request_type.data = 0b10100001;
        setup_data.request = request::kGetReport;
        setup_data.value = 0x100; // 入力レポート, ReportIDは指定しない
        setup_data.index = static_cast<uint16_t>(interface_number_) & 0xff; // |15  Reserved 8|7  Interface 0|という構造
        setup_data.length = 8;
        dev_->ControlIn(kDefaultControlPipeID, setup_data, buf_, sizeof(buf_));
        logger->debug("Send Report Request!! buf_: %p\n", buf_);
    }

    void HIDKeyboardDriver::SetBootProtocol()
    {
        SetupData setup_data = {};
        setup_data.request_type.data = 0b00100001;
        setup_data.request = request::kSetProtocol;
        setup_data.value = 0; // BootProtocol
        setup_data.index = static_cast<uint16_t>(interface_number_) & 0xff; // |15  Reserved 8|7  Interface 0|という構造
        setup_data.length = 0;
        dev_->ControlOut(kDefaultControlPipeID, setup_data, nullptr, 0);
        logger->debug("SET PROTOCOL!! index: %d\n", setup_data.index);
    }
}