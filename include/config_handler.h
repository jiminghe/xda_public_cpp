#ifndef CONFIG_HANDLER_H
#define CONFIG_HANDLER_H

#include <string>

class ConfigHandler {
public:
    ConfigHandler();
    
    // Read configuration from file
    bool readConfig();
    
    // Write configuration to file
    bool writeConfig();
    
    // Create default configuration file
    bool createDefaultConfig();
    
    // Getters
    std::string getPortName() const { return m_portName; }
    int getBaudrate() const { return m_baudrate; }
    bool getSetOutputConfig() const { return m_setOutputConfig; }
    
    // Setters
    void setPortName(const std::string& portName) { m_portName = portName; }
    void setBaudrate(int baudrate) { m_baudrate = baudrate; }
    void setSetOutputConfig(bool enable) { m_setOutputConfig = enable; }

private:
    std::string m_portName;
    int m_baudrate;
    bool m_setOutputConfig;
    
    static const std::string CONFIG_FILENAME;
    
    // Helper functions
    std::string trim(const std::string& str);
    bool parseLine(const std::string& line, std::string& key, std::string& value);
    bool configFileExists() const;
};

#endif