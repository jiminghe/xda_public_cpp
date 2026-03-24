#include <xscommon/journaller.h>
#include "device_scanner.h"
#include "device_configurator.h"
#include "data_logger.h"
#include "imu_sensor.h"
#include "imu_data_processor.h"
#include "periodic_request_scheduler.h"
#include "timestamped_packet.h"
#include <iostream>
#include <limits>

using namespace std;

// Define the global journal variable
Journaller* gJournal = 0;
static volatile bool keep_running = true;


void printPacketData(const TimestampedPacket& tp)
{
    const XsDataPacket& packet = tp.packet;
    std::cout << std::setw(5) << std::fixed << std::setprecision(2);

    cout << "\r" << "DeviceID:" << tp.deviceId << " |UtcTime:" << tp.utcTimeString();

    if (packet.containsSampleTimeFine())
        cout << " |SampleTimeFine:" << packet.sampleTimeFine();

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

int main() {

    // sensor1 creates and owns the XsControl.
    // sensor2 shares sensor1's XsControl and connects to the second MTi device found.
    ImuSensor sensor1;
    ImuSensor sensor2(sensor1.getControl(), 1);

    sensor1.setSendLatestEnabled(true);
    sensor2.setSendLatestEnabled(true);

    sensor1.setDisconnectionCallback([]() { keep_running = false; });
    sensor2.setDisconnectionCallback([]() { keep_running = false; });

    if (!sensor1.initialize() || !sensor1.startLogging()) {
        std::cout << "Failed to initialize sensor1" << std::endl;
        return -1;
    }
    if (!sensor2.initialize() || !sensor2.startLogging()) {
        std::cout << "Failed to initialize sensor2" << std::endl;
        return -1;
    }

    ImuDataProcessor processor1;
    ImuDataProcessor processor2;
    sensor1.startMeasurement();
    sensor2.startMeasurement();

    // Single scheduler fires requestData() to both sensors simultaneously at 1 Hz
    std::unique_ptr<PeriodicRequestScheduler> scheduler;
    if (sensor1.isSendLatestEnabled()) {
        scheduler = std::make_unique<PeriodicRequestScheduler>();
        sensor1.registerWithScheduler(*scheduler);
        sensor2.registerWithScheduler(*scheduler);
        scheduler->start(RequestRate::Hz1);
    }

    // Main loop
    while (keep_running) {
        if (!sensor1.isConnected() || !sensor2.isConnected()) {
            std::cout << "A device disconnected!" << std::endl;
            break;
        }

        try {
            if (sensor1.hasNewData()) {
                TimestampedPacket tp = sensor1.getLatestData();
                processor1.addData(tp);
                printPacketData(tp);
                std::cout << std::endl;
            }
            if (sensor2.hasNewData()) {
                TimestampedPacket tp = sensor2.getLatestData();
                processor2.addData(tp);
                printPacketData(tp);
                std::cout << std::endl;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error during measurement: " << e.what() << std::endl;
            break;
        }

        XsTime::msleep(1);
    }

    if (scheduler) scheduler->stop();
    sensor1.stopMeasurement();
    sensor2.stopMeasurement();
    sensor1.stopLogging();
    sensor2.stopLogging();

    std::cout << "\nProgram terminated" << std::endl;
    return 0;
}