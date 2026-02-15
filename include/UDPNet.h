#ifndef UDPNET_H
#define UDPNET_H

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>

extern unsigned long debounceTime;

class UDPNet {
private:
    WiFiUDP udp;
    WiFiClient tcpClient;
    IPAddress serverIP;
    unsigned long lastHeartbeatSent = 0;
    unsigned long lastDiscoveryRequest = 0;
    unsigned long lastServerResponse = 0;
    unsigned long lastTCPCheck = 0;
    
    const int udpLocalPort = 8889;  
    const int udpServerPort = 8888;
    const int tcpServerPort = 8887;
    bool serverFound = false;
    bool wasConnected = false;
    bool tcpConnected = false;

public:
    void begin() {
        Serial.println("[NET] Initializing network...");
        serverFound = false;
        tcpConnected = false;
        serverIP = INADDR_NONE;
        udp.stop();
        
        if (udp.begin(udpLocalPort)) {
            Serial.printf("[UDP] Listening on port %d\n", udpLocalPort);
            lastDiscoveryRequest = millis() - 6000;
        } else {
            Serial.println("[UDP] FATAL: Port bind failed.");
        }
    }

    void requestDiscovery() {
        Serial.println("[UDP] Broadcasting DISCOVER_SERVER...");
        udp.beginPacket(IPAddress(255, 255, 255, 255), udpServerPort);
        udp.print("DISCOVER_SERVER");
        udp.endPacket();
        lastDiscoveryRequest = millis();
    }

    void sendTagEvent() {
        if (!serverFound) {
            Serial.println("[NET] [X] Can't send tag: Server not found!");
            return;
        }
        
        // Try TCP first
        if (tcpConnected && tcpClient.connected()) {
            Serial.println("[TCP] [TAG] Sending tag event via TCP");
            tcpClient.print("TAG|");
            tcpClient.println(WiFi.macAddress());
            Serial.println("[TCP] [OK] Tag event sent successfully!");
            return;
        }
        
        // Fallback to UDP
        Serial.println("[UDP] [TAG] Sending tag event via UDP");
        if (udp.beginPacket(serverIP, udpServerPort)) {
            String event = "EVENT|TAG|" + WiFi.macAddress();
            udp.print(event);
            if (udp.endPacket()) {
                Serial.println("[UDP] [OK] Tag event sent successfully!");
            } else {
                Serial.println("[UDP] [X] Failed to send tag event!");
                serverFound = false;
            }
        }
    }

    void sendHitEvent(char shooterID, bool valid) {
        if (!serverFound) {
            Serial.println("[NET] [X] Can't send hit: Server not found!");
            return;
        }

        String validStr = valid ? "1" : "0";
        String payload = String(shooterID) + "|" + validStr;

        // Try TCP first
        if (tcpConnected && tcpClient.connected()) {
            Serial.printf("[TCP] [HIT] Sending hit from %c (Valid: %s)\n", shooterID, validStr.c_str());
            tcpClient.print("HIT|");
            tcpClient.print(payload); // HIT|<ShooterID>|<Valid>
            tcpClient.print("|");
            tcpClient.println(WiFi.macAddress()); // Append our MAC for identification
            Serial.println("[TCP] [OK] Hit event sent successfully!");
            return;
        }

        // Fallback to UDP
        Serial.printf("[UDP] [HIT] Sending hit from %c (Valid: %s)\n", shooterID, validStr.c_str());
        if (udp.beginPacket(serverIP, udpServerPort)) {
            // EVENT|HIT|<ShooterID>|<Valid>|<MAC>
            String event = "EVENT|HIT|" + payload + "|" + WiFi.macAddress();
            udp.print(event);
            if (udp.endPacket()) {
                Serial.println("[UDP] [OK] Hit event sent successfully!");
            } else {
                Serial.println("[UDP] [X] Failed to send hit event!");
                serverFound = false;
            }
        }
    }

    bool connectTCP() {
        if (serverIP == INADDR_NONE) return false;
        
        if (tcpClient.connect(serverIP, tcpServerPort)) {
            Serial.println("[TCP] Connected to server");
            
            // Send registration
            tcpClient.print("REGISTER|");
            tcpClient.println(WiFi.macAddress());
            
            tcpConnected = true;
            return true;
        } else {
            Serial.println("[TCP] Connection failed");
            tcpConnected = false;
            return false;
        }
    }

