#include "gyro_bias_estimator.h"
#include <iostream>
#include <thread>

GyroBiasEstimator::GyroBiasEstimator(XsDevice* device)
    : m_device(device)
    , m_isRunning(false)
{
}

GyroBiasEstimator::~GyroBiasEstimator()
{
    stopPeriodicEstimation();
}

void GyroBiasEstimator::startPeriodicEstimation(uint16_t intervalSeconds, uint16_t durationSeconds)
{
    if (m_isRunning) {
        stopPeriodicEstimation();
    }

    m_isRunning = true;
    m_estimationThread = std::thread([this, intervalSeconds, durationSeconds]() {
        // Add initial delay before starting periodic estimation
        // std::cout << "Waiting 15 seconds before starting periodic estimation..." << std::endl;
        for (int i = 0; i < 15 && m_isRunning; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Start the periodic estimation loop
        this->estimationLoop(intervalSeconds, durationSeconds);
    });
}

void GyroBiasEstimator::stopPeriodicEstimation()
{
    m_isRunning = false;
    if (m_estimationThread.joinable()) {
        m_estimationThread.join();
    }
}

bool GyroBiasEstimator::performSingleEstimation(uint16_t duration)
{
    // std::cout << "Performing gyro bias estimation for " << duration << " seconds..." << std::endl;
    return manualGyroBiasEstimation(duration);
}

void GyroBiasEstimator::estimationLoop(uint16_t intervalSeconds, uint16_t durationSeconds)
{
    while (m_isRunning)
    {
        performSingleEstimation(durationSeconds);
        
        // Sleep for the interval duration
        for (int i = 0; i < intervalSeconds && m_isRunning; ++i)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

bool GyroBiasEstimator::manualGyroBiasEstimation(uint16_t duration)
{
    if (duration < 2)
    {
        std::cout << "Duration is less than 2 seconds, setting it to 2 seconds." << std::endl;
        duration = 2;
    }

    XsMessage snd(XMID_SetNoRotation, sizeof(uint16_t));
    XsMessage rcv;
    snd.setDataShort(duration);
    if (!m_device->sendCustomMessage(snd, true, rcv, 1000))
    {
        std::cout << "Failed to perform gyro bias estimation." << std::endl;
        return false;
    }

    // std::cout << "Gyro bias estimation completed successfully." << std::endl;
    return true;
}