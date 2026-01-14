#include "device_scanner.h"
#include <xscontroller/xsscanner.h>
#include <iostream>
#include <fstream>
#include <sstream>

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

bool DeviceScanner::readPortConfig(std::string& portName, int& baudrate)
{
    std::ifstream configFile("port.ini");
    if (!configFile.is_open()) {
        return false;
    }

    std::string line;
    bool foundPort = false;
    bool foundBaudrate = false;

    while (std::getline(configFile, line)) {
        // Remove whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Parse key=value pairs
        size_t equalPos = line.find('=');
        if (equalPos != std::string::npos) {
            std::string key = line.substr(0, equalPos);
            std::string value = line.substr(equalPos + 1);

            // Trim whitespace from key and value
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            if (key == "port" || key == "PORT") {
                portName = value;
                foundPort = true;
            }
            else if (key == "baudrate" || key == "BAUDRATE") {
                try {
                    baudrate = std::stoi(value);
                    foundBaudrate = true;
                }
                catch (const std::exception& e) {
                    std::cerr << "Invalid baudrate value in port.ini: " << value << std::endl;
                }
            }
        }
    }

    configFile.close();
    return foundPort && foundBaudrate;
}

bool DeviceScanner::createDefaultPortConfig()
{
    std::ofstream configFile("port.ini");
    if (!configFile.is_open()) {
        std::cerr << "Failed to create port.ini file" << std::endl;
        return false;
    }

    configFile << "# MTi Device Port Configuration\n";
    configFile << "# Edit this file to match your device's port and baudrate\n";
    configFile << "# Common baudrates: 115200, 230400, 460800, 921600\n";
    configFile << "\n";
    configFile << "port=COM3\n";
    configFile << "baudrate=115200\n";

    configFile.close();
    std::cout << "Created default port.ini file with COM3 @ 115200 baud" << std::endl;
    return true;
}

XsBaudRate DeviceScanner::numericToXsBaudRate(int baudrate)
{
    return XsBaud::numericToRate(baudrate);
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

    // Second attempt: try reading from port.ini file
    std::cout << "No MTi device found in initial scan, trying port.ini configuration..." << std::endl;
    
    std::string portName;
    int baudrate;

    // Check if port.ini exists, if not create it
    if (!readPortConfig(portName, baudrate)) {
        std::cout << "port.ini not found or invalid, creating default configuration..." << std::endl;
        if (!createDefaultPortConfig()) {
            return false;
        }
        // Read the newly created config
        if (!readPortConfig(portName, baudrate)) {
            std::cerr << "Failed to read newly created port.ini" << std::endl;
            return false;
        }
    }

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
        std::cerr << "   - port: Your COM port (e.g., COM3, COM4)" << std::endl;
        std::cerr << "   - baudrate: Device baudrate (e.g., 115200, 921600)" << std::endl;
        std::cerr << "\nCurrent port.ini settings:" << std::endl;
        std::cerr << "   port=" << portName << std::endl;
        std::cerr << "   baudrate=" << baudrate << std::endl;
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