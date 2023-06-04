// TODO - po testach usunąć WebSerial, 
#include "configHandler.h" // used to read and write configs to memory
#include "stepperDriver.h"
#include <ArduinoOTA.h> // OTA library
#include <CertStoreBearSSL.h> // read certs and use them
#include <ESP8266WiFi.h> // WiFi library
#include <ESPAsyncTCP.h> // library for AsyncWebServer
#include <ESPAsyncWebServer.h> // WiFi server library
#include <LittleFS.h> // File system library
#include <PubSubClient.h> // MQTT library
#include <TZ.h>
#include <WebSerial.h> // TODO - delete later, only for debugging
#include <ezButton.h> // for debouncing buttons
#include <time.h>

#define SPIFFS LittleFS
// stepper motor
#define MOTOR_STEPS 200
#define STEP D1
#define DIR D2
#define DISABLE_POWER D7
// buttons
#define UPWARD_BUTTON D5
#define DOWNWARD_BUTTON D6
#define RESET_BUTTON D8
// object used to run a stepper motor
StepperDriver stepperDriver(STEP, DIR, DISABLE_POWER);
// WiFi client and certStore
WiFiClientSecure espClient;
BearSSL::CertStore certStore;
// MQTT client
PubSubClient* client;
// MQTT topic which esp uses to receive messages
const char topic[] = "ESP8266/blinds/in";
// MQTT cliend ID
String clientId = "ESP8266Blinds";
// configs and configs handler
struct webConfig webConfig;
struct blindsConfig blindsConfig;
ConfigHandler configHandler;
// 
bool restart = false;
bool manualMode = false;
// delete below after deleting webserial - TODO
long lastMillis = 0;
long currentMillis = 0;
const long timeInterval = 30000; // 30 seconds
// WebServer for WebSerial debugging - delete later TODO
AsyncWebServer serialServer(80);
// buttons and debouncing
ezButton buttonUp(UPWARD_BUTTON, INPUT);
ezButton buttonDown(DOWNWARD_BUTTON, INPUT);

// check the message from MQTT broker and do what it says
void handleMessage(String msg)
{
    Serial.println(msg);
    if (msg == "(reset)") {
        WebSerial.println("Reseting WiFi and MQTT data");
        // startAP();
        return;
    } else if (msg == "(set-zero)") {
        WebSerial.println("Setting 0 position");
        blindsConfig.currentPosition = 0;
        configHandler.saveBlindsConfig(blindsConfig);
    } else if (msg == "(set-max)") {
        WebSerial.println("Setting max position");
        blindsConfig.maxSteps = blindsConfig.currentPosition;
        configHandler.saveBlindsConfig(blindsConfig);
    } else if (msg == "(manual)") {
        WebSerial.println("Manual");
        manualMode = !manualMode;
    } else if (msg == "(save-now)") {
        WebSerial.println("Saving configs.");
        configHandler.saveBlindsConfig(blindsConfig);
        configHandler.saveWebConfig(webConfig);
    } else if (msg[0] == '+') {
        int steps = msg.substring(1).toInt();
        Serial.println(steps);
        stepperDriver.moveSteps(steps);
    } else if (msg[0] == '-') {
        int steps = -1 * msg.substring(1).toInt();
        Serial.println(steps);
        stepperDriver.moveSteps(steps);
    } else {
        int percent = msg.toInt();
        Serial.println(percent);
        stepperDriver.movePercent(percent);
    }
}

// process messages incoming from MQTT into String format
void callback(char* topic, byte* payload, unsigned int length)
{
    Serial.print("Message on topic ");
    Serial.println(topic);
    String msg = "";
    for (int i = 0; i < length; i++) {
        msg += String((char)payload[i]);
    }
    Serial.printf("%s \n", msg);
    handleMessage(msg.c_str());
}

// connects to WiFi network, if fails returns false else true
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
            return false;
        }
    }
    randomSeed(micros());
    Serial.printf("Connected to: %s at IP: ", webConfig.ssid);
    Serial.println(WiFi.localIP());
    return true;
}

