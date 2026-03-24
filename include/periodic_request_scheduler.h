#ifndef PERIODIC_REQUEST_SCHEDULER_H
#define PERIODIC_REQUEST_SCHEDULER_H

#include <xscontroller/xsdevice_def.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <cstdint>

/*!
 * Supported periodic request rates in Hz.
 * The scheduler fires requestData() to all registered devices
 * at exactly this frequency, using CLOCK_MONOTONIC absolute deadlines.
 */
enum class RequestRate {
    Hz1    = 1,
    Hz2    = 2,
    Hz4    = 4,
    Hz5   = 5,
    Hz10  = 10,
    Hz20  = 20,
    Hz25  = 25,
    Hz50  = 50,
    Hz100 = 100
};

/*!
 * High-accuracy periodic scheduler for XSF_SendLatest / XSL_ReqData mode.
 *
 * Usage (3 sensors example):
 *   PeriodicRequestScheduler scheduler;
 *   scheduler.registerDevice(sensor1.getDevice());
 *   scheduler.registerDevice(sensor2.getDevice());
 *   scheduler.registerDevice(sensor3.getDevice());
 *   scheduler.start(RequestRate::Hz50);
 *
 * A single background thread fires requestData() to every registered device
 * simultaneously so all sensors deliver data at the same moment.
 * Timing uses clock_nanosleep(TIMER_ABSTIME) to avoid accumulated drift.
 */
class PeriodicRequestScheduler {
public:
    PeriodicRequestScheduler();
    ~PeriodicRequestScheduler();

    // Register / unregister a device — safe to call before or after start()
    void registerDevice(XsDevice* device);
    void unregisterDevice(XsDevice* device);

    // Start the scheduler at the given rate; stops any previous run first
    bool start(RequestRate rate);
    void stop();

    bool isRunning() const { return m_isRunning.load(); }
    RequestRate getRate() const { return m_rate; }

private:
    std::vector<XsDevice*> m_devices;
    std::mutex              m_devicesMutex;
    std::thread             m_schedulerThread;
    std::atomic<bool>       m_isRunning{false};
    RequestRate             m_rate{RequestRate::Hz100};

    // Returns current CLOCK_MONOTONIC time in nanoseconds
    static uint64_t getAbsoluteTimeNs();

    // Main scheduler loop; periodNs = 1e9 / Hz
    void schedulerLoop(uint64_t periodNs);
};

#endif
