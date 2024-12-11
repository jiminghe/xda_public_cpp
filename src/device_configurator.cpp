#include "device_configurator.h"
#include <iostream>

DeviceConfigurator::DeviceConfigurator() : m_estimator(nullptr) {}

DeviceConfigurator::~DeviceConfigurator() {
    if (m_estimator) {
        m_estimator->stopPeriodicEstimation();
    }
}

void DeviceConfigurator::stopGyroBiasEstimation() {
    if (m_estimator) {
        m_estimator->stopPeriodicEstimation();
    }
}

bool DeviceConfigurator::configureDevice(XsDevice* device)
{
    std::cout << "Putting device into configuration mode..." << std::endl;
    if (!device->gotoConfig()) {
        std::cout << "Could not put device into configuration mode." << std::endl;
        return false;
    }

    std::cout << "Configuring the device..." << std::endl;
    device->readEmtsAndDeviceConfiguration();
    XsOutputConfigurationArray configArray = createConfigArray(device);
    
    if (!device->setOutputConfiguration(configArray)) {
        std::cout << "Could not configure MTi device." << std::endl;
        return false;
    }

    std::cout << "Putting device into measurement mode..." << std::endl;
    if (!device->gotoMeasurement()) {
        std::cout << "Could not put device into measurement mode." << std::endl;
        return false;
    }

    // Create and store the gyro bias estimator
    m_estimator = std::make_unique<GyroBiasEstimator>(device);
    
    // Perform initial estimation for 6 seconds
    std::cout << "Performing initial gyro bias estimation for 6 seconds..." << std::endl;
    if (!m_estimator->performSingleEstimation(6)) {
        std::cout << "Initial gyro bias estimation failed." << std::endl;
        return false;
    }

    // Start periodic estimation (every 15 seconds with 3 seconds duration)
    m_estimator->startPeriodicEstimation(15, 3);

    return true;
}

XsOutputConfigurationArray DeviceConfigurator::createConfigArray(XsDevice* device)
{
    XsOutputConfigurationArray configArray;
    configArray.push_back(XsOutputConfiguration(XDI_PacketCounter, 0));
    configArray.push_back(XsOutputConfiguration(XDI_SampleTimeFine, 0));

    if (device->deviceId().isImu()) {
        configArray.push_back(XsOutputConfiguration(XDI_Acceleration, 100));
        configArray.push_back(XsOutputConfiguration(XDI_RateOfTurn, 100));
        configArray.push_back(XsOutputConfiguration(XDI_MagneticField, 100));
    }
    else if (device->deviceId().isVru() || device->deviceId().isAhrs()) {
        configArray.push_back(XsOutputConfiguration(XDI_Quaternion, 100));
        configArray.push_back(XsOutputConfiguration(XDI_FreeAcceleration, 100));
        configArray.push_back(XsOutputConfiguration(XDI_Acceleration, 100));
        configArray.push_back(XsOutputConfiguration(XDI_RateOfTurn, 100));
        configArray.push_back(XsOutputConfiguration(XDI_MagneticField, 100));
    }
    else if (device->deviceId().isGnss()) {
        configArray.push_back(XsOutputConfiguration(XDI_Quaternion, 100));
        configArray.push_back(XsOutputConfiguration(XDI_FreeAcceleration, 100));
        configArray.push_back(XsOutputConfiguration(XDI_Acceleration, 100));
        configArray.push_back(XsOutputConfiguration(XDI_RateOfTurn, 100));
        configArray.push_back(XsOutputConfiguration(XDI_MagneticField, 100));
        configArray.push_back(XsOutputConfiguration(XDI_LatLon | XDI_SubFormatFp1632, 100));
        configArray.push_back(XsOutputConfiguration(XDI_AltitudeEllipsoid | XDI_SubFormatFp1632, 100));
        configArray.push_back(XsOutputConfiguration(XDI_VelocityXYZ | XDI_SubFormatFp1632, 100));
    }

    configArray.push_back(XsOutputConfiguration(XDI_StatusWord, 0));

    return configArray;
}