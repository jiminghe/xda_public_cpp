#ifndef IMU_DATA_PROCESSOR_H
#define IMU_DATA_PROCESSOR_H

#include <xstypes/xsdatapacket.h>
#include <deque>
#include <mutex>

//Example class for the user to process the sensor data.
class ImuDataProcessor {
public:
    ImuDataProcessor(size_t windowSize = 1000); // Window size in samples
    
    void addData(const XsDataPacket& packet);
    
    // Analysis functions
    XsVector getAverageAcceleration(double timeWindowSeconds = 10.0);
    XsVector getAverageGyroscope(double timeWindowSeconds = 10.0);


private:
    struct TimeStampedData {
        XsDataPacket packet;
        int64_t timestamp;
    };

    std::deque<TimeStampedData> m_dataWindow;
    size_t m_maxWindowSize;
    mutable std::mutex m_mutex;

    void removeOldData(double timeWindowSeconds);
};

#endif