#ifndef IMU_DATA_PROCESSOR_H
#define IMU_DATA_PROCESSOR_H

#include <deque>
#include <mutex>
#include "timestamped_packet.h"

//Example class for the user to process the sensor data.
class ImuDataProcessor {
public:
    ImuDataProcessor(size_t windowSize = 1000); // Window size in samples

    void addData(const TimestampedPacket& tp);

    // Analysis functions
    XsVector getAverageAcceleration(double timeWindowSeconds = 10.0);
    XsVector getAverageGyroscope(double timeWindowSeconds = 10.0);

private:
    std::deque<TimestampedPacket> m_dataWindow;
    size_t m_maxWindowSize;
    mutable std::mutex m_mutex;

    void removeOldData(double timeWindowSeconds);
};

#endif