#ifndef MEASUREMENT_HANDLER_H
#define MEASUREMENT_HANDLER_H

#include "callback_handler.h"
#include <xscontroller/xsdevice_def.h>

class MeasurementHandler {
public:
    static void performMeasurement(XsDevice* device, CallbackHandler& callback, int durationMs);
private:
    static void printPacketData(const XsDataPacket& packet);
};

#endif