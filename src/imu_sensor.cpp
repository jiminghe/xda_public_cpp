#include "imu_sensor.h"
#include <iostream>

ImuSensor::ImuSensor()
    : m_deviceScanner(std::make_unique<DeviceScanner>())
    , m_callback(std::make_unique<CallbackHandler>())
    , m_configurator(std::make_unique<DeviceConfigurator>())
{
    // Set up error callback
    m_callback->setErrorCallback([this](XsDevice* dev, XsResultValue error) {
        this->handleError(dev, error);
    });
}

ImuSensor::~ImuSensor()
{
    if (m_isMeasuring) {
        stopMeasurement();
    }
    
    if (auto device = getDevice()) {
        device->removeCallbackHandler(m_callback.get());
    }
}

bool ImuSensor::initialize()
{
    // Scan and connect
    if (!m_deviceScanner->scanAndConnect()) {
        std::cout << "Failed to scan and connect to device" << std::endl;
        return false;
    }

    auto device = getDevice();
    if (!device) {
        std::cout << "Device not available" << std::endl;
        return false;
    }

    // Add callback handler
    device->addCallbackHandler(m_callback.get());

    // Get setOutputConfig setting from config handler
    bool setOutputConfig = m_deviceScanner->getConfig().getSetOutputConfig();
    std::cout << "Output configuration will be " 
              << (setOutputConfig ? "SET" : "SKIPPED") << std::endl;

    // Configure device with the setOutputConfig parameter
    if (!m_configurator->configureDevice(device, setOutputConfig)) {
        std::cout << "Failed to configure device" << std::endl;
        return false;
    }

    return true;
}

bool ImuSensor::startLogging()
{
    auto device = getDevice();
    if (!device) {
        std::cout << "Device not connected" << std::endl;
        return false;
    }

    return DataLogger::createLogFile(device);
}

bool ImuSensor::stopLogging()
{
    auto device = getDevice();
    if (!device) {
        return false;
    }

    return DataLogger::stopAndCloseLog(device);
}

bool ImuSensor::hasNewData() const
{
    return m_callback->packetAvailable();
}

XsDataPacket ImuSensor::getLatestData()
{
    return m_callback->getNextPacket();
}

void ImuSensor::startMeasurement()
{
    if (!getDevice() || m_isMeasuring) {
        return;
    }

    m_isMeasuring = true;
    std::cout << "Starting measurement..." << std::endl;
}

void ImuSensor::stopMeasurement()
{
    if (!getDevice() || !m_isMeasuring) {
        return;
    }

    m_isMeasuring = false;
    if (m_configurator) {
        m_configurator->stopGyroBiasEstimation();
    }
    std::cout << "Measurement stopped." << std::endl;
}


void ImuSensor::handleError(XsDevice* device, XsResultValue error)
{
    if (device == getDevice()) {
        if (error == XRV_EXPECTED_DISCONNECT) {
            m_isConnected = false;
            std::cout << "Device disconnected!" << std::endl;
            
            if (m_isMeasuring) {
                stopMeasurement();
            }
            
            if (m_disconnectionCallback) {
                m_disconnectionCallback();
            }
        }
        else if (error == XRV_DATAOVERFLOW) {
            std::cout << XsResultValue_toString(error) << ", change your baudrate to higher value! " << std::endl;
        }
    }
}

bool ImuSensor::startGyroBiasEstimation() {
    if (m_configurator) {
        return m_configurator->startGyroBiasEstimation();
    }
    return false;
}