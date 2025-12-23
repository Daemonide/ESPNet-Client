#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>

class CaptiveWifi {
private:
    WebServer server;
    DNSServer dnsServer;
    Preferences prefs;
    const byte DNS_PORT = 53;
    String fullApName; // Stores the calculated unique SSID

    // Helper: Calculates unique ID from MAC Address
    String getUniqueName(String baseName) {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char uniqueId[7];
        // Uses the last 3 bytes of the MAC (e.g., 0A:1B:2C)
        snprintf(uniqueId, sizeof(uniqueId), "-%02X%02X%02X", mac[3], mac[4], mac[5]);
        return baseName + uniqueId;
    }
    
    String getHTMLHead() {
        return "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
               "<style>body{font-family:-apple-system,sans-serif; background:#f0f2f5; color:#1c1e21; display:flex; flex-direction:column; align-items:center; padding:20px;}"
               ".card{background:white; padding:24px; border-radius:16px; box-shadow:0 8px 24px rgba(0,0,0,0.1); width:100%; max-width:380px;}"
               "h2{margin:0 0 10px 0; color:#007bff;} h3{font-size:14px; color:#65676b; text-transform:uppercase; margin-bottom:10px;}"
               ".btn{background:#007bff; color:white; border:none; padding:14px; border-radius:8px; width:100%; cursor:pointer; font-weight:bold; font-size:16px; margin-top:10px; transition:0.2s;}"
               ".btn:active{transform:scale(0.98); background:#0069d9;}"
               ".btn-red{background:#fa3e3e;} .btn-red:active{background:#d92121;}"
               ".btn-outline{background:none; border:2px solid #007bff; color:#007bff; margin-top:20px;}"
               "input{width:100%; padding:12px; margin:8px 0; border:1px solid #dddfe2; border-radius:8px; box-sizing:border-box; font-size:16px;}"
               ".wifi-list{list-style:none; padding:0; margin:0 0 20px 0; max-height:200px; overflow-y:auto; border:1px solid #eee; border-radius:8px;}"
               ".wifi-item{padding:12px; border-bottom:1px solid #eee; cursor:pointer; display:flex; justify-content:space-between; align-items:center;}"
               ".wifi-item:last-child{border-bottom:none;} .wifi-item:hover{background:#f0f7ff;}</style></head><body><div class='card'>";
    }

    void handleRoot() {
        String html = getHTMLHead() + "<h2>WiFi Setup</h2>";
        html += "<h3>Available Networks</h3><ul class='wifi-list'>";
        int n = WiFi.scanNetworks();
        if (n <= 0) {
            html += "<li class='wifi-item'>No networks found</li>";
        } else {
            for (int i = 0; i < n; ++i) {
                html += "<li class='wifi-item' onclick='f(\"" + WiFi.SSID(i) + "\")'><span>" + WiFi.SSID(i) + "</span><small>" + String(WiFi.RSSI(i)) + "dBm</small></li>";
            }
        }
        html += "</ul><form action='/save' method='POST'>"
                "<input id='s' name='ssid' placeholder='WiFi Name' required>"
                "<input name='pass' type='password' placeholder='Password' required>"
                "<button class='btn'>Connect Device</button></form>"
                "<button class='btn btn-outline' onclick='location.href=\"/restart\"'>Restart ESP32</button>"
                "<button class='btn btn-red' onclick='if(confirm(\"Wipe all settings?\"))location.href=\"/clear\"'>Clear Saved WiFi</button>"
                "<script>function f(s){document.getElementById(\"s\").value=s;}</script></div></body></html>";
        server.send(200, "text/html", html);
    }

public:
    CaptiveWifi() : server(80) {}

    // You can now pass a base name like "ESP-Config"
    void startPortal(String baseName = "ESP32-Portal") {
        fullApName = getUniqueName(baseName);
        
        WiFi.mode(WIFI_AP);
        WiFi.softAP(fullApName.c_str());
        dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

        Serial.println("\n--- CAPTIVE PORTAL ACTIVE ---");
        Serial.print("Device SSID: "); Serial.println(fullApName);
        Serial.print("Portal IP:   "); Serial.println(WiFi.softAPIP());
        Serial.println("-----------------------------");

        server.on("/", [this](){ handleRoot(); });
        
        server.on("/save", [this]() {
            prefs.begin("wifi-creds", false);
            prefs.putString("ssid", server.arg("ssid"));
            prefs.putString("pass", server.arg("pass"));
            prefs.end();
            server.send(200, "text/html", "Settings saved! Rebooting...");
            delay(1000);
            ESP.restart();
        });

        server.on("/restart", [this]() {
            server.send(200, "text/html", "Rebooting...");
            delay(1000);
            ESP.restart();
        });

        server.on("/clear", [this]() {
            prefs.begin("wifi-creds", false);
            prefs.clear();
            prefs.end();
            server.send(200, "text/html", "WiFi Cleared. Rebooting...");
            delay(1000);
            ESP.restart();
        });

        auto redirect = [this]() { server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); };
        server.on("/generate_204", redirect);
        server.on("/fwlink", redirect);
        server.on("/hotspot-detect.html", redirect);
        server.onNotFound(redirect);

        server.begin();
        while (true) {
            dnsServer.processNextRequest();
            server.handleClient();
            yield();
        }
    }

    bool tryConnect() {
        prefs.begin("wifi-creds", true);
        String ssid = prefs.getString("ssid", "");
        String pass = prefs.getString("pass", "");
        prefs.end();

        if (ssid == "") return false;

        Serial.print("\n[+] Attempting to connect to: ");
        Serial.println(ssid);

        WiFi.begin(ssid.c_str(), pass.c_str());
        int c = 0;
        while (WiFi.status() != WL_CONNECTED && c < 20) {
            delay(500);
            Serial.print(".");
            c++;
        }
        Serial.println("");
        return (WiFi.status() == WL_CONNECTED);
    }
};