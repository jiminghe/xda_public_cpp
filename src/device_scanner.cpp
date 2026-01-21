#include "device_scanner.h"
#include <xscontroller/xsscanner.h>
#include <iostream>

DeviceScanner::DeviceScanner() 
    : m_control(nullptr)
    , m_device(nullptr)
    , m_configHandler(std::make_unique<ConfigHandler>())
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

XsBaudRate DeviceScanner::numericToXsBaudRate(int baudrate)
{
    return XsBaud::numericToRate(baudrate);
}

bool DeviceScanner::scanAndConnect()
{
    std::cout << "\nScanning for devices..." << std::endl;
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

    // If first attempt succeeded, connect and return
    if (!m_portInfo.empty()) {
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

    // Second attempt: try using port.ini configuration
    std::cout << "No MTi device found in initial scan, trying port.ini configuration..." << std::endl;
    
    std::string portName = m_configHandler->getPortName();
    int baudrate = m_configHandler->getBaudrate();

    std::cout << "Trying port " << portName << " @ " << baudrate << " baud..." << std::endl;
    
    XsBaudRate xsBaudRate = numericToXsBaudRate(baudrate);
    m_portInfo = XsScanner::scanPort(portName, xsBaudRate);

    // If still empty after port.ini scan
    if (m_portInfo.empty()) {
        std::cerr << "\n================================================" << std::endl;
        std::cerr << "ERROR: No MTi device found!" << std::endl;
        std::cerr << "================================================" << std::endl;
        std::cerr << "Please check the following:" << std::endl;
        std::cerr << "1. Device is connected to the computer" << std::endl;
        std::cerr << "2. Device is powered on" << std::endl;
        std::cerr << "3. Correct drivers are installed" << std::endl;
        std::cerr << "4. Edit 'port.ini' file with correct settings:" << std::endl;
        std::cerr << "   - port: Your COM port (e.g., COM3, COM4, /dev/ttyUSB0)" << std::endl;
        std::cerr << "   - baudrate: Device baudrate (e.g., 115200, 921600)" << std::endl;
        std::cerr << "   - setOutputConfig: 1 to configure device, 0 to skip" << std::endl;
        std::cerr << "\nCurrent port.ini settings:" << std::endl;
        std::cerr << "   port=" << portName << std::endl;
        std::cerr << "   baudrate=" << baudrate << std::endl;
        std::cerr << "   setOutputConfig=" << m_configHandler->getSetOutputConfig() << std::endl;
        std::cerr << "================================================\n" << std::endl;
        return false;
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