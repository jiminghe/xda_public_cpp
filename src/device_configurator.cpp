#include "device_configurator.h"
#include <iostream>
#include <cstring>

// Cross-platform includes for high-resolution time
#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/time.h>
    #include <time.h>
#endif

DeviceConfigurator::DeviceConfigurator() : m_estimator(nullptr) {}

DeviceConfigurator::~DeviceConfigurator() {
    if (m_estimator) {
        m_estimator->stopPeriodicEstimation();
    }
}

void DeviceConfigurator::stopGyroBiasEstimation() {
    if (m_estimator) {
        m_estimator->stopPeriodicEstimation();
    }
}

XsTimeInfo DeviceConfigurator::getCurrentUtcTime() {
    XsTimeInfo timeInfo;
    memset(&timeInfo, 0, sizeof(XsTimeInfo));

#ifdef _WIN32
    // Windows implementation using GetSystemTimeAsFileTime for microsecond precision
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    
    // Convert FILETIME to SYSTEMTIME
    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);
    
    // FILETIME is in 100-nanosecond intervals since January 1, 1601
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    
    // Extract microseconds from the 100-nanosecond precision
    uint64_t totalHundredNs = uli.QuadPart;
    uint32_t microSeconds = (totalHundredNs % 10000000ULL) / 10;  // Convert to microseconds
    uint32_t nanoSeconds = microSeconds * 1000;  // Convert to nanoseconds
    
    timeInfo.m_year = st.wYear;
    timeInfo.m_month = static_cast<uint8_t>(st.wMonth);
    timeInfo.m_day = static_cast<uint8_t>(st.wDay);
    timeInfo.m_hour = static_cast<uint8_t>(st.wHour);
    timeInfo.m_minute = static_cast<uint8_t>(st.wMinute);
    timeInfo.m_second = static_cast<uint8_t>(st.wSecond);
    timeInfo.m_nano = nanoSeconds;
    
#else
    // Linux/Unix implementation using clock_gettime for nanosecond precision
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        // Convert to UTC time structure
        struct tm* utc_tm = gmtime(&ts.tv_sec);
        if (utc_tm != nullptr) {
            timeInfo.m_year = static_cast<uint16_t>(utc_tm->tm_year + 1900);
            timeInfo.m_month = static_cast<uint8_t>(utc_tm->tm_mon + 1);
            timeInfo.m_day = static_cast<uint8_t>(utc_tm->tm_mday);
            timeInfo.m_hour = static_cast<uint8_t>(utc_tm->tm_hour);
            timeInfo.m_minute = static_cast<uint8_t>(utc_tm->tm_min);
            timeInfo.m_second = static_cast<uint8_t>(utc_tm->tm_sec);
            timeInfo.m_nano = static_cast<uint32_t>(ts.tv_nsec);
        }
    }
#endif

    // Set validity flags (UTC Date valid | UTC Time valid | Time fully resolved)
    timeInfo.m_valid = 0x01;  // Bits 0, 1, and 2 set
    timeInfo.m_utcOffset = 0;  // Already UTC time, so offset is 0

    return timeInfo;
}

bool DeviceConfigurator::setUtcTime(XsDevice* device) {
    if (!device) {
        std::cout << "Invalid device pointer for UTC time setting." << std::endl;
        return false;
    }

    XsTimeInfo currentTime = getCurrentUtcTime();
    
    std::cout << "Setting UTC time on device: " 
              << static_cast<int>(currentTime.m_year) << "-"
              << static_cast<int>(currentTime.m_month) << "-"
              << static_cast<int>(currentTime.m_day) << " "
              << static_cast<int>(currentTime.m_hour) << ":"
              << static_cast<int>(currentTime.m_minute) << ":"
              << static_cast<int>(currentTime.m_second) << "."
              << (currentTime.m_nano / 1000) << " UTC" << std::endl;

    if (!device->setUtcTime(currentTime)) {
        std::cout << "Failed to set UTC time on device." << std::endl;
        return false;
    }

    std::cout << "UTC time successfully set on device." << std::endl;
    return true;
}

