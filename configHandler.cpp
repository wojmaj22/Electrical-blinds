#include "ConfigHandler.h"
#include <ArduinoJson.h> // JSON library
#include <LittleFS.h>

ConfigHandler::ConfigHandler(void) { }

void ConfigHandler::readWebConfig(webConfig& config)
{

    File file = LittleFS.open("/webConfig.json", "r");
    if (!file) {
        Serial.println(F("Failed to open file for reading"));
        return;
    }
    StaticJsonDocument<480> doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    config.mqttPort = doc["mqttPort"];
    strlcpy(config.ssid, doc["ssid"], sizeof(config.ssid));
    strlcpy(config.password, doc["password"], sizeof(config.password));
    strlcpy(config.mqttUsername, doc["mqttUsername"], sizeof(config.mqttUsername));
    strlcpy(config.mqttPassword, doc["mqttPassword"], sizeof(config.mqttPassword));
    strlcpy(config.mqttServer, doc["mqttServer"], sizeof(config.mqttServer));
    strlcpy(config.otaPassword, doc["otaPassword"], sizeof(config.otaPassword));
    file.close();
}

void ConfigHandler::saveWebConfig(webConfig& config)
{

    File file = LittleFS.open("/webConfig.json", "w");
    if (!file) {
        Serial.println(F("Failed to open file for reading"));
        exit(-1);
    }
    StaticJsonDocument<480> doc;
    doc["ssid"] = config.ssid;
    doc["password"] = config.password;
    doc["mqttUsername"] = config.mqttUsername;
    doc["mqttPassword"] = config.mqttPassword;
    doc["mqttServer"] = config.mqttServer;
    doc["mqttPort"] = config.mqttPort;
    doc["otaPassword"] = config.otaPassword;

    if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write to file"));
    }

    file.close();
}

void ConfigHandler::readBlindsConfig(blindsConfig& config)
{

    File file = LittleFS.open("/blindsConfig.json", "r");
    StaticJsonDocument<128> doc;

    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    config.currentPosition = doc["currentPosition"];
    config.maxSteps = doc["maxSteps"];

    file.close();
}

void ConfigHandler::saveBlindsConfig(blindsConfig& config)
{

    File file = LittleFS.open("/blindsConfig.json", "w");
    StaticJsonDocument<128> doc;

    doc["currentPosition"] = config.currentPosition;
    doc["maxSteps"] = config.maxSteps;

    if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write to file"));
    }

    file.close();
}
