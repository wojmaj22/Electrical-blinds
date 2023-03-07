#ifndef ConfigHandler_h
#define ConfigHandler_h

struct webConfig {
    char ssid[24];
    char password[24];
    char mqttUsername[24];
    char mqttPassword[24];
    char mqttServer[24];
    int mqttPort;
    char otaPassword[24];
};

struct blindsConfig {
    int currentPosition;
    int maxSteps;
};

class ConfigHandler {
public:
    ConfigHandler(int s);
    void readWebConfig(webConfig& config);
    void saveWebConfig(webConfig& config);
    void readBlindsConfig(blindsConfig& config);
    void saveBlindsConfig(blindsConfig& config);
};

#endif