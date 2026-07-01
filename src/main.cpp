#include <Arduino.h>
#include <WiFi.h>
#include "ota.h"
#include "logger.h"
#include "modbus_handler.h"
#include "firebase_handler.h"
#include "protocols_handler.h"

#define WIFI_SSID "DvorNet"
#define WIFI_PASS "dvor62tuc"
String currentVersion = "";

#define OUT1 23
#define OUT2 22
#define OUT3 21
#define OUT4 19
#define OUT5 32
#define OUT6 25
#define OUT7 26
#define OUT8 27
const int outputs[] = {OUT1, OUT2, OUT3, OUT4, OUT5, OUT6, OUT7, OUT8};
const int n_outputs=8;

int upper_soc = 80;
int lower_soc = 60;
int upper_current = 2;
int lower_current = 0;
bool heating = false;

#define OTA_INTERVAL 60000


bool power_mode=false;
int idx=0;

String logBuffer = "";

void turn_on()
{
    if(idx<n_outputs)
    {
        digitalWrite(outputs[idx],LOW);
        idx++;
    }
    return;
}
void turn_off()
{

    if (idx>0)idx--;
    else return;
    digitalWrite(outputs[idx],HIGH);
    return;
}

void shutdown()
{
    for(int pin: outputs)
    {
        digitalWrite(pin,HIGH);
    }
    idx = 0;
}


void setup() {
    Serial.begin(115200);
    
    
    for (int pin : outputs) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
    }

    Protocols::setup();
    TIME.begin();
    ModbusHandler::setup();
    FirebaseHandler::setup();

    FirebaseHandler::getConfigData(upper_soc,lower_soc,upper_current,lower_current,heating);

    if(FirebaseHandler::recoverData(idx,power_mode))
    {
        Serial.print("Obnova stavu: Stupen="); Serial.print(idx); 
        Serial.print(", Rezim="); Serial.println(power_mode);
        
        for (int i = 0; i < idx; i++)
        {
            digitalWrite(outputs[i], LOW);
            delay(10);
        }
    }


    currentVersion = OTA::getCurrentSHA().substring(0,7);
    webLog("Start systemu - verze " + currentVersion);
}

void loop() {
    static unsigned long lastOTA = millis();

// OTA kontrola az po par minutach
    if (millis() > OTA_INTERVAL && (millis() - lastOTA > OTA_INTERVAL)) {
        lastOTA = millis(); 
        OTA::check();
    }

    FirebaseHandler::getConfigData(upper_soc,lower_soc,upper_current,lower_current,heating);

    bool modbusOK = ModbusHandler::update();
    webLog("Config data: "+String(upper_soc)+" | "+String(lower_soc)+" | "+
                            String(heating)+" | "+String(power_mode));

    if(heating && modbusOK)
    {
        static unsigned long lastSwitch = 0;
        
        if(ModbusHandler::battery_soc < lower_soc)
        {
            power_mode = false;
            if(idx > 0) shutdown();
        }
        if(ModbusHandler::battery_soc>=upper_soc)power_mode=true;
        
// Pause between next round
        if (millis() - lastSwitch > 10000) {
            if(power_mode && (ModbusHandler::battery_I>=upper_current)) {
                turn_on();
                lastSwitch = millis();
            }
            if(power_mode && (ModbusHandler::battery_I<=lower_current)) {
                turn_off();
                lastSwitch = millis();
            }
        }
    }
    else if(!heating){
        shutdown();
    }

// Push to Firebase
    static unsigned long lastFirebasePush = 0;
    if (millis() - lastFirebasePush > 5000) {
        lastFirebasePush = millis();
        FirebaseHandler::updateData(
            ModbusHandler::battery_P,
            ModbusHandler::battery_I,
            ModbusHandler::grid_I,
            ModbusHandler::battery_soc,
            ModbusHandler::status_msg,
            currentVersion,
            logBuffer,
            idx,
            power_mode
        );
    }

//MQTT
    Protocols::maintainMQTT();

    static unsigned long lastMqttPush = 0;
    if (millis() - lastMqttPush > 10000) {
        lastMqttPush = millis();
        Protocols::pushMqtt("esp32/energy/battery_soc",ModbusHandler::battery_soc);
        Protocols::pushMqtt("esp32/energy/battery_P",ModbusHandler::battery_P);
        Protocols::pushMqtt("esp32/energy/grid_I",ModbusHandler::grid_I);
    }
}