#pragma once
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include "logger.h"


#define VERSION_URL  "https://raw.githubusercontent.com/Tucapy/TucapyEnergyRegulation_ESP32/main//version.txt"
#define FIRMWARE_URL "https://raw.githubusercontent.com/Tucapy/TucapyEnergyRegulation_ESP32/main/firmware.bin"

namespace OTA {

Preferences prefs;

String getCurrentSHA()
{
    prefs.begin("ota", true);
    String sha = prefs.getString("sha", "");
    prefs.end();
    return sha;
}

void saveSHA(String sha)
{
    prefs.begin("ota", false);
    prefs.putString("sha", sha);
    prefs.end();
}

String fetchString(String url) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int code = http.GET();
    webLog("HTTP GET " + url + " -> " + String(code));

    String result = "";
    if (code == 200)
    {
        result = http.getString();
        result.trim();
    }
    else
    {
        webLog("Chyba: " + http.errorToString(code));
    }

    http.end();
    return result;
}

void check() {
    webLog("Kontroluji verzi...");
    String latestSHA = fetchString(VERSION_URL);
    if (latestSHA.isEmpty()) {
        webLog("Nepodarilo se ziskat verzi.");
        return;
    }

    String currentSHA = getCurrentSHA();
    webLog("Ulozena SHA:  " + currentSHA);
    webLog("GitHub SHA:   " + latestSHA);

    if (latestSHA == currentSHA) {
        webLog("Firmware je aktualni.");
        return;
    }

    webLog("Novy firmware nalezen, stahuji...");

    HTTPClient http;
    http.begin(FIRMWARE_URL);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    if (http.GET() == 200)
    {
        int size = http.getSize();
        if (size <= 0) size = UPDATE_SIZE_UNKNOWN;
        WiFiClient* stream = http.getStreamPtr();

        if (Update.begin(size))
        {
            Update.writeStream(*stream);
            if (Update.end(true))
            {
                saveSHA(latestSHA);
                webLog("Update OK, restartuji...");
                ESP.restart();
            }
            else
            {
                webLog("Update Error: " + String(Update.errorString()));
            }
        }
        else
        {
            webLog("Update Error: " + String(Update.errorString()));
        }
    }
    http.end();
}

}