#include <Arduino.h>
#include "CaptiveWifi.h"

const int BYPASS_PIN = 0; 
CaptiveWifi wifiManager;

void setup() {
    Serial.begin(115200);
    pinMode(BYPASS_PIN, INPUT_PULLUP);

    Serial.println("\n--- ESP32 Bootloader ---");
    Serial.println("Hold BOOT button to force Config.");

    bool forceConfig = false;
    int countdown = 5;
    unsigned long lastTick = 0;
    unsigned long startTime = millis();
    
    while (millis() - startTime < 5000) {
        if (digitalRead(BYPASS_PIN) == LOW) {
            forceConfig = true;
            break;
        }
        if (millis() - lastTick >= 1000) {
            Serial.printf("Waiting... %d\n", countdown);
            countdown--;
            lastTick = millis();
        }
        yield();
    }

    if (forceConfig) {
        // You can change the prefix here
        wifiManager.startPortal("Sachi-ESP32"); 
    } else {
        if (!wifiManager.tryConnect()) {
            Serial.println("[!] Connection failed. Starting Portal...");
            wifiManager.startPortal("Sachi-ESP32");
        }
    }

    Serial.println("------------------------------------");
    Serial.print("[SUCCESS] Connected to: ");
    Serial.println(WiFi.SSID());
    Serial.print("[SUCCESS] IP Address:   ");
    Serial.println(WiFi.localIP());
    Serial.println("------------------------------------");
}

void loop() {
    // App Logic
}