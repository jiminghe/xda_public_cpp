#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <xscontroller/xsdevice_def.h>
#include <string>

class DataLogger {
public:
    static bool createLogFile(XsDevice* device);
    static bool stopAndCloseLog(XsDevice* device);
private:
    static std::string generateLogFileName(const std::string& productCode);
};

#endif