// manages MQTT broker connection and reconnection
void reconnect()
{
    while (!client->connected()) {
        Serial.println("Attempting MQTT connection.");
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

// configures and starts OTA
void setupOTA()
{
    ArduinoOTA.onStart([]() {
        LittleFS.end();
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else {
            type = "filesystem";
        }
        // Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
        WebSerial.print("\nEnd of update, restarting now.\n");
        delay(2000);
        ESP.restart();
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        // Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        // Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            // Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            // Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            // Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            // Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            // Serial.println("End Failed");
        }
    });
    ArduinoOTA.setPassword((const char*)webConfig.otaPassword);
    ArduinoOTA.begin();
}

// function used to set new web settings, starts AP and a simple website
/*
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
                    // Serial.print("SSID set to: ");
                    // Serial.println(webConfig.ssid);
                }
                if (p->name() == "password") {
                    strlcpy(webConfig.password, p->value().c_str(), 24);
                    // Serial.print("Password set to: ");
                    // Serial.println(webConfig.password);
                }
                if (p->name() == "mqttUsername") {
                    strlcpy(webConfig.mqttUsername, p->value().c_str(), 24);
                    // Serial.print("IP Address set to: ");
                    // Serial.println(webConfig.mqttUsername);
                }
                if (p->name() == "mqttPassword") {
                    strlcpy(webConfig.mqttPassword, p->value().c_str(), 24);
                    // Serial.print("Gateway set to: ");
                    // Serial.println(webConfig.mqttPassword);
                }
                if (p->name() == "mqttServer") {
                    strlcpy(webConfig.mqttServer, p->value().c_str(), 24);
                    // Serial.print("Gateway set to: ");
                    // Serial.println(webConfig.mqttServer);
                }
                if (p->name() == "otaPassword") {
                    strlcpy(webConfig.otaPassword, p->value().c_str(), 24);
                    // Serial.print("Gateway set to: ");
                    // Serial.println(webConfig.otaPassword);
                }
                if (p->name() == "mqttPort") {
                    webConfig.mqttPort = p->value().toInt();
                    // Serial.print("Gateway set to: ");
                    // Serial.println(webConfig.mqttPort);
                }
                //.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
            }
        }
        restart = true;
        // Serial.println("Restarting");
        request->send(200, "text/plain", "Done. ESP will restart.");
    });

    server.begin();

    while (1) {
        yield();
        if (restart) {
            LittleFS.end();
            configHandler.saveWebConfig(webConfig);
            delay(3000);
            ESP.restart();
        }
    }
}
*/

// sets proper date and time, used to load certifices to connect to MQTT with secure connection
void setDateTime()
{
    // Only the date is needed for validating the certificates.
    configTime(TZ_Europe_Berlin, "pool.ntp.org", "time.nist.gov");
    Serial.print("Waiting for time sync: ");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
        delay(100);
        Serial.print(".");
        now = time(nullptr);
    }
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    Serial.printf("%s %s", tzname[0], asctime(&timeinfo));
}

// delete when removing WebSerial - TODO
void recvMsg(uint8_t* data, size_t len)
{
    WebSerial.println("Received Data...");
    String d = "";
    for (int i = 0; i < len; i++) {
        d += char(data[i]);
    }
    WebSerial.println(d);
}

void setup()
{
    // setting serial output
    Serial.begin(115200);
    while (!Serial) {
        ;
    }
    delay(100);
    if (!LittleFS.begin()) {
        Serial.println(F("An Error has occurred while mounting LittleFS"));
        return;
    }
    delay(100);
    //  read webConfig from memory
    configHandler.readWebConfig(webConfig);
    // try to connect to wifi
    if (!setupWIFI()) {
        // if cannot connect to WIFI setup AP to get new WiFi settings - TODO uncomment later
        // startAP();
    }
    // set date and time
    setDateTime();
    // get certificates
    int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
    if (numCerts == 0) {
        Serial.println("Error while loading certs!");
        return; // Can't connect to anything w/o certs!
    }
    BearSSL::WiFiClientSecure* bear = new BearSSL::WiFiClientSecure();
    bear->setCertStore(&certStore);
    // create MQTT client and set it
    client = new PubSubClient(*bear);
    client->setServer(webConfig.mqttServer, webConfig.mqttPort);
    client->setCallback(callback);
    client->setKeepAlive(60);
    client->setSocketTimeout(60);
    // setup OTA client
    setupOTA();
    // TODO -- delete this later, debugging only
    WebSerial.begin(&serialServer);
    WebSerial.msgCallback(recvMsg);
    serialServer.begin();
    // setting reset pin
    pinMode(RESET_BUTTON, INPUT_PULLUP);
    // setting stepper properties
    configHandler.readBlindsConfig(blindsConfig)   ;
    stepperDriver.begin(50, &configHandler, &blindsConfig);
    stepperDriver.setButtons(&buttonUp, &buttonDown);
}

void loop()
{
    // OTA
    ArduinoOTA.handle();
    // manage MQTT connection
    if (!client->connected()) {
        reconnect();
    }
    client->loop();
    // movement by buttons
    buttonUp.loop();
    buttonDown.loop();
    int count = stepperDriver.moveButtons(manualMode);
    
    if( count != 0){
    WebSerial.print("Moved ");
    WebSerial.print(count);
    WebSerial.println(" steps with buttons");
    }
    // TODO - delete if and all WebSerial
    currentMillis = millis();
    if (currentMillis - lastMillis > timeInterval) {
        lastMillis = currentMillis;
        WebSerial.print("Max pos:");
        WebSerial.println(blindsConfig.maxSteps);
        WebSerial.print("Curr pos:");
        WebSerial.println(blindsConfig.currentPosition);
    }
   
    // reset wifi configuration, TODO - test this afer deleting WebSerial
    if (digitalRead(RESET_BUTTON) == HIGH) {
        // startAP();
    }
}
