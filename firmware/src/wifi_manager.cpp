#include "wifi_manager.h"
#include "config.h"
#include <WiFi.h>
#include <ESPmDNS.h>

bool wifiConnect() {
    WiFi.setHostname(HOSTNAME);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    Serial.printf("[wifi] Connecting to %s", WIFI_SSID);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 15000) {
            Serial.println("\n[wifi] Connection timed out");
            return false;
        }
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n[wifi] Connected, IP: %s\n", WiFi.localIP().toString().c_str());

    if (MDNS.begin(HOSTNAME)) {
        Serial.printf("[wifi] mDNS started: http://%s.local/\n", HOSTNAME);
    }
    return true;
}
