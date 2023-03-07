// imports
#include "configHandler.h"
#include <A4988.h> // stepper motor driver library
#include <ArduinoOTA.h> // OTA library
#include <CertStoreBearSSL.h>
#include <ESP8266WiFi.h> // WiFi library
#include <ESPAsyncTCP.h> // library for AsyncWebServer
#include <ESPAsyncWebServer.h> // WiFi server library
#include <LittleFS.h> // File system library
#include <PubSubClient.h> // MQTT library
#include <TZ.h>
#include <time.h>

// defines
// stepper motor
#define MOTOR_STEPS 200
#define STEP D1
#define DIR D2
#define DISABLE_POWER D7
// buttons
#define UPWARD_BUTTON D5
#define DOWNWARD_BUTTON D6

// global variables
// object used to run a stepper motor
A4988 stepper(MOTOR_STEPS, DIR, STEP, DISABLE_POWER);
// WiFi client and certStore
WiFiClientSecure espClient;
BearSSL::CertStore certStore;
// MQTT client
PubSubClient* client;
// MQTT topic which esp uses to receive messages
const char topic[] = "ESP8266/blinds/in";
// configs and config handler
struct blindsConfig blindsConfig;
struct webConfig webConfig;
ConfigHandler ConfigHandler(5);
// variables used in configs handling
bool resetCredentials = 0;
bool restart = false;
bool blindsConfigChanged = false;
bool manualMode = false;
// used to count currentMillis
long lastMillis = 0;
long currentMillis = 0;
const long timeInterval = 60000; // 1 minute

// main function used to change position of blinds to the desired position between 0-100%
void move(int percent)
{
    int onePercentSteps = blindsConfig.maxSteps / 100;
    int expectedPosition = onePercentSteps * percent;
    int stepsNeeded = expectedPosition - blindsConfig.currentPosition;
    if (stepsNeeded != 0) {
        stepper.enable();
        delay(5);
        stepper.move(stepsNeeded);
        blindsConfig.currentPosition = expectedPosition;
        blindsConfigChanged = true;
        delay(5);
        stepper.disable();
    }
}

// used to move stepper motor by number of steps insted of desired percent state
void moveSteps(int stepsToMove)
{
    if (stepsToMove != 0) {
        stepper.enable();
        delay(5);
        stepper.move(stepsToMove);
        blindsConfig.currentPosition += stepsToMove;
        blindsConfigChanged = true;
        delay(5);
        stepper.disable();
    }
}

// moves the blinds with 3 state button
void moveByButtons()
{
    int upState = digitalRead(UPWARD_BUTTON);
    int downState = digitalRead(DOWNWARD_BUTTON);

    if (upState == HIGH && (manualMode || blindsConfig.currentPosition < blindsConfig.maxSteps)) {
        moveSteps(blindsConfig.maxSteps / 100);
    } else if (downState == HIGH && (manualMode || blindsConfig.currentPosition > 0)) {
        moveSteps(-1 * blindsConfig.maxSteps / 100);
    }
}

// process messages incoming from MQTT broker
void callback(char* topic, byte* payload, unsigned int length)
{
    Serial.printf("Message on topic %s: ", topic);
    String msg = "";
    for (int i = 0; i < length; i++) {
        msg += String((char)payload[i]);
    }
    Serial.printf("%s \n", msg);
    handleMessage(msg.c_str());
}

// check the message and do what it says
void handleMessage(String msg)
{
    if (msg == "(reset)") {
        Serial.println("Reseting WiFi and MQTT data");
        startAP();
        return;
    } else if (msg == "(set-zero)") {
        Serial.println("Setting 0 position");
        blindsConfig.currentPosition = 0;
        blindsConfigChanged = true;
    } else if (msg == "(set-max)") {
        Serial.println("Setting max position");
        blindsConfig.maxSteps = blindsConfig.currentPosition;
        blindsConfigChanged = true;
    } else if (msg == "(manual)") {
        if (!manualMode) {
            Serial.println("Entering manual mode");
        } else {
            Serial.println("Leaving manual mode");
        }
        manualMode = !manualMode;
    } else if (msg == "(save-now)") {
        Serial.println("Saving configs.");
        ConfigHandler.saveBlindsConfig(blindsConfig);
        blindsConfigChanged = false;
        ConfigHandler.saveWebConfig(webConfig);
    } else {
        int percent = msg.toInt();
        if (percent >= 0 && percent <= 100) {
            move(percent);
        }
    }
}

// sets WiFi connectivity
bool setupWIFI()
{
    delay(10);
    Serial.println("Connecting to WiFi.");
    WiFi.mode(WIFI_STA);
    WiFi.begin(webConfig.ssid, webConfig.password);
    long currentTime = millis();
    long maxTime = 20000;
    while (WiFi.status() != WL_CONNECTED) {
        yield();
        if (millis() - currentTime > maxTime) {
            Serial.println("Cannot connect to wifi.");
            return true;
        }
    }
    randomSeed(micros());
    Serial.printf("Connected to: %s at IP: ", webConfig.ssid);
    Serial.println(WiFi.localIP());
    return false;
}

// manages MQTT broker connection and reconections
void reconnect()
{
    while (!client->connected()) {
        Serial.println("Attempting MQTT connection.");
        String clientId = "ESP8266Blinds";
        if (client->connect(clientId.c_str(), webConfig.mqttUsername, webConfig.mqttPassword)) {
            Serial.println("Connected");
            client->subscribe(topic);
        } else {
            Serial.print("Failed, reason = ");
            Serial.println(client->state());
            delay(5000);
        }
    }
}

