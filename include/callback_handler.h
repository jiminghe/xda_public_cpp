#ifndef CALLBACK_HANDLER_H
#define CALLBACK_HANDLER_H

#include <xscontroller/xscallback.h>
#include <xstypes/xsdatapacket.h>
#include <xscommon/xsens_mutex.h>
#include <list>
#include <functional>

class CallbackHandler : public XsCallback {
public:
    // Callback type for error handling
    using ErrorCallback = std::function<void(XsDevice*, XsResultValue)>;

    CallbackHandler(size_t maxBufferSize = 5);
    virtual ~CallbackHandler() throw();

    bool packetAvailable() const;
    XsDataPacket getNextPacket();

    // Set callback for error handling
    void setErrorCallback(ErrorCallback callback) {
        m_errorCallback = callback;
    }

protected:
    void onLiveDataAvailable(XsDevice*, const XsDataPacket* packet) override;
    void onError(XsDevice* device, XsResultValue error) override;

private:
    mutable xsens::Mutex m_mutex;
    size_t m_maxNumberOfPacketsInBuffer;
    size_t m_numberOfPacketsInBuffer;
    std::list<XsDataPacket> m_packetBuffer;
    ErrorCallback m_errorCallback;
};

#endif