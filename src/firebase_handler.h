#pragma once
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include "logger.h"

#define FIREBASE_DATABASE_URL "https://tucapyenergy-default-rtdb.europe-west1.firebasedatabase.app"
#define API_KEY "AIzaSyAA9CCYeT44Mt8BLOkqpL4uu3cp5gpaevs"

#define USER_EMAIL "esp32@tucapy.cz"
#define USER_PASSWORD "123456"



namespace FirebaseHandler {

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


void setup() {
    config.database_url = FIREBASE_DATABASE_URL;
    config.api_key = API_KEY;

    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    Serial.println("Cekam na Firebase token...");
    unsigned long t = millis();
    while (!Firebase.ready() && millis() - t < 10000) {
        delay(100);
        Serial.print(".");
    }
    
    if (Firebase.ready()) {
        Serial.print("\nFirebase: READY (UID: ");
        Serial.print(auth.token.uid.c_str());
        Serial.println(")");
    } else {
        Serial.println("\nFirebase: NOT READY. Zkontroluj WiFi nebo přihlašovací údaje.");
    }
}



bool checkDataAge(FirebaseJson & json)
{
    FirebaseJsonData res;
    json.get(res,"last_update");
    int timediff=time(NULL) - res.intValue;
    return (timediff<60);
}

bool recoverData(int & idx, bool & power_mode)
{
    if(!Firebase.RTDB.getJSON(&fbdo,"/recovery_data"))return false;
    FirebaseJson* json = fbdo.jsonObjectPtr();
    if(!checkDataAge(*json))return false;

    FirebaseJsonData res;
    json->get(res,"idx");
    idx=res.intValue;
    
    json->get(res,"power_mode");
    power_mode=res.intValue;
    return true;
}

bool getConfigData(int & upper_soc, int & lower_soc, int & upper_current,
                    int & lower_current,bool & heat)
{
    if(!Firebase.RTDB.getJSON(&fbdo,"/config_data"))return false;
    FirebaseJson* json = fbdo.jsonObjectPtr();
    
    FirebaseJsonData res;
    json->get(res,"upper_soc");
    upper_soc=res.intValue;
    json->get(res,"lower_soc");
    lower_soc=res.intValue;
    json->get(res,"upper_current");
    upper_current=res.intValue;
    json->get(res,"lower_current");
    lower_current=res.intValue;
    json->get(res,"heat");
    heat=res.intValue;
    
    return true;
}



void updateData(float battery_P, float battery_I, float grid_I, float battery_soc, String status_msg, String version, const String& consoleLogs, int relay_idx,bool power_mode) {
    if (!Firebase.ready()) {
        static unsigned long lastWarn = 0;
        if (millis() - lastWarn > 10000) {
            webLog("Firebase NOT READY", true);
            lastWarn = millis();
        }
        return;
    }

    String trimmedLogs = consoleLogs;
    if (trimmedLogs.length() > 1500) {
        trimmedLogs = trimmedLogs.substring(trimmedLogs.length() - 1500);
    }

//Battery
    FirebaseJson batteryJson;
    batteryJson.set("P", battery_P);
    batteryJson.set("I", battery_I);
    batteryJson.set("grid_I", grid_I);
    batteryJson.set("soc", battery_soc);

    if (!Firebase.RTDB.setJSON(&fbdo, "/battery_data", &batteryJson)) {
        webLog("FB err(battery) " + String(fbdo.httpCode()) + ": " + fbdo.errorReason(), true);
    }

//System
    FirebaseJson systemJson;
    systemJson.set("status_msg", status_msg);
    systemJson.set("version", version);
    systemJson.set("console_log", trimmedLogs);

    if (!Firebase.RTDB.setJSON(&fbdo, "/web_data", &systemJson)) {
        webLog("FB err(system) " + String(fbdo.httpCode()) + ": " + fbdo.errorReason(), true);
    }

//Control
    FirebaseJson controlJson;
    controlJson.set("idx", relay_idx);
    controlJson.set("power_mode", power_mode);
    controlJson.set("last_update", (int)time(NULL));

    if (!Firebase.RTDB.setJSON(&fbdo, "/recovery_data", &controlJson)) {
        webLog("FB err(control) " + String(fbdo.httpCode()) + ": " + fbdo.errorReason(), true);
    }
    }

}
