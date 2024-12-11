#ifndef DEVICE_SCANNER_H
#define DEVICE_SCANNER_H

#include <xscontroller/xscontrol_def.h>
#include <xscontroller/xsdevice_def.h>
#include <string>

class DeviceScanner {
public:
    DeviceScanner();
    ~DeviceScanner();

    bool scanAndConnect();
    XsDevice* getDevice() const { return m_device; }
    XsControl* getControl() const { return m_control; }

private:
    XsControl* m_control;
    XsDevice* m_device;
    XsPortInfo m_portInfo;
};

#endif