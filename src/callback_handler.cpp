#include "callback_handler.h"
#include <xscontroller/xsdevice_def.h>
#include <cassert>
#include <iostream>

CallbackHandler::CallbackHandler(size_t maxBufferSize)
    : m_maxNumberOfPacketsInBuffer(maxBufferSize)
    , m_numberOfPacketsInBuffer(0)
{
}

CallbackHandler::~CallbackHandler() throw()
{
}

bool CallbackHandler::packetAvailable() const
{
    xsens::Lock locky(&m_mutex);
    return m_numberOfPacketsInBuffer > 0;
}

TimestampedPacket CallbackHandler::getNextPacket()
{
    assert(packetAvailable());
    xsens::Lock locky(&m_mutex);
    TimestampedPacket oldest(m_packetBuffer.front());
    m_packetBuffer.pop_front();
    --m_numberOfPacketsInBuffer;
    return oldest;
}

void CallbackHandler::onLiveDataAvailable(XsDevice* device, const XsDataPacket* packet)
{
    // Capture UTC wall-clock time as early as possible
    const uint64_t receiveTimeNs = TimestampedPacket::now();

    xsens::Lock locky(&m_mutex);
    assert(packet != 0);
    while (m_numberOfPacketsInBuffer >= m_maxNumberOfPacketsInBuffer)
        (void)getNextPacket();

    TimestampedPacket tp;
    tp.packet        = *packet;
    tp.receiveTimeNs = receiveTimeNs;
    tp.deviceId      = device ? device->deviceId().toString().toStdString() : "unknown";
    m_packetBuffer.push_back(tp);
    ++m_numberOfPacketsInBuffer;
    assert(m_numberOfPacketsInBuffer <= m_maxNumberOfPacketsInBuffer);
}

void CallbackHandler::onError(XsDevice* device, XsResultValue error)
{
    if (m_errorCallback) {
        m_errorCallback(device, error);
    }
    //std::cout << "CallbackHandler::onError, error = " << XsResultValue_toString(error) << std::endl;
}