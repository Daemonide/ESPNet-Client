#include <Arduino.h>
#include "CaptiveWifi.h"
#include "UDPNet.h"

CaptiveWifi wifiManager;
UDPNet networking;

const int LASER_PIN = 15; 
const int BOOT_PIN = 0;
const int BUTTON_PIN = 13;

unsigned long lastButtonPress = 0;
bool buttonPressed = false;
unsigned long lastStatusPrint = 0;
unsigned long connectionStart = 0;

void setup() {
    Serial.begin(115200);
    delay(100);
    
    pinMode(BOOT_PIN, INPUT_PULLUP); 
    pinMode(LASER_PIN, INPUT_PULLUP);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    Serial.println("\n\n+======================================+");
    Serial.println("|     ESPNet LASER TAG NODE v3.1       |");
    Serial.println("+======================================+");
    Serial.println();

    // ============================================
    // 2-SECOND BOOT BUTTON CHECK
    // ============================================
    Serial.println("[BOOT] Press BOOT button within 2 seconds to reset WiFi...");
    Serial.print("[BOOT] ");
    
    unsigned long bootCheckStart = millis();
    bool bootPressed = false;
    
    while (millis() - bootCheckStart < 2000) {
        if (digitalRead(BOOT_PIN) == LOW) {
            bootPressed = true;
            Serial.println("\n[BOOT] [OK] BOOT button detected!");
            break;
        }
        
        // Print countdown dots
        if ((millis() - bootCheckStart) % 200 == 0) {
            Serial.print(".");
        }
        delay(10);
    }
    
    if (bootPressed) {
        Serial.println("[BOOT] Clearing WiFi credentials and starting captive portal...");
        wifiManager.clearCredentials();
        delay(500);
        wifiManager.startPortal();
    } else {
        Serial.println("\n[BOOT] No button press detected, continuing normal boot...");
    }

    // ============================================
    // NORMAL WIFI CONNECTION
    // ============================================
    if (!wifiManager.tryConnect()) {
        Serial.println("[WIFI] No saved credentials, starting portal...");
        wifiManager.startPortal();
    } else {
        Serial.println("[WIFI] [OK] Connected to saved network!");
    }

    Serial.println("==========================================");
    Serial.println("[WIFI] SSID: " + WiFi.SSID());
    Serial.print("[WIFI] IP: "); Serial.println(WiFi.localIP());
    Serial.print("[WIFI] MAC: "); Serial.println(WiFi.macAddress());
    Serial.print("[WIFI] RSSI: "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
    Serial.println("==========================================");
    
    networking.begin();
    connectionStart = millis();
    Serial.println("[SYSTEM] UDP Networking Ready");
    Serial.println("[SYSTEM] Searching for server...");
    Serial.println("==========================================\n");
}

void loop() {
    networking.update();

    // BUTTON PRESS (Manual tag event)
    if (digitalRead(BUTTON_PIN) == LOW && !buttonPressed) {
        buttonPressed = true;
        lastButtonPress = millis();
        Serial.println("[BUTTON] [TAG] Manual tag event triggered!");
        networking.sendTagEvent();
    }
    
    // Button release detection
    if (buttonPressed && digitalRead(BUTTON_PIN) == HIGH) {
        buttonPressed = false;
    }

    // LASER SENSOR LOGIC (with debounce)
    static unsigned long lastLaserTrigger = 0;
    if (digitalRead(LASER_PIN) == LOW && (millis() - lastLaserTrigger > 300)) {
        lastLaserTrigger = millis();
        Serial.println("[LASER] Trigger detected!");
        networking.sendTagEvent();
    }

    // Status print every 10 seconds
    if (millis() - lastStatusPrint > 10000) {
        lastStatusPrint = millis();
        
        static bool lastServerStatus = false;
        bool currentServerStatus = networking.isServerFound();
        
        if (currentServerStatus != lastServerStatus) {
            if (currentServerStatus) {
                Serial.println("[STATUS] [OK] Connected to server");
            } else {
                Serial.println("[STATUS] [X] Searching for server...");
            }
            lastServerStatus = currentServerStatus;
        }
        
        // Connection uptime
        unsigned long uptime = (millis() - connectionStart) / 1000;
        Serial.printf("[STATUS] Uptime: %lus | WiFi: %ddBm | Server: %s\n", 
            uptime, 
            WiFi.RSSI(),
            currentServerStatus ? "Connected" : "Searching"
        );
    }
    
    yield();
}
