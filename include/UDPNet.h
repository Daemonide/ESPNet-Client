#ifndef UDPNET_H
#define UDPNET_H

#include <WiFi.h>
#include <WiFiUdp.h>

class UDPNet {
private:
    WiFiUDP udp;
    IPAddress serverIP;
    unsigned long lastHeartbeatSent = 0;
    unsigned long lastDiscoveryRequest = 0;
    unsigned long lastServerResponse = 0;
    
    const int localPort = 8889;  
    const int serverPort = 8888;
    bool serverFound = false;
    bool wasConnected = false;

public:
    void begin() {
        Serial.println("[UDP] Initializing network...");
        serverFound = false;
        serverIP = INADDR_NONE;
        udp.stop(); 
        delay(100);
        
        if (udp.begin(localPort)) {
            Serial.printf("[UDP] Listening on port %d\n", localPort);
            lastDiscoveryRequest = millis() - 6000;
        } else {
            Serial.println("[UDP] FATAL: Port bind failed.");
        }
    }

    void requestDiscovery() {
        Serial.println("[UDP] Broadcasting DISCOVER_SERVER...");
        udp.beginPacket(IPAddress(255, 255, 255, 255), serverPort);
        udp.print("DISCOVER_SERVER");
        udp.endPacket();
        lastDiscoveryRequest = millis();
    }

    void sendTagEvent() {
        if (!serverFound) {
            Serial.println("[UDP] [X] Can't send tag: Server not found!");
            return;
        }
        
        Serial.println("[UDP] [TAG] SENDING TAG EVENT");
        if (udp.beginPacket(serverIP, serverPort)) {
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

    void update() {
        if (WiFi.status() != WL_CONNECTED) {
            if (wasConnected) {
                Serial.println("[UDP] WiFi disconnected!");
                serverFound = false;
                wasConnected = false;
            }
            return;
        }
        
        if (!wasConnected) {
            Serial.println("[UDP] WiFi connected, IP: " + WiFi.localIP().toString());
            wasConnected = true;
        }

        // Request discovery every 3 seconds until server found
        if (!serverFound && (millis() - lastDiscoveryRequest > 3000)) {
            requestDiscovery();
        }

        // Check if server hasn't responded in 15 seconds
        if (serverFound && (millis() - lastServerResponse > 15000)) {
            Serial.println("[UDP] No server response for 15s, reconnecting...");
            serverFound = false;
            requestDiscovery();
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
                if (msg == "ESPNet-Server-Online" || msg == "espnet-server-discovery") {
                    serverIP = udp.remoteIP();
                    if (!serverFound) {
                        Serial.printf("[UDP] [OK] SERVER FOUND! IP: %s\n", serverIP.toString().c_str());
                        serverFound = true;
                        lastServerResponse = millis();
                        sendHeartbeat();
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

        // Send heartbeat every 4 seconds when connected
        if (serverFound && (millis() - lastHeartbeatSent > 4000)) {
            sendHeartbeat();
        }
    }

    void sendHeartbeat() {
        if (serverIP == INADDR_NONE) return;
        
        if (udp.beginPacket(serverIP, serverPort)) {
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
            }
        }
    }

    bool isServerFound() {
        return serverFound;
    }
};

#endif
