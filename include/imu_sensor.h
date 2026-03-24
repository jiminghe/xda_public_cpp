#ifndef IMU_SENSOR_H
#define IMU_SENSOR_H

#include <memory>
#include <atomic>
#include "callback_handler.h"
#include "timestamped_packet.h"
#include "device_scanner.h"
#include "data_logger.h"
#include "device_configurator.h"

class ImuSensor {
public:
    // Single-sensor: creates its own XsControl, connects to the first MTi device found
    ImuSensor();

    // Multi-sensor: uses a shared XsControl from another ImuSensor, connects to device at deviceIndex
    // Pass sensor1.getControl() as sharedControl for sensor2, sensor3, etc.
    ImuSensor(XsControl* sharedControl, int deviceIndex);

    ~ImuSensor();

    // Call before initialize(). Enables XSF_SendLatest sync mode + PeriodicRequestScheduler. Default: true.
    void setSendLatestEnabled(bool enable) { m_configurator->setSendLatestEnabled(enable); }
    bool isSendLatestEnabled() const { return m_configurator->isSendLatestEnabled(); }

    bool initialize();
    bool startLogging();
    bool stopLogging();
    bool hasNewData() const;
    TimestampedPacket getLatestData();
    
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