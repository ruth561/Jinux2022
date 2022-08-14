#include "port.hpp"
#include "../../logging.hpp"
extern logging::Logger *logger; 

int printk(const char *format, ...);


namespace usb::xhci
{
    Port::Port(uint8_t port_num, PortRegisterSet *port_reg_set) :
        port_num_(port_num), 
        port_reg_set_(port_reg_set) {}

    uint8_t Port::Number()
    {
        return port_num_;
    }

    uint32_t Port::Speed()
    {
        return port_reg_set_->PORTSC.bits.port_speed;
    }

    bool Port::IsConnected()
    {
        return static_cast<bool>(port_reg_set_->PORTSC.bits.current_connect_status);
    }

    bool Port::IsEnabled()
    {
        return static_cast<bool>(port_reg_set_->PORTSC.bits.port_enabled_disabled);
    }

    bool Port::IsConnectStatusChanged()
    {
        return static_cast<bool>(port_reg_set_->PORTSC.bits.connect_status_change);
    }

    bool Port::IsPortResetChanged()
    {
        return (bool) port_reg_set_->PORTSC.bits.port_reset_change;
    }

    bool Port::IsPortLinkStateChanged()
    {
        return (bool) port_reg_set_->PORTSC.bits.port_link_state_change;
    }

    int Port::Reset()
    {
        if (IsConnected()) {
            PORTSC_Bitmap portsc;
            portsc.data[0] = port_reg_set_->PORTSC.data[0] & 0x0e01c3e0u;
            portsc.bits.port_reset = 1;
            portsc.bits.connect_status_change = 1;
            port_reg_set_->PORTSC.data[0] = portsc.data[0];

            printk("[Port::Reset()] Port%02hhd Start Reseting...\n", Number());
            while (port_reg_set_->PORTSC.bits.port_reset);
            printk("[Port::Reset()] Port%02hhd Finish Reseting...\n", Number());

            return 0;
        }
        return -1;   
    }

    void Port::ClearConnectStatusChanged()
    {
        PORTSC_Bitmap portsc;
        portsc.data[0] = port_reg_set_->PORTSC.data[0] & 0x0e01c3e0u;
        portsc.bits.connect_status_change = 1;
        port_reg_set_->PORTSC.data[0] = portsc.data[0];
    }
    
    void Port::ClearPortResetChange()
    {
        // logger->debug("[Port::ClearPortResetChange()] PRC=%d\n", port_reg_set_->PORTSC.bits.port_reset_change);
        PORTSC_Bitmap portsc;
        portsc.data[0] = port_reg_set_->PORTSC.data[0] & 0x0e01c3e0u;
        portsc.bits.port_reset_change = 1;
        port_reg_set_->PORTSC.data[0] = portsc.data[0];
        // logger->debug("[Port::ClearPortResetChange()] PRC=%d\n", port_reg_set_->PORTSC.bits.port_reset_change);
    }
}
 