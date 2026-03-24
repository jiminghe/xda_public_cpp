#ifndef CALLBACK_HANDLER_H
#define CALLBACK_HANDLER_H

#include <xscontroller/xscallback.h>
#include <xscommon/xsens_mutex.h>
#include <list>
#include <functional>
#include <atomic>
#include "timestamped_packet.h"

class CallbackHandler : public XsCallback {
public:
    // Callback type for error handling
    using ErrorCallback = std::function<void(XsDevice*, XsResultValue)>;

    CallbackHandler(size_t maxBufferSize = 5);
    virtual ~CallbackHandler() throw();

    bool packetAvailable() const;
    TimestampedPacket getNextPacket();

    // Set callback for error handling
    void setErrorCallback(ErrorCallback callback) {
        m_errorCallback = callback;
    }

    // Called by PeriodicRequestScheduler just before requestData() so all
    // devices in the same batch share one trigger timestamp.
    // Falls back to clock_gettime() when 0 (continuous streaming mode).
    void setBatchTimestamp(uint64_t ts) { m_batchTimestamp.store(ts); }

protected:
    void onLiveDataAvailable(XsDevice*, const XsDataPacket* packet) override;
    void onError(XsDevice* device, XsResultValue error) override;

private:
    mutable xsens::Mutex m_mutex;
    size_t m_maxNumberOfPacketsInBuffer;
    size_t m_numberOfPacketsInBuffer;
    std::list<TimestampedPacket> m_packetBuffer;
    ErrorCallback m_errorCallback;
    std::atomic<uint64_t> m_batchTimestamp{0};
};

#endif