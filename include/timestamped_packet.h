#ifndef TIMESTAMPED_PACKET_H
#define TIMESTAMPED_PACKET_H

#include <xstypes/xsdatapacket.h>
#include <cstdint>
#include <time.h>
#include <string>
#include <sstream>
#include <iomanip>

struct TimestampedPacket {
    XsDataPacket packet;
    uint64_t     receiveTimeNs{0};  // CLOCK_REALTIME nanoseconds at moment of receipt

    // Format as "seconds.microseconds" (e.g. 1774325614.123456)
    std::string utcTimeString() const
    {
        uint64_t sec  = receiveTimeNs / 1'000'000'000ULL;
        uint64_t usec = (receiveTimeNs % 1'000'000'000ULL) / 1000ULL;
        std::ostringstream oss;
        oss << sec << "." << std::setfill('0') << std::setw(6) << usec;
        return oss.str();
    }

    static uint64_t now()
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL
             + static_cast<uint64_t>(ts.tv_nsec);
    }
};

#endif
