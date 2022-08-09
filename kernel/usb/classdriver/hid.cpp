#include "hid.hpp"
#include "../../logging.hpp"
#include "../../timer.hpp"
extern logging::Logger *logger;
extern TimerManager *timer_manager;
void Halt(); 

namespace
{
    // キーを押す間隔を取り決める
    // 値が大きいほど長押し中の文字認識料が減る
    const uint32_t kKeyStrokeInterval = 150;

    // HID Keyboardでデータ構造で先頭の１Byteに置かれるもの
    // Shiftキーを押下しているかどうかやALtキーについての情報が得られる
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

    // Shiftキーが押されていない状態でキーを押された時に出力する文字
    // 文字コードとの対応表であり、kKeyCord[key_num]で得られるcharが文字となる
    char kKeyCord[] = "\x00\x00\x00\x00" "abcdef" // 0~9
                      "ghijklmnop" // 10~19
                      "qrstuvwxyz" // 20~29
                      "1234567890" // 30~39
                      "\n\x00\x00\00 -=[]\\" // 40~49
                      "\x00;\'\x00,./\x00\x00\x00" // 50~59
                      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" // 60~69 全てFキー
                      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" // 70~79 
                      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";// 80~89

    // Shiftキーが押されているバージョン
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
        last_tick_ = timer_manager->CurrentTick();
        key_stroke_interval_ = kKeyStrokeInterval;
        SetBootProtocol();
    }

    void HIDKeyboardDriver::OnEndpointsConfigured()
    {
        Run();
    }

    void HIDKeyboardDriver::OnControlCompleted(EndpointID ep_id, SetupData setup_data, void *buf, int len)
    {
        if (setup_data.request == request::kSetProtocol) {
            logger->debug("Set Boot Protocol Done.\n");
            GetKeyInControlPipe();
            // RequestKeyViaIntEP();
            return;
        } else if (setup_data.request == request::kGetReport) {
            logger->debug("Received Data From HID Device!!\n");
            uint8_t *data;
            data = reinterpret_cast<uint8_t *>(buf);
/*             logger->info("BUF: %02hhx, %02hhx, %02hhx, %02hhx, %02hhx, %02hhx, %02hhx, %02hhx\n", 
                data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]); */
            
            ModifierKey modifier_key{data[0]};
            char key[2] = {0, 0};
            uint32_t current_tick = timer_manager->CurrentTick(); 
            if (current_tick < last_tick_ + key_stroke_interval_) {
                // logger->info("Current Tick: %d, Last Tick: %d\n", current_tick, last_tick_);
                // 最後に入力検知をしてから十分な時間が立っていなければ出力しない
                GetKeyInControlPipe();
                return;
            }
            last_tick_ = current_tick; // キー入力の時間を更新

            if (modifier_key.bits.left_shift || modifier_key.bits.right_shift) {
                // 入力を検知した文字を全て出力する
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
        // Halt();
        logger->set_level(logging::kINFO);
        // logger->debug("Send Report Request!! buf_: %p\n", buf_);
    }

    void HIDKeyboardDriver::RequestKeyViaIntEP()
    {
        dev_->InterruptIn(EndpointID{xhci::DeviceContextIndex{3}}, buf_, sizeof(buf_));
        logger->debug("-------- RequestKeyViaIntEP() ----------\n");
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