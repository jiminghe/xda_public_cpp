#include "imu_data_processor.h"

ImuDataProcessor::ImuDataProcessor(size_t windowSize)
    : m_maxWindowSize(windowSize)
{
}

void ImuDataProcessor::addData(const TimestampedPacket& tp)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_dataWindow.push_back(tp);

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

    for (const auto& tp : m_dataWindow) {
        if (tp.packet.containsCalibratedData()) {
            XsVector acc = tp.packet.calibratedAcceleration();
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

    for (const auto& tp : m_dataWindow) {
        if (tp.packet.containsCalibratedData()) {
            XsVector gyr = tp.packet.calibratedGyroscopeData();
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
    // receiveTimeNs is in nanoseconds; convert window to nanoseconds for comparison
    const uint64_t nowNs       = TimestampedPacket::now();
    const uint64_t windowNs    = static_cast<uint64_t>(timeWindowSeconds * 1'000'000'000.0);
    const uint64_t thresholdNs = (nowNs > windowNs) ? (nowNs - windowNs) : 0;

    while (!m_dataWindow.empty() && m_dataWindow.front().receiveTimeNs < thresholdNs) {
        m_dataWindow.pop_front();
    }
}
