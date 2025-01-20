#include "imu_data_processor.h"
#include <xstypes/xstime.h>

ImuDataProcessor::ImuDataProcessor(size_t windowSize)
    : m_maxWindowSize(windowSize)
{
}

void ImuDataProcessor::addData(const XsDataPacket& packet)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    TimeStampedData data;
    data.packet = packet;
    data.timestamp = XsTime::timeStampNow();
    
    m_dataWindow.push_back(data);
    
    while (m_dataWindow.size() > m_maxWindowSize) {
        m_dataWindow.pop_front();
    }
}

XsVector ImuDataProcessor::getAverageAcceleration(double timeWindowSeconds)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    removeOldData(timeWindowSeconds);

    XsVector sum(3);
    sum.zero();
    int count = 0;

    for (const auto& data : m_dataWindow) {
        if (data.packet.containsCalibratedData()) {
            XsVector acc = data.packet.calibratedAcceleration();
            sum[0] += acc[0];
            sum[1] += acc[1];
            sum[2] += acc[2];
            count++;
        }
    }

    if (count > 0) {
        double scale = 1.0 / count;
        sum[0] *= scale;
        sum[1] *= scale;
        sum[2] *= scale;
    }

    return sum;
}

XsVector ImuDataProcessor::getAverageGyroscope(double timeWindowSeconds)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    removeOldData(timeWindowSeconds);

    XsVector sum(3);
    sum.zero();
    int count = 0;

    for (const auto& data : m_dataWindow) {
        if (data.packet.containsCalibratedData()) {
            XsVector gyr = data.packet.calibratedGyroscopeData();
            sum[0] += gyr[0];
            sum[1] += gyr[1];
            sum[2] += gyr[2];
            count++;
        }
    }

    if (count > 0) {
        double scale = 1.0 / count;
        sum[0] *= scale;
        sum[1] *= scale;
        sum[2] *= scale;
    }

    return sum;
}


void ImuDataProcessor::removeOldData(double timeWindowSeconds)
{
    int64_t currentTime = XsTime::timeStampNow();
    int64_t threshold = currentTime - static_cast<int64_t>(timeWindowSeconds * 1000000); // Convert to microseconds

    while (!m_dataWindow.empty() && m_dataWindow.front().timestamp < threshold) {
        m_dataWindow.pop_front();
    }
}