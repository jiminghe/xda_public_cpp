#include "device_scanner.h"
#include <xscontroller/xsscanner.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <iostream>

DeviceScanner::DeviceScanner()
    : m_control(XsControl::construct())
    , m_device(nullptr)
    , m_deviceIndex(0)
    , m_ownsControl(true)
{
    preScan();
}

DeviceScanner::DeviceScanner(XsControl* sharedControl, int deviceIndex)
    : m_control(sharedControl)
    , m_device(nullptr)
    , m_deviceIndex(deviceIndex)
    , m_ownsControl(false)
{
    preScan();
}

DeviceScanner::~DeviceScanner()
{
    if (m_control) {
        m_control->closePort(m_portInfo.portName().toStdString());
        if (m_ownsControl) {
            m_control->destruct();
        }
    }
}


void DeviceScanner::preScan()
{
    // Scan is done here (in the constructor) so all sensors scan before any port is opened.
    // If sensor2 scanned inside initialize(), sensor1's port would already be open
    // and the scanner would not find it, leaving only one visible device.
    XsPortInfoArray portInfoArray = XsScanner::scanPorts();
    int found = 0;
    for (auto const& portInfo : portInfoArray)
    {
        if (portInfo.deviceId().isMti() || portInfo.deviceId().isMtig())
        {
            if (found == m_deviceIndex) {
                m_portInfo = portInfo;
                return;
            }
            ++found;
        }
    }
    std::cout << "Warning: no MTi device found at index " << m_deviceIndex
              << " during pre-scan (" << found << " device(s) visible)." << std::endl;
}

bool DeviceScanner::scanAndConnect()
{
    if (m_portInfo.empty()) {
        std::cout << "No MTi device found at index " << m_deviceIndex << "." << std::endl;
        return false;
    }

    std::cout << "Found a device with ID: " << m_portInfo.deviceId().toString().toStdString()
              << " @ port: " << m_portInfo.portName().toStdString()
              << ", baudrate: " << m_portInfo.baudrate() << std::endl;

    setLowLatency(m_portInfo.portName().toStdString());

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

void DeviceScanner::setLowLatency(const std::string& portName)
{
    int fd = open(portName.c_str(), O_RDWR | O_NONBLOCK | O_NOCTTY);
    if (fd < 0) {
        std::cout << "setLowLatency: could not open " << portName << std::endl;
        return;
    }

    struct serial_struct ser;
    if (ioctl(fd, TIOCGSERIAL, &ser) < 0) {
        std::cout << "setLowLatency: TIOCGSERIAL failed on " << portName << std::endl;
        close(fd);
        return;
    }

    ser.flags |= ASYNC_LOW_LATENCY;

    if (ioctl(fd, TIOCSSERIAL, &ser) < 0) {
        std::cout << "setLowLatency: TIOCSSERIAL failed on " << portName
                  << " (try running as root or with CAP_SYS_ADMIN)" << std::endl;
    } else {
        std::cout << "Low latency mode enabled on " << portName << std::endl;
    }

    close(fd);
}