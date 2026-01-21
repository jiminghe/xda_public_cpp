#ifndef IMU_SENSOR_H
#define IMU_SENSOR_H

#include <memory>
#include <atomic>
#include "callback_handler.h"
#include "device_scanner.h"
#include "data_logger.h"
#include "device_configurator.h"

class ImuSensor {
public:
    ImuSensor();
    ~ImuSensor();

    bool initialize();
    bool startGyroBiasEstimation(); 
    bool startLogging();
    bool stopLogging();
    bool hasNewData() const;
    XsDataPacket getLatestData();
    
    void startMeasurement();
    void stopMeasurement();
    
    XsDevice* getDevice() const { return m_deviceScanner ? m_deviceScanner->getDevice() : nullptr; }
    XsControl* getControl() const { return m_deviceScanner ? m_deviceScanner->getControl() : nullptr; }
    bool isConnected() const { return m_isConnected; }
    bool isMeasuring() const { return m_isMeasuring; }

    // callback for device disconnection
    using DisconnectionCallback = std::function<void()>;
    void setDisconnectionCallback(DisconnectionCallback callback) {
        m_disconnectionCallback = callback;
    }

private:
    std::unique_ptr<DeviceScanner> m_deviceScanner;
    std::unique_ptr<CallbackHandler> m_callback;
    std::unique_ptr<DeviceConfigurator> m_configurator;
    std::atomic<bool> m_isMeasuring{false};
    std::atomic<bool> m_isConnected{true};
    DisconnectionCallback m_disconnectionCallback;

    void handleError(XsDevice* device, XsResultValue error);
};

#endif