// sets OTA updates and begins service
void setupOTA()
{
    // TODO - check if works as intended
    ArduinoOTA.onStart([]() {
        LittleFS.end();
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else {
            type = "filesystem";
        }
        Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
        Serial.print("\nEnd of update, restarting now.\n");
        delay(2000);
        ESP.restart();
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            Serial.println("End Failed");
        }
    });
    ArduinoOTA.setPassword((const char*)webConfig.otaPassword);
    ArduinoOTA.begin();
}

// start AP used to set up new WiFi and MQTT data
void startAP()
{
    AsyncWebServer server(80);
    Serial.println("Setting Access Point");
    WiFi.softAP("ESP8266 - roleta", "12345678");

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    server.on(
        "/", HTTP_GET, [](AsyncWebServerRequest* request) { request->send(LittleFS, "/index.html", "text/html"); });

    server.on("/style.css", HTTP_GET,
        [](AsyncWebServerRequest* request) { request->send(LittleFS, "/style.css", "test/css"); });

    server.on("/", HTTP_POST, [](AsyncWebServerRequest* request) {
        int params = request->params();
        for (int i = 0; i < params; i++) {
            AsyncWebParameter* p = request->getParam(i);
            if (p->isPost()) {
                if (p->name() == "ssid") {
                    strlcpy(webConfig.ssid, p->value().c_str(), 24);
                    Serial.print("SSID set to: ");
                    Serial.println(webConfig.ssid);
                }
                if (p->name() == "password") {
                    strlcpy(webConfig.password, p->value().c_str(), 24);
                    Serial.print("Password set to: ");
                    Serial.println(webConfig.password);
                }
                if (p->name() == "mqttUsername") {
                    strlcpy(webConfig.mqttUsername, p->value().c_str(), 24);
                    Serial.print("IP Address set to: ");
                    Serial.println(webConfig.mqttUsername);
                }
                if (p->name() == "mqttPassword") {
                    strlcpy(webConfig.mqttPassword, p->value().c_str(), 24);
                    Serial.print("Gateway set to: ");
                    Serial.println(webConfig.mqttPassword);
                }
                if (p->name() == "mqttServer") {
                    strlcpy(webConfig.mqttServer, p->value().c_str(), 24);
                    Serial.print("Gateway set to: ");
                    Serial.println(webConfig.mqttServer);
                }
                if (p->name() == "otaPassword") {
                    strlcpy(webConfig.otaPassword, p->value().c_str(), 24);
                    Serial.print("Gateway set to: ");
                    Serial.println(webConfig.otaPassword);
                }
                if (p->name() == "mqttPort") {
                    webConfig.mqttPort = p->value().toInt();
                    Serial.print("Gateway set to: ");
                    Serial.println(webConfig.mqttPort);
                }
                Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
            }
        }
        restart = true;
        Serial.println("Restarting");
        request->send(200, "text/plain", "Done. ESP will restart.");
    });

    server.begin();

    while (1) {
        yield();
        if (restart) {
            LittleFS.end();
            ConfigHandler.saveWebConfig(webConfig);
            delay(3000);
            ESP.restart();
        }
    }
}

void setDateTime()
{
    // You can use your own timezone, but the exact time is not used at all.
    // Only the date is needed for validating the certificates.
    configTime(TZ_Europe_Berlin, "pool.ntp.org", "time.nist.gov");

    Serial.print("Waiting for time sync: ");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
        delay(100);
        Serial.print(".");
        now = time(nullptr);
    }
    Serial.println();

    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.printf("%s %s", tzname[0], asctime(&timeinfo));
}

void setup()
{
    //  setting serial output
    Serial.begin(115200);
    while (!Serial) {
        ;
    }
    delay(250);
    if (!LittleFS.begin()) {
        Serial.println(F("An Error has occurred while mounting LittleFS"));
        return;
    }
    delay(250);
    //  read webConfig from memory
    ConfigHandler.readWebConfig(webConfig);
    // try to connect to wifi
    resetCredentials = setupWIFI();
    if (resetCredentials) {
        // if cannot connect to WIFI setup AD to setup new credentianls
        startAP();
    } else {
        // set date and time
        setDateTime();
        // get certificates
        int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
        if (numCerts == 0) {
            return; // Can't connect to anything w/o certs!
        }
        BearSSL::WiFiClientSecure* bear = new BearSSL::WiFiClientSecure();
        bear->setCertStore(&certStore);
        // create MQTT client and set it
        client = new PubSubClient(*bear);
        client->setServer("cc41b5b17bf04d40ada9022a7d1c5d47.s2.eu.hivemq.cloud", 8883);
        client->setCallback(callback);
        // setup OTA client
        setupOTA();
        // setting mode for button control
        pinMode(UPWARD_BUTTON, INPUT);
        pinMode(DOWNWARD_BUTTON, INPUT);
        // setting stepper properties
        ConfigHandler.readBlindsConfig(blindsConfig);
        stepper.begin(60, 1);
        stepper.setEnableActiveState(LOW);
        stepper.disable();
    }
}

void loop()
{
    // OTA handler
    ArduinoOTA.handle();
    // manage MQTT connection
    if (!client->connected()) {
        reconnect();
    }
    client->loop();
    // if motor is not working make sure to disable power to coils
    if (stepper.getStepsRemaining() == 0) {
        stepper.disable();
    }
    // handle movement by buttons
    moveByButtons();
    // saving blinds config if enought time has passed and set has changed
    currentMillis = millis();
    if (currentMillis - lastMillis > timeInterval) {
        lastMillis = currentMillis;
        if (blindsConfigChanged) {
            ConfigHandler.saveBlindsConfig(blindsConfig);
            blindsConfigChanged = false;
        }
    }
}
