#include <xscommon/journaller.h>
#include "device_scanner.h"
#include "device_configurator.h"
#include "data_logger.h"
#include "imu_sensor.h"
#include "imu_data_processor.h"
#include <iostream>
#include <limits>

using namespace std;

// Define the global journal variable
Journaller* gJournal = 0;
static volatile bool keep_running = true;


void printPacketData(const XsDataPacket& packet)
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

int main() {
    
    
    ImuSensor sensor;
    
    // Set up disconnection handling
    sensor.setDisconnectionCallback([]() {
        keep_running = false;
    });

    if (!sensor.initialize() || !sensor.startLogging()) {
        std::cout << "Failed to initialize sensor" << std::endl;
        return -1;
    }

    ImuDataProcessor processor;
    sensor.startMeasurement();

    // Main loop
    while (keep_running) {
        if (!sensor.isConnected()) {
            std::cout << "Device no longer connected!" << std::endl;
            break;
        }

        try {
            if (sensor.hasNewData()) {
                XsDataPacket packet = sensor.getLatestData();
                processor.addData(packet);


                printPacketData(packet);

                //check if manual gyro bias estimation is working
                // if (packet.containsStatus())
                // {
                //     uint32_t status = packet.status();
                //     uint32_t no_rotation_update = (status >> 3) & 0x3;
                //     std::cout << "no_rotation_update_status = " << no_rotation_update << std::endl;
                // }

                static int counter = 0;
                if (++counter % 100 == 0) {
                    XsVector avgAcc = processor.getAverageAcceleration();
                    //std::cout << "Average acceleration (10s): "
                    //          << avgAcc[0] << ", " 
                    //          << avgAcc[1] << ", " 
                    //          << avgAcc[2] << std::endl;
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error during measurement: " << e.what() << std::endl;
            break;
        }

        XsTime::msleep(1);
    }

    sensor.stopMeasurement();
    sensor.stopLogging();

    std::cout << "\nProgram terminated" << std::endl;
    return 0;
}