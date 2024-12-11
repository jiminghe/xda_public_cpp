#ifndef GYRO_BIAS_ESTIMATOR_H
#define GYRO_BIAS_ESTIMATOR_H

#include <xscontroller/xsdevice_def.h>
#include <thread>
#include <atomic>
#include <chrono>

class GyroBiasEstimator {
public:
    GyroBiasEstimator(XsDevice* device);
    ~GyroBiasEstimator();

    // Start periodic estimation with given interval and duration
    void startPeriodicEstimation(uint16_t intervalSeconds, uint16_t durationSeconds);
    // Stop periodic estimation
    void stopPeriodicEstimation();
    // Perform a single bias estimation
    bool performSingleEstimation(uint16_t duration);

private:
    XsDevice* m_device;
    std::thread m_estimationThread;
    std::atomic<bool> m_isRunning;
    
    // Thread function for periodic estimation
    void estimationLoop(uint16_t intervalSeconds, uint16_t durationSeconds);
    // Helper function to send the no-rotation message
    bool manualGyroBiasEstimation(uint16_t duration);
};

#endif