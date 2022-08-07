#pragma once

#include <cstdint>


namespace usb
{
    //  一つのデバイスに対しひとつだけ存在するデータ構造。
    //  デバイスに関する一般的な情報を保持する。
    struct DeviceDescriptor
    {
        //  このディスクリプタのサイズ（バイト）
        uint8_t length;
        //  このディスクリプタのタイプ
        uint8_t descriptor_type;
        //  USBバージョンを表す。
        uint16_t usb_release;
        //  デバイスのクラスコード
        uint8_t device_class;
        uint8_t device_sub_class;
        uint8_t device_protocol;
        // EP０における最大パケットサイズで、２のべきで表される。
        uint8_t max_packet_size;
        uint16_t vendor_id;
        uint16_t product_id;
        uint16_t device_release; 
        uint8_t manufacturer;
        uint8_t product;
        uint8_t serial_number; 
        uint8_t num_configurations; 
    } __attribute__((packed));

    struct ConfigurationDescriptor
    {
        //  このディスクリプタのサイズ（バイト）
        uint8_t length;
        uint8_t descriptor_type;
        //  今回のコントロール転送で返ってきたデータ全てのサイズ（バイト）
        //  lengthがこのディスクリプタの大きさであるのに対して、
        //  この値は、INTERFACEやENDPOINTなどのディスクリプタも含めたサイズになっている。
        uint16_t total_length;
        //  インターフェースの数
        uint8_t num_interfaces;
        //  SET_CONFIGURATIONで使用するフィールド。
        uint8_t configuration_value;
        //  このディスクリプタを表現するStringDescriptorのインデックス。
        uint8_t configuration_id;
        uint8_t attributes;
        uint8_t max_power;
    } __attribute__((packed));

    struct InterfaceDescriptor
    {
        //  このディスクリプタのサイズ（バイト）
        uint8_t length;      
        //  このディスクリプタの種類（４）       
        uint8_t descriptor_type;
        //  この機能に関連するインターフェースの番号。
        //  ０を基準に同時使用可能なインターフェースの配列のインデックスを保持する。
        uint8_t interface_number;
        uint8_t alternate_setting;
        //  このインターフェースが使用するエンドポイントの数
        uint8_t num_endpoints;
        //  インターフェースクラス。USB-IFでによって割り当てられた値。
        //  この値が0xFFの時、ベンダー固有のものであることを示している。
        uint8_t interface_class;
        //  インターフェースのサブクラス。USB-IFでによって割り当てられた値。
        uint8_t interface_sub_class;
        uint8_t interface_protocol;
        //  このインターフェースに関するstring descriptorの番号。
        uint8_t interface_id;       
    } __attribute__((packed));
    
    struct EndpointDescriptor
    {
        uint8_t length;
        uint8_t descriptor_type;
        union {
            uint8_t data;
            struct {
                uint8_t number : 4;
                uint8_t : 3;
                uint8_t dir_in : 1;
            } __attribute__((packed)) bits;
        } endpoint_address;
        union {
            uint8_t data;
            struct {
                uint8_t transfer_type : 2;
                uint8_t sync_type : 2;
                uint8_t usage_type : 2;
                uint8_t : 2;
            } __attribute__((packed)) bits;
        } attributes;
        uint16_t max_packet_size;
        uint8_t interval;
    } __attribute__((packed));
    
    //  HIDはHuman Interface Deviceの頭文字。
    struct HIDDescriptorHeader
    {
        //  このディスクリプタのサイズ（バイト）
        uint8_t length;
        //  ３３
        //  他のディスクリプタと見分けるための値。
        uint8_t descriptor_type;
        //  バージョン
        uint16_t hid_release;
        //  国名コードを数字で表している。
        //  ほとんどのハードウェアは、国がどこであるかは関係しないため０の値になる。
        //  キーボードなどの、国によってキーキャップが変わったりする場合は、
        //  対応する国のカントリーコードの値になる。
        uint8_t country_code;
        //  クラスディスクリプタの数。HIDディスクリプタには少なくとも１つの
        //  クラスディスクリプタが存在しているので、この値は１以上になる。
        uint8_t num_descriptors;
    } __attribute__((packed));

    //  HIDディスクリプタの中にあるクラスディスクリプタの構造体。
    struct HIDClassDescriptor
    {
        uint8_t descriptor_type;
        uint16_t descriptor_length;
    } __attribute__((packed));

    //  HIDディスクリプタは可変なデータ構造なので、それを管理するクラスを作った。
    class HIDDescriptorReader
    {
    public:
        HIDDescriptorReader(HIDDescriptorHeader *desc) : 
            desc_(desc), 
            class_desc_head_((HIDClassDescriptor *) (desc + 1)) {}
        
        //  指定した番号のクラスディスクリプタへのポインタを返す。
        HIDClassDescriptor *GetClassDescriptor(uint8_t class_num)
        {
            if (class_num < desc_->num_descriptors) {
                return class_desc_head_ + class_num;
            }

            return nullptr;
        }

        const HIDDescriptorHeader *desc_;
        HIDClassDescriptor *class_desc_head_;
    };

}

