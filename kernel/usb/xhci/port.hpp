#pragma once 





#include <stdint.h>
#include "registers.hpp"

namespace usb::xhci
{
    enum PortSpeed
    {
        kFullSpeed = 1,
        kLowSpeed = 2,
        kHighSpeed = 3,
        kSuperSpeed = 4,
        kSuperSpeedPlus = 5,
    };
    
    //  ポート関連のレジスタを保持するクラス
    //  このクラスのオブジェクトを生成すると、その時点での
    //  PortRegisterSetの内容が反映され、各メンバ関数を用いてそのデータを読み書きすることができる。
    //  内部で状態を保つことはないので、何回もオブジェクトを生成しても、
    //  影響があるのはレジスタの値だけである。
    class Port
    {
    public:
        Port(uint8_t port_num, PortRegisterSet *port_reg_set);

        uint8_t Number();
        uint32_t Speed();
        //  PORTSCのCCSの値を返す。
        //  つまり、このポートが接続状態ならtrueを、そうでないならfalseを返す。
        bool IsConnected();
        //  PORTSCのPEDの値を返す。
        //  つまり、このポートがEnable状態ならtrueを、
        //  disabled状態ならfalseを返す。
        bool IsEnabled();
        //  trueの場合、CCS（Current Connect Status）の値が変化したことを知らせる。
        bool IsConnectStatusChanged();
        //  trueの場合、そのポートのリセット処理が成功したことを知らせる。
        bool IsPortResetChanged();
        bool IsPortLinkStateChanged();
        //  ポートのレジスタPORTSCのPRフィールドを１にすることで、
        //  ポートのリセット処理を始めさせる。
        //  無事この関数を終了することができたら、ポートの状態がEnabledに変わり、
        //  PortStatusChangeEventが発行されているはずである。
        int Reset();
        //  CSCフィールドに１を立てて値を０クリアする。
        void ClearConnectStatusChanged();
        //  PORTSCのport_reset_changeを１にすることで、このフィールド値を０にする。（特殊な場合）
        //  このレジスタのchange系列のフィールドは、状態変化が起きた時などに通知するために
        //  用いられるため、確認後０クリアする必要がある。
        void ClearPortResetChange();

    private:
        const uint8_t port_num_; // Portの番号
        PortRegisterSet *port_reg_set_; // ポート関連のレジスタ群
    };
}





