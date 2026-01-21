#ifndef DEVICE_SCANNER_H
#define DEVICE_SCANNER_H
#include <xscontroller/xscontrol_def.h>
#include <xscontroller/xsdevice_def.h>
#include <string>
#include "config_handler.h"
#include <memory>

class DeviceScanner {
public:
    DeviceScanner();
    ~DeviceScanner();
    bool scanAndConnect();
    XsDevice* getDevice() const { return m_device; }
    XsControl* getControl() const { return m_control; }
    const ConfigHandler& getConfig() const { return *m_configHandler; }

private:
    XsControl* m_control;
    XsDevice* m_device;
    XsPortInfo m_portInfo;
    std::unique_ptr<ConfigHandler> m_configHandler;

    XsBaudRate numericToXsBaudRate(int baudrate);
};

#endif