bool DeviceConfigurator::configureDevice(XsDevice* device, bool setOutputConfig)
{
    std::cout << "Putting device into configuration mode..." << std::endl;
    if (!device->gotoConfig()) {
        std::cout << "Could not put device into configuration mode." << std::endl;
        return false;
    }

    if (setOutputConfig) {
        std::cout << "Configuring the device output..." << std::endl;
        XsOutputConfigurationArray configArray = createConfigArray(device);

        if (!device->setOutputConfiguration(configArray)) {
            std::cout << "Could not configure MTi device." << std::endl;
            return false;
        }

        XsTime::msleep(100);
    } else {
        std::cout << "Skipping output configuration (setOutputConfig=0)" << std::endl;
    }

    // Set UTC time after configuration but before creating log file
    if (!setUtcTime(device)) {
        std::cout << "Warning: Could not set UTC time on device." << std::endl;
    }

    XsTime::msleep(100);

    //read config before create log file, otherwise the log file will use the old settings.
    device->readEmtsAndDeviceConfiguration();
    XsTime::msleep(200);

    // Create log file BEFORE going to measurement mode
    if (!DataLogger::createLogFile(device)) {
        return false;
    }

    XsTime::msleep(100);

    std::cout << "Putting device into measurement mode..." << std::endl;
    if (!device->gotoMeasurement()) {
        std::cout << "Could not put device into measurement mode." << std::endl;
        return false;
    }

    XsTime::msleep(100);

    // Start recording AFTER going to measurement mode
    if (!DataLogger::startRecording(device)) {
        return false;
    }

    XsTime::msleep(100);

    // Create the gyro bias estimator but DON'T run initial estimation yet
    m_estimator = std::make_unique<GyroBiasEstimator>(device);

    return true;
}

XsOutputConfigurationArray DeviceConfigurator::createConfigArray(XsDevice* device)
{
    XsOutputConfigurationArray configArray;
    configArray.push_back(XsOutputConfiguration(XDI_PacketCounter, 0));
    configArray.push_back(XsOutputConfiguration(XDI_SampleTimeFine, 0));
    configArray.push_back(XsOutputConfiguration(XDI_UtcTime, 0));


    if (device->deviceId().isImu()) {
        configArray.push_back(XsOutputConfiguration(XDI_Acceleration, 100));
        configArray.push_back(XsOutputConfiguration(XDI_RateOfTurn, 100));
        configArray.push_back(XsOutputConfiguration(XDI_MagneticField, 100));
    }
    else if (device->deviceId().isVru() || device->deviceId().isAhrs()) {
        configArray.push_back(XsOutputConfiguration(XDI_Quaternion, 100));
        configArray.push_back(XsOutputConfiguration(XDI_FreeAcceleration, 100));
        configArray.push_back(XsOutputConfiguration(XDI_Acceleration, 100));
        configArray.push_back(XsOutputConfiguration(XDI_RateOfTurn, 100));
        configArray.push_back(XsOutputConfiguration(XDI_MagneticField, 100));
        // Baro output: MTi-600 series, MTi-100 series, MTi-7/8, Sirius series.
        // configArray.push_back(XsOutputConfiguration(XDI_BaroPressure, 100));
        configArray.push_back(XsOutputConfiguration(XDI_Temperature, 100));
        // configArray.push_back(XsOutputConfiguration(XDI_AccelerationHR, 0xFFFF));
        // configArray.push_back(XsOutputConfiguration(XDI_RateOfTurnHR, 0xFFFF));
    }
    else if (device->deviceId().isGnss()) {
        configArray.push_back(XsOutputConfiguration(XDI_Quaternion, 100));
        configArray.push_back(XsOutputConfiguration(XDI_FreeAcceleration, 100));
        configArray.push_back(XsOutputConfiguration(XDI_Acceleration, 100));
        configArray.push_back(XsOutputConfiguration(XDI_RateOfTurn, 100));
        configArray.push_back(XsOutputConfiguration(XDI_MagneticField, 100));
        configArray.push_back(XsOutputConfiguration(XDI_LatLon | XDI_SubFormatFp1632, 100));
        configArray.push_back(XsOutputConfiguration(XDI_AltitudeEllipsoid | XDI_SubFormatFp1632, 100));
        configArray.push_back(XsOutputConfiguration(XDI_VelocityXYZ | XDI_SubFormatFp1632, 100));
    }

    configArray.push_back(XsOutputConfiguration(XDI_StatusWord, 0));

    return configArray;
}

bool DeviceConfigurator::startGyroBiasEstimation() {
    if (!m_estimator) {
        std::cout << "Gyro bias estimator not initialized." << std::endl;
        return false;
    }

    // Perform initial estimation
    std::cout << "Performing initial gyro bias estimation for 6 seconds..." << std::endl;
    if (!m_estimator->performSingleEstimation(6)) {
        std::cout << "Initial gyro bias estimation failed." << std::endl;
        return false;
    }

    // Start periodic estimation
    m_estimator->startPeriodicEstimation(15, 3);
    return true;
}