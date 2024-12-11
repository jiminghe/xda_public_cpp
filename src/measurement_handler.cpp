#include "measurement_handler.h"
#include <xstypes/xstime.h>
#include <iostream>
#include <iomanip>

using namespace std;

void MeasurementHandler::performMeasurement(XsDevice* device, CallbackHandler& callback, int durationMs)
{
    std::cout << "\nMain loop. Recording data for "<< durationMs/1e3 <<" seconds." << std::endl;
    int64_t startTime = XsTime::timeStampNow();
    while (XsTime::timeStampNow() - startTime <= durationMs)
    {
        if (callback.packetAvailable())
        {
            XsDataPacket packet = callback.getNextPacket();
            printPacketData(packet);
        }
        XsTime::msleep(0);
    }
}

void MeasurementHandler::printPacketData(const XsDataPacket& packet)
{
    std::cout << std::setw(5) << std::fixed << std::setprecision(2);

    if (packet.containsSampleTimeFine())
        cout << "\r" << "SampleTimeFine:" << packet.sampleTimeFine();

    if (packet.containsOrientation())
    {
        XsQuaternion quaternion = packet.orientationQuaternion();
        // cout << " |q0:" << quaternion.w()
        //     << ", q1:" << quaternion.x()
        //     << ", q2:" << quaternion.y()
        //     << ", q3:" << quaternion.z();

        XsEuler euler = packet.orientationEuler();
        cout << " |Roll:" << euler.roll()
            << ", Pitch:" << euler.pitch()
            << ", Yaw:" << euler.yaw();
    }

    if (packet.containsCalibratedData())
    {
        XsVector acc = packet.calibratedAcceleration();
        cout << " |Acc X:" << acc[0]
            << ", Acc Y:" << acc[1]
            << ", Acc Z:" << acc[2];

        XsVector gyr = packet.calibratedGyroscopeData();
        cout << " |Gyr X:" << gyr[0]
            << ", Gyr Y:" << gyr[1]
            << ", Gyr Z:" << gyr[2];

        XsVector mag = packet.calibratedMagneticField();
        // cout << " |Mag X:" << mag[0]
        //     << ", Mag Y:" << mag[1]
        //     << ", Mag Z:" << mag[2];
    }

    if (packet.containsLatitudeLongitude())
    {
        XsVector latLon = packet.latitudeLongitude();
        cout << " |Lat:" << latLon[0]
            << ", Lon:" << latLon[1];
    }

    if (packet.containsAltitude())
        cout << " |Alt:" << packet.altitude();

    if (packet.containsVelocity())
    {
        XsVector vel = packet.velocity(XDI_CoordSysEnu);
        cout << " |E:" << vel[0]
            << ", N:" << vel[1]
            << ", U:" << vel[2];
    }

    std::cout << std::flush;
}