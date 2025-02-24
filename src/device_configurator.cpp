#include "device_configurator.h"
#include <iostream>
#include <chrono>
#include <thread>

#define GYRO_BIAS_INITIAL_DURATION 5
#define GYRO_BIAS_INTERVAL 15
#define GYRO_BIAS_DURATION 3

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

    //wait for 20ms until sending next command, otherwise the initial gyro bias estimation won't work.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));


    // Create and store the gyro bias estimator
    m_estimator = std::make_unique<GyroBiasEstimator>(device);
    
    // Perform initial estimation for 6 seconds
    std::cout << "Performing initial gyro bias estimation for "<< GYRO_BIAS_INITIAL_DURATION <<" seconds..." << std::endl;
    if (!m_estimator->performSingleEstimation(GYRO_BIAS_INITIAL_DURATION)) {
        std::cout << "Initial gyro bias estimation failed." << std::endl;
        return false;
    }

    // Start periodic estimation (every GYRO_BIAS_INTERVAL seconds with GYRO_BIAS_DURATION seconds duration)
    std::cout << "Start Periodic estimation with interval: " << GYRO_BIAS_INTERVAL << "s, duration: " << GYRO_BIAS_DURATION << "s." << std::endl;
    m_estimator->startPeriodicEstimation(GYRO_BIAS_INTERVAL, GYRO_BIAS_DURATION);

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
        // configArray.push_back(XsOutputConfiguration(XDI_AccelerationHR, 0xFFFF));
        // configArray.push_back(XsOutputConfiguration(XDI_RateOfTurnHR, 0xFFFF));
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