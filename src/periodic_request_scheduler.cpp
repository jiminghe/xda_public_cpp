#include "periodic_request_scheduler.h"
#include "callback_handler.h"
#include <algorithm>
#include <iostream>
#include <time.h>

PeriodicRequestScheduler::PeriodicRequestScheduler() {}

PeriodicRequestScheduler::~PeriodicRequestScheduler()
{
    stop();
}

void PeriodicRequestScheduler::registerDevice(XsDevice* device, CallbackHandler* handler)
{
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    m_devices.push_back({device, handler});
}

void PeriodicRequestScheduler::unregisterDevice(XsDevice* device)
{
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    m_devices.erase(
        std::remove_if(m_devices.begin(), m_devices.end(),
            [device](const DeviceEntry& e) { return e.device == device; }),
        m_devices.end());
}

bool PeriodicRequestScheduler::start(RequestRate rate)
{
    if (m_isRunning) {
        stop();
    }

    m_rate = rate;
    const uint64_t periodNs = 1'000'000'000ULL / static_cast<uint64_t>(rate);

    m_isRunning    = true;
    m_triggerCount = 0;

    {
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        for (auto& entry : m_devices) {
            m_workerThreads.emplace_back(
                &PeriodicRequestScheduler::workerLoop, this,
                entry.device, entry.handler);
        }
    }

    m_schedulerThread = std::thread(&PeriodicRequestScheduler::schedulerLoop, this, periodNs);

    std::cout << "PeriodicRequestScheduler started at "
              << static_cast<int>(rate) << " Hz ("
              << periodNs / 1000 << " us period), "
              << m_workerThreads.size() << " device(s)" << std::endl;
    return true;
}

void PeriodicRequestScheduler::stop()
{
    m_isRunning = false;
    m_triggerCV.notify_all();

    for (auto& t : m_workerThreads) {
        if (t.joinable()) t.join();
    }
    m_workerThreads.clear();

    if (m_schedulerThread.joinable()) {
        m_schedulerThread.join();
    }
}

uint64_t PeriodicRequestScheduler::getAbsoluteTimeNs()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL
         + static_cast<uint64_t>(ts.tv_nsec);
}

void PeriodicRequestScheduler::workerLoop(XsDevice* device, CallbackHandler* handler)
{
    uint64_t lastTrigger = 0;

    while (m_isRunning) {
        std::unique_lock<std::mutex> lock(m_triggerMutex);
        m_triggerCV.wait(lock, [&]() {
            return m_triggerCount.load() > lastTrigger || !m_isRunning;
        });
        lastTrigger = m_triggerCount.load();
        lock.unlock();

        if (!m_isRunning) break;

        // Push the batch timestamp into the handler so the incoming packet
        // gets the trigger time rather than the (later) callback arrival time.
        if (handler) {
            handler->setBatchTimestamp(m_batchTimestamp.load());
        }
        device->requestData();
    }
}

void PeriodicRequestScheduler::schedulerLoop(uint64_t periodNs)
{
    uint64_t nextWakeUpNs = getAbsoluteTimeNs() + periodNs;

    while (m_isRunning) {
        struct timespec absTs;
        absTs.tv_sec  = static_cast<time_t>(nextWakeUpNs / 1'000'000'000ULL);
        absTs.tv_nsec = static_cast<long>(nextWakeUpNs   % 1'000'000'000ULL);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &absTs, nullptr);

        if (!m_isRunning) break;

        // Record ONE timestamp for the entire batch — all devices share it.
        // Use CLOCK_REALTIME so it matches the UTC epoch format in TimestampedPacket.
        struct timespec rts;
        clock_gettime(CLOCK_REALTIME, &rts);
        m_batchTimestamp.store(
            static_cast<uint64_t>(rts.tv_sec) * 1'000'000'000ULL +
            static_cast<uint64_t>(rts.tv_nsec));

        // Wake all worker threads simultaneously
        {
            std::lock_guard<std::mutex> lock(m_triggerMutex);
            ++m_triggerCount;
        }
        m_triggerCV.notify_all();

        nextWakeUpNs += periodNs;
        const uint64_t nowNs = getAbsoluteTimeNs();
        if (nextWakeUpNs < nowNs) {
            nextWakeUpNs = nowNs + periodNs;
        }
    }
}
