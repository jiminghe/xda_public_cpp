#ifndef PERIODIC_REQUEST_SCHEDULER_H
#define PERIODIC_REQUEST_SCHEDULER_H

#include <xscontroller/xsdevice_def.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cstdint>

class CallbackHandler;

/*!
 * Supported periodic request rates in Hz.
 */
enum class RequestRate {
    Hz1    = 1,
    Hz2    = 2,
    Hz4    = 4,
    Hz5    = 5,
    Hz10   = 10,
    Hz20   = 20,
    Hz25   = 25,
    Hz50   = 50,
    Hz100  = 100
};

/*!
 * High-accuracy periodic scheduler for XSF_SendLatest / XSL_ReqData mode.
 *
 * One worker thread per device. The timer thread records a single batch
 * timestamp, stores it on every device's CallbackHandler via setBatchTimestamp(),
 * then calls notify_all() to wake all workers simultaneously. Each worker calls
 * requestData() immediately. Because all devices in the batch share the same
 * trigger timestamp, their TimestampedPacket::receiveTimeNs values are identical
 * regardless of when the SDK delivers each callback.
 *
 * Register devices via ImuSensor::registerWithScheduler() which passes both the
 * XsDevice* and the internal CallbackHandler* in one call.
 */
class PeriodicRequestScheduler {
public:
    PeriodicRequestScheduler();
    ~PeriodicRequestScheduler();

    // Register a device+handler pair — call before start()
    void registerDevice(XsDevice* device, CallbackHandler* handler);
    void unregisterDevice(XsDevice* device);

    bool start(RequestRate rate);
    void stop();

    bool isRunning() const { return m_isRunning.load(); }
    RequestRate getRate() const { return m_rate; }

private:
    struct DeviceEntry {
        XsDevice*        device;
        CallbackHandler* handler;
    };

    std::vector<DeviceEntry>  m_devices;
    std::mutex                m_devicesMutex;

    std::vector<std::thread>  m_workerThreads;
    std::thread               m_schedulerThread;

    std::condition_variable   m_triggerCV;
    std::mutex                m_triggerMutex;
    std::atomic<uint64_t>     m_triggerCount{0};
    std::atomic<uint64_t>     m_batchTimestamp{0}; // set once per period, shared by all workers

    std::atomic<bool>         m_isRunning{false};
    RequestRate               m_rate{RequestRate::Hz100};

    static uint64_t getAbsoluteTimeNs();
    void schedulerLoop(uint64_t periodNs);
    void workerLoop(XsDevice* device, CallbackHandler* handler);
};

#endif
