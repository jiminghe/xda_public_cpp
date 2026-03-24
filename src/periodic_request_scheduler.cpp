#include "periodic_request_scheduler.h"
#include <algorithm>
#include <iostream>
#include <time.h>

PeriodicRequestScheduler::PeriodicRequestScheduler() {}

PeriodicRequestScheduler::~PeriodicRequestScheduler()
{
    stop();
}

void PeriodicRequestScheduler::registerDevice(XsDevice* device)
{
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    m_devices.push_back(device);
}

void PeriodicRequestScheduler::unregisterDevice(XsDevice* device)
{
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    m_devices.erase(std::remove(m_devices.begin(), m_devices.end(), device), m_devices.end());
}

bool PeriodicRequestScheduler::start(RequestRate rate)
{
    if (m_isRunning) {
        stop();
    }

    m_rate = rate;
    const uint64_t periodNs = 1'000'000'000ULL / static_cast<uint64_t>(rate);

    m_isRunning = true;
    m_schedulerThread = std::thread(&PeriodicRequestScheduler::schedulerLoop, this, periodNs);

    std::cout << "PeriodicRequestScheduler started at "
              << static_cast<int>(rate) << " Hz ("
              << periodNs / 1000 << " us period)" << std::endl;
    return true;
}

void PeriodicRequestScheduler::stop()
{
    m_isRunning = false;
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

void PeriodicRequestScheduler::schedulerLoop(uint64_t periodNs)
{
    // Establish the first absolute deadline one period from now
    uint64_t nextWakeUpNs = getAbsoluteTimeNs() + periodNs;

    while (m_isRunning) {
        // Sleep until the next absolute deadline using CLOCK_MONOTONIC
        struct timespec absTs;
        absTs.tv_sec  = static_cast<time_t>(nextWakeUpNs / 1'000'000'000ULL);
        absTs.tv_nsec = static_cast<long>(nextWakeUpNs   % 1'000'000'000ULL);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &absTs, nullptr);

        if (!m_isRunning) {
            break;
        }

        // Fire requestData() to all registered devices simultaneously
        {
            std::lock_guard<std::mutex> lock(m_devicesMutex);
            for (auto* device : m_devices) {
                device->requestData();
            }
        }

        // Advance to the next absolute deadline
        nextWakeUpNs += periodNs;

        // Guard against overrun: if we are more than one period behind,
        // reset the deadline to avoid a burst of back-to-back requests
        const uint64_t nowNs = getAbsoluteTimeNs();
        if (nextWakeUpNs < nowNs) {
            nextWakeUpNs = nowNs + periodNs;
        }
    }
}
