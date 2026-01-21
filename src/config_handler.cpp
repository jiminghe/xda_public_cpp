#include "config_handler.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

const std::string ConfigHandler::CONFIG_FILENAME = "port.ini";

ConfigHandler::ConfigHandler()
    : m_portName("COM3")
    , m_baudrate(115200)
    , m_setOutputConfig(true)
{
    // Check if config file exists, create if it doesn't
    if (!configFileExists()) {
        std::cout << "Configuration file '" << CONFIG_FILENAME << "' not found." << std::endl;
        std::cout << "Creating default configuration file..." << std::endl;
        createDefaultConfig();
    }
    
    // Read the configuration
    if (readConfig()) {
        std::cout << "Loaded configuration from " << CONFIG_FILENAME << std::endl;
        std::cout << "  Port: " << m_portName << std::endl;
        std::cout << "  Baudrate: " << m_baudrate << std::endl;
        std::cout << "  SetOutputConfig: " << (m_setOutputConfig ? "enabled" : "disabled") << std::endl;
    } else {
        std::cout << "Warning: Failed to read configuration, using defaults." << std::endl;
    }
}

bool ConfigHandler::configFileExists() const
{
    std::ifstream file(CONFIG_FILENAME);
    return file.good();
}

std::string ConfigHandler::trim(const std::string& str)
{
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool ConfigHandler::parseLine(const std::string& line, std::string& key, std::string& value)
{
    // Skip empty lines and comments
    std::string trimmedLine = trim(line);
    if (trimmedLine.empty() || trimmedLine[0] == '#' || trimmedLine[0] == ';') {
        return false;
    }
    
    // Parse key=value pairs
    size_t equalPos = trimmedLine.find('=');
    if (equalPos == std::string::npos) {
        return false;
    }
    
    key = trim(trimmedLine.substr(0, equalPos));
    value = trim(trimmedLine.substr(equalPos + 1));
    
    return !key.empty();
}

bool ConfigHandler::readConfig()
{
    std::ifstream configFile(CONFIG_FILENAME);
    if (!configFile.is_open()) {
        return false;
    }
    
    std::string line;
    bool foundPort = false;
    bool foundBaudrate = false;
    bool foundSetOutputConfig = false;
    
    while (std::getline(configFile, line)) {
        std::string key, value;
        if (!parseLine(line, key, value)) {
            continue;
        }
        
        // Convert key to lowercase for case-insensitive comparison
        std::string lowerKey = key;
        for (char& c : lowerKey) {
            c = std::tolower(static_cast<unsigned char>(c));
        }
        
        if (lowerKey == "port") {
            m_portName = value;
            foundPort = true;
        }
        else if (lowerKey == "baudrate") {
            try {
                m_baudrate = std::stoi(value);
                foundBaudrate = true;
            }
            catch (const std::exception& e) {
                std::cerr << "Invalid baudrate value in " << CONFIG_FILENAME << ": " << value 
                          << " (" << e.what() << ")" << std::endl;
            }
        }
        else if (lowerKey == "setoutputconfig") {
            // Accept various true/false representations
            std::string lowerValue = value;
            for (char& c : lowerValue) {
                c = std::tolower(static_cast<unsigned char>(c));
            }
            
            if (lowerValue == "1" || lowerValue == "true" || lowerValue == "yes" || lowerValue == "on") {
                m_setOutputConfig = true;
                foundSetOutputConfig = true;
            }
            else if (lowerValue == "0" || lowerValue == "false" || lowerValue == "no" || lowerValue == "off") {
                m_setOutputConfig = false;
                foundSetOutputConfig = true;
            }
            else {
                std::cerr << "Invalid setOutputConfig value in " << CONFIG_FILENAME << ": " << value << std::endl;
            }
        }
    }
    
    configFile.close();
    
    // If setOutputConfig wasn't found in config file, use default (true)
    if (!foundSetOutputConfig) {
        m_setOutputConfig = true;
    }
    
    return foundPort && foundBaudrate;
}

bool ConfigHandler::writeConfig()
{
    std::ofstream configFile(CONFIG_FILENAME);
    if (!configFile.is_open()) {
        std::cerr << "Failed to open " << CONFIG_FILENAME << " for writing" << std::endl;
        return false;
    }
    
    configFile << "# MTi Device Port Configuration\n";
    configFile << "# Edit this file to match your device's port and baudrate\n";
    configFile << "# Common baudrates: 115200, 230400, 460800, 921600\n";
    configFile << "#\n";
    configFile << "# setOutputConfig: Set to 1 to configure device output, 0 to skip configuration\n";
    configFile << "#                  Default is 1 (true)\n";
    configFile << "#                  Set to 0 if you want to use device's existing configuration\n";
    configFile << "\n";
    configFile << "port=" << m_portName << "\n";
    configFile << "baudrate=" << m_baudrate << "\n";
    configFile << "setOutputConfig=" << (m_setOutputConfig ? "1" : "0") << "\n";
    
    configFile.close();
    std::cout << "Configuration file '" << CONFIG_FILENAME << "' created successfully." << std::endl;
    return true;
}

bool ConfigHandler::createDefaultConfig()
{
    return writeConfig();
}