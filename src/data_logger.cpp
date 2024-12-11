#include "data_logger.h"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <iostream>

bool DataLogger::createLogFile(XsDevice* device)
{
    std::string device_product_code = device->shortProductCode().toStdString();
    std::string device_id = device->deviceId().toString().toStdString();
    std::string device_code_id = device_product_code + "_" + device_id;

    std::string logFileName = generateLogFileName(device_code_id);
    std::cout << "Creating a log file..." << std::endl;
    if (device->createLogFile(logFileName) != XRV_OK) {
        return false;
    }
    else {
        std::cout << "Created a log file: " << logFileName.c_str() << std::endl;
    }
    std::cout << "Starting recording..." << std::endl;
    return device->startRecording();
}

bool DataLogger::stopAndCloseLog(XsDevice* device)
{
    if (!device->stopRecording()) {
        return false;
    }
    return device->closeLogFile();
}

std::string DataLogger::generateLogFileName(const std::string& productCode)
{
    std::string cleanProductCode = productCode;

    std::replace(cleanProductCode.begin(), cleanProductCode.end(), ' ', '_');

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << cleanProductCode << "_"
        << std::put_time(&tm, "%Y%m%d_%H%M%S")
        << ".mtb";
    return oss.str();
}