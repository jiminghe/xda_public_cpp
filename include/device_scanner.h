#ifndef DEVICE_SCANNER_H
#define DEVICE_SCANNER_H

#include <xscontroller/xscontrol_def.h>
#include <xscontroller/xsdevice_def.h>
#include <string>

class DeviceScanner {
public:
    // Single-sensor: creates and owns its own XsControl, connects to device index 0
    DeviceScanner();

    // Multi-sensor: uses a shared XsControl (not owned), connects to device at deviceIndex
    DeviceScanner(XsControl* sharedControl, int deviceIndex);

    ~DeviceScanner();

    bool scanAndConnect();
    XsDevice* getDevice() const { return m_device; }
    XsControl* getControl() const { return m_control; }

private:
    XsControl* m_control;
    XsDevice*  m_device;
    XsPortInfo m_portInfo;
    int        m_deviceIndex;
    bool       m_ownsControl;

    // Scans all ports and caches the portInfo for m_deviceIndex.
    // Called from the constructor so the scan happens before any port is opened.
    void preScan();

    static void setLowLatency(const std::string& portName);
};

#endif