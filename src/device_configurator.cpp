#include "device_configurator.h"
#include <xstypes/xssyncsettingarray.h>
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

    // Query product code while still in config mode
    m_productCode = device->productCode().toStdString();

    XsOutputConfigurationArray configArray = createConfigArray(device);
    
    if (!device->setOutputConfiguration(configArray)) {
        std::cout << "Could not configure MTi device." << std::endl;
        return false;
    }

    if (m_sendLatestEnabled) {
        if (!configureSyncSendLatest(device)) {
            std::cout << "Could not configure sync SendLatest." << std::endl;
            return false;
        }
    } else {
        // Clear any sync settings left from a previous run (e.g. a prior SendLatest session)
        if (!device->setSyncSettings(XsSyncSettingArray())) {
            std::cout << "Warning: could not clear sync settings." << std::endl;
        } else {
            std::cout << "Sync settings cleared (continuous streaming mode)." << std::endl;
        }
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

bool DeviceConfigurator::configureSyncSendLatest(XsDevice* device)
{
    // XSL_ReqData: serial sync line triggered by XMID_ReqData message (Mk4)
    // XSF_SendLatest: on trigger, transmit the latest sampled packet
    // Device must already be in config mode when this is called.
    XsSyncSettingArray syncSettings;
    syncSettings.push_back(XsSyncSetting(
        XSL_ReqData,        // line:        serial ReqData trigger
        XSF_SendLatest,     // function:    send latest sample on trigger
        XSP_RisingEdge,     // polarity:    (unused for serial line, kept as default)
        1000,               // pulseWidth:  1 ms (unused for serial line)
        0,                  // offset:      no delay
        0,                  // skipFirst:   no frames skipped before first action
        0,                  // skipFactor:  act on every trigger
        0,                  // clockPeriod: not used (ClockBiasEstimation only)
        0                   // triggerOnce: repeat on every trigger
    ));

    if (!device->setSyncSettings(syncSettings)) {
        std::cout << "Could not set sync settings (SendLatest / ReqData)." << std::endl;
        return false;
    }

    std::cout << "Sync configured: XSL_ReqData + XSF_SendLatest (send_latest mode)." << std::endl;
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