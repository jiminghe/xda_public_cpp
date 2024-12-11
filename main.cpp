#include <xscommon/journaller.h>
#include "device_scanner.h"
#include "device_configurator.h"
#include "data_logger.h"
#include "measurement_handler.h"
#include <iostream>
#include <limits>

// Define the global journal variable
Journaller* gJournal = 0;

int getMeasurementDuration() {
    int minutes;
    const int DEFAULT_DURATION_MS = 100000; // 100 seconds default

    std::cout << "Enter measurement duration in minutes (or press Enter for default 100 seconds): ";
    
    // Get the entire line
    std::string input;
    std::getline(std::cin, input);
    
    // If input is empty, return default duration
    if (input.empty()) {
        std::cout << "Using default duration of 100 seconds." << std::endl;
        return DEFAULT_DURATION_MS;
    }

    try {
        minutes = std::stoi(input);
        
        // Check for negative or zero values
        if (minutes <= 0) {
            std::cout << "Invalid duration. Using default duration of 100 seconds." << std::endl;
            return DEFAULT_DURATION_MS;
        }
        
        // Convert minutes to milliseconds
        int64_t durationMs = static_cast<int64_t>(minutes) * 60 * 1000;
        
        // Check for overflow
        if (durationMs > std::numeric_limits<int>::max()) {
            std::cout << "Duration too large. Using default duration of 100 seconds." << std::endl;
            return DEFAULT_DURATION_MS;
        }
        
        std::cout << "Setting measurement duration to " << minutes << " minutes." << std::endl;
        return static_cast<int>(durationMs);
    }
    catch (const std::invalid_argument& e) {
        std::cout << "Invalid input. Using default duration of 100 seconds." << std::endl;
        return DEFAULT_DURATION_MS;
    }
    catch (const std::out_of_range& e) {
        std::cout << "Number too large. Using default duration of 100 seconds." << std::endl;
        return DEFAULT_DURATION_MS;
    }
}

int main()
{
    int measurementDuration = getMeasurementDuration();

    DeviceScanner scanner;
    if (!scanner.scanAndConnect()) {
        std::cout << "Failed to connect to device." << std::endl;
        return -1;
    }

    XsDevice* device = scanner.getDevice();
    std::string device_product_code = device->productCode().toStdString();
    std::string device_id = device->deviceId().toString().toStdString();

    std::cout << "Device: " << device_product_code << ", with ID: " << device_id << " opened." << std::endl;

    CallbackHandler callback;
    device->addCallbackHandler(&callback);

    DeviceConfigurator configurator;  // Create configurator instance
    if (!configurator.configureDevice(device)) {
        std::cout << "Failed to configure device." << std::endl;
        return -1;
    }

    if (!DataLogger::createLogFile(device)) {
        std::cout << "Failed to create log file." << std::endl;
        return -1;
    }

    MeasurementHandler::performMeasurement(device, callback, measurementDuration);

    configurator.stopGyroBiasEstimation();  // Stop the estimation before closing

    if (!DataLogger::stopAndCloseLog(device)) {
        std::cout << "Failed to close log file." << std::endl;
        return -1;
    }

    std::cout << "\nMeasurement completed successfully." << std::endl;
    return 0;
}