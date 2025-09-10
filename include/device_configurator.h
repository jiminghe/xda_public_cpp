#ifndef DEVICE_CONFIGURATOR_H
#define DEVICE_CONFIGURATOR_H

#include <xscontroller/xsdevice_def.h>
#include <xstypes/xsoutputconfigurationarray.h>
#include <xstypes/xstimeinfo.h>
#include "gyro_bias_estimator.h"
#include <memory>

class DeviceConfigurator {
public:
    DeviceConfigurator();
    ~DeviceConfigurator();
    bool configureDevice(XsDevice* device);
    void stopGyroBiasEstimation();
    bool setUtcTime(XsDevice* device);

private:
    XsOutputConfigurationArray createConfigArray(XsDevice* device);
    XsTimeInfo getCurrentUtcTime();
    std::unique_ptr<GyroBiasEstimator> m_estimator;
};

#endif