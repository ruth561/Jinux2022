#pragma once 

#include "../endpoint.hpp"
#include "../setupdata.hpp"


namespace usb
{
    class ClassDriver
    {
    public:
        // Configured状態になった時に呼ばれる。
        // ひたすらポーリングして情報を得ようとする
        virtual void Run() {}

        // コントロール転送への返信が来た時に呼ばれる
        virtual void OnControlCompleted(EndpointID ep_id, SetupData setup_data, void *buf, int len) {}
    };
}