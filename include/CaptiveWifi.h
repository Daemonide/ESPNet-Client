#ifndef CAPTIVEWIFI_H
#define CAPTIVEWIFI_H

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>

class CaptiveWifi {
private:
    WebServer server;
    DNSServer dnsServer;
    Preferences prefs;

    bool isIp(String str) {
        for (size_t i = 0; i < str.length(); i++) {
            int c = str.charAt(i);
            if (c != '.' && (c < '0' || c > '9')) return false;
        }
        return true;
    }

public:
    CaptiveWifi() : server(80) {}

    String getMacSuffix() {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char buf[7];
        sprintf(buf, "%02X%02X%02X", mac[3], mac[4], mac[5]);
        return String(buf);
    }

    void clearCredentials() {
        prefs.begin("wifi-creds", false);
        prefs.clear();
        prefs.end();
        Serial.println("[WIFI] Credentials cleared!");
    }

    void startPortal() {
        String fullName = "ESP-LaserTag-" + getMacSuffix();
        
        Serial.println("[PORTAL] Scanning available networks...");
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);
        
        int n = WiFi.scanNetworks();
        String opts = "";
        if (n == 0) {
            opts = "<option>No networks found</option>";
        } else {
            for (int i = 0; i < n; ++i) {
                opts += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + WiFi.RSSI(i) + "dBm)</option>";
            }
        }

        WiFi.mode(WIFI_AP);
        IPAddress apIP(192, 168, 4, 1);
        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
        WiFi.softAP(fullName.c_str());

        Serial.println("==========================================");
        Serial.println("[PORTAL] Captive Portal Started!");
        Serial.println("[PORTAL] SSID: " + fullName);
        Serial.println("[PORTAL] IP: 192.168.4.1");
        Serial.println("[PORTAL] Connect and configure WiFi");
        Serial.println("==========================================");

        dnsServer.start(53, "*", apIP);
        
        // Main page
        server.on("/", [this, opts](){
            String h = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
            h += "<style>body{font-family:Arial,sans-serif;background:linear-gradient(135deg,#0d0f14,#1a1d24);color:#fff;padding:20px;margin:0;}";
            h += ".container{max-width:400px;margin:50px auto;background:rgba(255,255,255,0.05);padding:30px;border-radius:16px;border:1px solid rgba(255,255,255,0.1);box-shadow:0 8px 32px rgba(0,0,0,0.4);}";
            h += "h2{text-align:center;color:#00d2ff;margin-bottom:30px;font-size:24px;}";
            h += "label{display:block;margin:15px 0 5px;font-weight:600;color:#aaa;font-size:12px;text-transform:uppercase;letter-spacing:1px;}";
            h += "select,input{width:100%;padding:14px;margin:8px 0 20px;border-radius:8px;border:1px solid rgba(255,255,255,0.2);background:rgba(255,255,255,0.05);color:#fff;font-size:14px;box-sizing:border-box;}";
            h += "select:focus,input:focus{outline:none;border-color:#00d2ff;box-shadow:0 0 0 2px rgba(0,210,255,0.2);}";
            h += "button{width:100%;padding:16px;background:linear-gradient(135deg,#00d2ff,#3a7bd5);border:none;border-radius:8px;font-weight:bold;color:#000;font-size:16px;cursor:pointer;transition:all 0.3s;}";
            h += "button:hover{transform:translateY(-2px);box-shadow:0 8px 24px rgba(0,210,255,0.4);}";
            h += ".info{text-align:center;margin-top:20px;font-size:11px;color:#666;}";
            h += "</style></head><body><div class='container'>";
            h += "<h2>ESP LASER TAG</h2>";
            h += "<form action='/save' method='POST'>";
            h += "<label>Select Network</label>";
            h += "<select name='ssid' required>" + opts + "</select>";
            h += "<label>WiFi Password</label>";
            h += "<input name='pass' type='password' placeholder='Enter password' required>";
            h += "<button type='submit'>[SAVE] SAVE & CONNECT</button>";
            h += "</form>";
            h += "<div class='info'>MAC: " + WiFi.macAddress() + "</div>";
            h += "</div></body></html>";
            server.send(200, "text/html", h);
        });
        
        // Save credentials
        server.on("/save", [this](){
            String ssid = server.arg("ssid");
            String pass = server.arg("pass");
            
            prefs.begin("wifi-creds", false);
            prefs.putString("ssid", ssid);
            prefs.putString("pass", pass);
            prefs.end();
            
            Serial.println("[PORTAL] Credentials saved!");
            Serial.println("[PORTAL] SSID: " + ssid);
            
            String h = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width'>";
            h += "<style>body{font-family:Arial;background:linear-gradient(135deg,#0d0f14,#1a1d24);color:#fff;text-align:center;padding:50px;}";
            h += ".success{background:rgba(0,255,136,0.1);border:2px solid #00ff88;padding:30px;border-radius:16px;max-width:400px;margin:auto;}";
            h += "h2{color:#00ff88;margin-bottom:20px;}</style></head>";
            h += "<body><div class='success'><h2>[OK] Success!</h2><p>WiFi credentials saved.<br>Restarting ESP32...</p></div></body></html>";
            
            server.send(200, "text/html", h);
            delay(2000); 
            ESP.restart();
        });

        // Captive portal redirect
        server.onNotFound([this](){
            server.sendHeader("Location", "http://192.168.4.1/", true);
            server.send(302, "text/plain", "");
        });
        
        server.begin();
        
        // Loop forever in portal mode
        while(true) {
            dnsServer.processNextRequest();
            server.handleClient();
            yield();
        }
    }

    bool tryConnect() {
        prefs.begin("wifi-creds", true);
        String s = prefs.getString("ssid", "");
        String p = prefs.getString("pass", "");
        prefs.end();
        
        if(s == "" || s == "null") {
            Serial.println("[WIFI] No saved credentials found");
            return false;
        }
        
        Serial.print("[WIFI] Connecting to: "); Serial.println(s);
        WiFi.mode(WIFI_STA);
        WiFi.begin(s.c_str(), p.c_str());
        
        int attempts = 0;
        while(WiFi.status() != WL_CONNECTED && attempts < 30) {
            delay(500); 
            Serial.print(".");
            attempts++;
        }
        Serial.println();
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("[WIFI] Connection successful!");
            return true;
        } else {
            Serial.println("[WIFI] Connection failed!");
            return false;
        }
    }
};

#endif
