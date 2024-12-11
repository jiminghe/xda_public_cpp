#ifndef CALLBACK_HANDLER_H
#define CALLBACK_HANDLER_H

#include <xscontroller/xscallback.h>
#include <xstypes/xsdatapacket.h>
#include <xscommon/xsens_mutex.h>
#include <list>

class CallbackHandler : public XsCallback {
public:
    CallbackHandler(size_t maxBufferSize = 5);
    virtual ~CallbackHandler() throw();

    bool packetAvailable() const;
    XsDataPacket getNextPacket();

protected:
    void onLiveDataAvailable(XsDevice*, const XsDataPacket* packet) override;

private:
    mutable xsens::Mutex m_mutex;
    size_t m_maxNumberOfPacketsInBuffer;
    size_t m_numberOfPacketsInBuffer;
    std::list<XsDataPacket> m_packetBuffer;
};

#endif