#include "device_scanner.h"
#include <xscontroller/xsscanner.h>
#include <iostream>

DeviceScanner::DeviceScanner() : m_control(nullptr), m_device(nullptr)
{
    m_control = XsControl::construct();
}

DeviceScanner::~DeviceScanner()
{
    if (m_control) {
        m_control->closePort(m_portInfo.portName().toStdString());
        m_control->destruct();
    }
}


bool DeviceScanner::scanAndConnect()
{
    std::cout << "Scanning for devices..." << std::endl;
    XsPortInfoArray portInfoArray = XsScanner::scanPorts();

    // First attempt: scan all ports
    for (auto const &portInfo : portInfoArray)
    {
        if (portInfo.deviceId().isMti() || portInfo.deviceId().isMtig())
        {
            m_portInfo = portInfo;
            break;
        }
    }

    // If first attempt failed, try specific port scan
    if (m_portInfo.empty()) {
        std::cout << "No MTi device found in initial scan, trying specific port scan..." << std::endl;

        //change this port name and baudrate to your own port and baudrate(by default it is 115200, unless you had changed the value with MT Manager.)
        m_portInfo = XsScanner::scanPort("/dev/ttyUSB0", XBR_115k2); 
        
        // If still empty after specific scan
        if (m_portInfo.empty()) {
            std::cout << "No MTi device found on any port." << std::endl;
            return false;
        }
    }

    std::cout << "Found a device with ID: " << m_portInfo.deviceId().toString().toStdString() 
              << " @ port: " << m_portInfo.portName().toStdString() 
              << ", baudrate: " << m_portInfo.baudrate() << std::endl;

    if (!m_control->openPort(m_portInfo.portName().toStdString(), m_portInfo.baudrate())) {
        std::cout << "Could not open port." << std::endl;
        return false;
    }

    m_device = m_control->device(m_portInfo.deviceId());
    if (m_device == nullptr) {
        std::cout << "Could not create device object." << std::endl;
        return false;
    }

    return true;
}