    void update() {
        if (WiFi.status() != WL_CONNECTED) {
            if (wasConnected) {
                Serial.println("[NET] WiFi disconnected!");
                serverFound = false;
                tcpConnected = false;
                tcpClient.stop();
                wasConnected = false;
            }
            return;
        }
        
        if (!wasConnected) {
            Serial.println("[NET] WiFi connected, IP: " + WiFi.localIP().toString());
            wasConnected = true;
        }

        // Request discovery every 3 seconds until server found
        if (!serverFound && (millis() - lastDiscoveryRequest > 3000)) {
            requestDiscovery();
        }

        // Check if server hasn't responded in 15 seconds
        if (serverFound && (millis() - lastServerResponse > 15000)) {
            Serial.println("[NET] No server response for 15s, reconnecting...");
            serverFound = false;
            tcpConnected = false;
            tcpClient.stop();
            requestDiscovery();
        }

        // Try to establish TCP connection if not connected
        if (serverFound && !tcpConnected && (millis() - lastTCPCheck > 5000)) {
            lastTCPCheck = millis();
            connectTCP();
        }

        // Process incoming UDP packets
        int packetSize = udp.parsePacket();
        if (packetSize) {
            char buf[128];
            int len = udp.read(buf, 127);
            if (len > 0) {
                buf[len] = 0;
                String msg = String(buf);
                msg.trim();

                Serial.printf("[UDP] Received: '%s' from %s\n", 
                    msg.c_str(), udp.remoteIP().toString().c_str());

                // SERVER DISCOVERY RESPONSES
                if (msg == "ESPNet-Server-Online") {
                    serverIP = udp.remoteIP();
                    if (!serverFound) {
                        Serial.printf("[UDP] [OK] SERVER FOUND! IP: %s\n", serverIP.toString().c_str());
                        serverFound = true;
                        lastServerResponse = millis();
                        sendHeartbeat();
                        connectTCP(); // Try to connect TCP immediately
                    } else {
                        lastServerResponse = millis();
                    }
                }
                // HEARTBEAT ACKNOWLEDGMENT
                else if (msg == "HEARTBEAT_ACK") {
                    lastServerResponse = millis();
                    if (!serverFound) {
                        serverFound = true;
                        serverIP = udp.remoteIP();
                        Serial.printf("[UDP] Server reconnected via ACK: %s\n", 
                            serverIP.toString().c_str());
                    }
                }
                // RESTART COMMAND
                else if (msg == "RESTART") {
                    Serial.println("[CMD] [!] RESTART command received!");
                    Serial.println("[CMD] Rebooting ESP32 in 1 second...");
                    delay(1000);
                    ESP.restart();
                }
                // WIFI RESET COMMAND
                else if (msg == "WIFI_RESET") {
                    Serial.println("[CMD] [WIFI] WIFI_RESET command received!");
                    Serial.println("[CMD] Clearing WiFi credentials...");
                    
                    // Clear WiFi credentials
                    Preferences prefs;
                    prefs.begin("wifi-creds", false);
                    prefs.clear();
                    prefs.end();
                    
                    Serial.println("[CMD] WiFi credentials cleared!");
                    Serial.println("[CMD] Restarting to captive portal...");
                    delay(1000);
                    ESP.restart();
                }
            }
        }

        // Send heartbeat every 5 seconds as requested
        if (serverFound && (millis() - lastHeartbeatSent > 5000)) {
            sendHeartbeat();
        }

        // Process TCP messages
        if (tcpConnected && tcpClient.available()) {
            String msg = tcpClient.readStringUntil('\n');
            msg.trim();
            
            if (msg.length() > 0) {
                Serial.printf("[TCP] Received: '%s'\n", msg.c_str());
                
                if (msg == "RESTART") {
                    Serial.println("[CMD] [!] RESTART command received via TCP!");
                    Serial.println("[CMD] Rebooting ESP32 in 1 second...");
                    delay(1000);
                    ESP.restart();
                }
                else if (msg == "WIFI_RESET") {
                    Serial.println("[CMD] [WIFI] WIFI_RESET command received via TCP!");
                    Serial.println("[CMD] Clearing WiFi credentials...");
                    
                    Preferences prefs;
                    prefs.begin("wifi-creds", false);
                    prefs.clear();
                    prefs.end();
                    
                    Serial.println("[CMD] WiFi credentials cleared!");
                    Serial.println("[CMD] Restarting to captive portal...");
                    delay(1000);
                    ESP.restart();
                }
                else if (msg == "REGISTERED") {
                    Serial.println("[TCP] [OK] Registration confirmed by server");
                }
                else if (msg.startsWith("CONFIG|DEBOUNCE|")) {
                    String valStr = msg.substring(16);
                    long val = valStr.toInt();
                    if (val >= 0) {
                        debounceTime = val;
                        Serial.printf("[CMD] [CONFIG] Updated debounce time to %lu ms\n", debounceTime);
                    }
                }
            }
        }

        // Check TCP connection
        if (tcpConnected && !tcpClient.connected()) {
            Serial.println("[TCP] Connection lost");
            tcpConnected = false;
        }
    }

    void sendHeartbeat() {
        if (serverIP == INADDR_NONE) return;
        
        if (udp.beginPacket(serverIP, udpServerPort)) {
            String hb = "HEARTBEAT|" + WiFi.macAddress() + "|" + WiFi.localIP().toString();
            udp.print(hb);
            if (udp.endPacket()) {
                lastHeartbeatSent = millis();
                // Reduced logging spam
                static unsigned long lastHeartbeatLog = 0;
                if (millis() - lastHeartbeatLog > 20000) {
                    Serial.printf("[UDP] [HB] Heartbeat sent to %s\n", serverIP.toString().c_str());
                    lastHeartbeatLog = millis();
                }
            } else {
                Serial.println("[UDP] [X] Heartbeat failed. Server link lost.");
                serverFound = false;
                tcpConnected = false;
                tcpClient.stop();
            }
        }
    }

    bool isServerFound() {
        return serverFound;
    }

    bool isTCPConnected() {
        return tcpConnected;
    }
};

#endif