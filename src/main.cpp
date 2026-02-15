#include <Arduino.h>
#include "CaptiveWifi.h"
#include "UDPNet.h"
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>

CaptiveWifi wifiManager;
UDPNet networking;

// --- PIN DEFINITIONS ---
const int LASER_PIN = 15; // Kept for legacy/button support if needed, or replace?
const int BOOT_PIN = 0;
const int BUTTON_PIN = 13;
const uint16_t PIN_PRIMARY = 14;   // Full Decoder (Front)
const uint16_t PIN_SECONDARY = 23; // Activity Monitor (Back) - Reverted to 23 as per user request. 
// User said 23. But typical ESP32 dev kit pin 23 is MOSI. If user schematics say 23, I should use 23. 
// Wait, user provided snippet says: const uint16_t PIN_SECONDARY = 23;
// I will stick to user's 23 unless it conflicts. 

// --- CONFIGURATION ---
const uint8_t TEAM_ID = 0xAB; 
const unsigned long SIGNAL_TIMEOUT = 150; // Time to wait before declaring a "Missed Shot"

// --- OBJECTS ---
IRrecv irrecv(PIN_PRIMARY, 1024, 15, true);
decode_results results;

// --- INTERRUPT VARIABLES ---
volatile int pinSecondary_pulse_count = 0;
volatile unsigned long pinSecondary_last_activity = 0;

// Interrupt Service Routine (Fast & Lightweight)
void IRAM_ATTR onSecondaryPinChange() {
  pinSecondary_pulse_count++;
  pinSecondary_last_activity = millis();
}

unsigned long lastButtonPress = 0;
bool buttonPressed = false;
unsigned long lastStatusPrint = 0;
unsigned long connectionStart = 0;

// Configurable parameters
unsigned long debounceTime = 1000; // Default 1 second

void setup() {
    Serial.begin(115200);
    delay(100);
    
    pinMode(BOOT_PIN, INPUT_PULLUP); 
    pinMode(LASER_PIN, INPUT_PULLUP);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // 1. Setup Primary Receiver
    irrecv.enableIRIn();

    // 2. Setup Secondary Monitor (User said 23, but checking if 23 is safe. 
    // On ESP32 DEVKIT V1, GPIO 23 is MOSI. It is usable if SPI not used.
    // I will use 23 as requested.)
    pinMode(PIN_SECONDARY, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_SECONDARY), onSecondaryPinChange, CHANGE);

    Serial.println("\n\n+======================================+");
    Serial.println("|     ESPNet LASER TAG NODE v3.2       |");
    Serial.println("|     Dual Sensor System Active        |");
    Serial.println("+======================================+");
    Serial.println();

    // ============================================
    // 2-SECOND BOOT BUTTON CHECK
    // ============================================
    Serial.println("[BOOT] Hold BOOT button for 1 second to reset WiFi credentials...");
    Serial.print("[BOOT] ");
    
    unsigned long bootCheckStart = millis();
    unsigned long pressStart = 0;
    bool bootPressed = false;
    
    while (millis() - bootCheckStart < 3000) {
        if (digitalRead(BOOT_PIN) == LOW) {
            // Button is pressed
            if (pressStart == 0) {
                pressStart = millis();
            }
            
            // Check if held for 1 second
            if (millis() - pressStart > 1000) {
                bootPressed = true;
                Serial.println("\n[BOOT] [OK] Button held detected!");
                break;
            }
        } else {
            // Button released or noise
            pressStart = 0;
        }
        
        // Print countdown dots
        if ((millis() - bootCheckStart) % 500 == 0 && pressStart == 0) {
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

    // BUTTON PRESS (Manual tag event) - Kept for testing/backup
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

    // ---------------------------------------------------------
    // IR SENSOR LOGIC
    // ---------------------------------------------------------
    if (irrecv.decode(&results)) {
        // Check if it's our protocol (NEC)
        if (results.decode_type == decode_type_t::NEC) {
            uint32_t val = (uint32_t)results.value;
            uint8_t id = (val >> 16) & 0xFF; // Team ID
            uint8_t charCode = (val >> 8) & 0xFF; // Character (Shooter ID)
            uint8_t check = val & 0xFF; // Checksum

            // Validate Team ID and Checksum
            if (id == TEAM_ID && check == (charCode ^ 0xFF)) {
                
                // --- COINCIDENCE CHECK ---
                bool secondaryActive = false;
                // If Secondary Pin saw >10 pulses recently, it was also hit
                if (pinSecondary_pulse_count > 10 && (millis() - pinSecondary_last_activity < SIGNAL_TIMEOUT)) {
                    secondaryActive = true;
                }

                // Output Result locally
                Serial.print("HIT RECEIVED: ");
                Serial.print((char)charCode);
                Serial.print(" [Pulses: ");
                Serial.print(pinSecondary_pulse_count);
                Serial.print("]");
                
                if (secondaryActive) {
                    Serial.println(" [SUCCESS] (Both Sensors Triggered)");
                } else {
                    Serial.println(" [WARNING] (Front Sensor Only - Back Sensor Blocked)");
                }

                // Send to Server
                // If secondaryActive is true, it's a valid hit. 
                // If false, it's an "invalid shot" (primary hit, secondary missed).
                networking.sendHitEvent((char)charCode, secondaryActive);
            }
        }
        
        // Clear variables
        pinSecondary_pulse_count = 0; 
        irrecv.resume();
    }

    // ---------------------------------------------------------
    // SECONDARY SENSOR ONLY (Monitor)
    // ---------------------------------------------------------
    unsigned long now = millis();
    if (pinSecondary_pulse_count > 15 && (now - pinSecondary_last_activity > SIGNAL_TIMEOUT)) {
        // Primary missed it, but secondary saw it. 
        // User request: "Incase The primary missed it dont show anything in server"
        // So we just log locally for debug.
        Serial.println("------------------------------------------------");
        Serial.println("[FAILURE] Back Sensor hit, but Front Sensor BLIND!");
        Serial.println("          (Ignored by server protocol)");
        Serial.println("------------------------------------------------");

        pinSecondary_pulse_count = 0;
    }

    // Status print every 10 seconds
    if (millis() - lastStatusPrint > 10000) {
        lastStatusPrint = millis();
        
        static bool lastServerStatus = false;
        bool currentServerStatus = networking.isServerFound();
        bool tcpStatus = networking.isTCPConnected();
        
        if (currentServerStatus != lastServerStatus) {
            if (currentServerStatus) {
                if (tcpStatus) {
                    Serial.println("[STATUS] [OK] Connected to server (TCP)");
                } else {
                    Serial.println("[STATUS] [~] Connected to server (UDP only)");
                }
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
            currentServerStatus ? (tcpStatus ? "TCP" : "UDP") : "Searching"
        );
    }
    
    yield();
}
