#ifndef DEVICE_CONFIGURATOR_H
#define DEVICE_CONFIGURATOR_H

#include <xscontroller/xsdevice_def.h>
#include <xstypes/xsoutputconfigurationarray.h>
#include "gyro_bias_estimator.h"
#include <memory>
#include <string>

class DeviceConfigurator {
public:
    DeviceConfigurator();
    ~DeviceConfigurator();

    // Full configuration: output config + sync SendLatest + gyro bias estimation
    bool configureDevice(XsDevice* device);

    // Configure sync setting: XSL_ReqData + XSF_SendLatest.
    // Device must be in config mode. Called internally by configureDevice().
    bool configureSyncSendLatest(XsDevice* device);

    void stopGyroBiasEstimation();

    // Product code queried during configureDevice() while in config mode
    const std::string& productCode() const { return m_productCode; }

private:
    XsOutputConfigurationArray createConfigArray(XsDevice* device);
    std::unique_ptr<GyroBiasEstimator> m_estimator;
    std::string m_productCode;
};

#endif