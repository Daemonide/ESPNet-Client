# ESPNet Laser Tag Node (ESP32 Firmware)

ESP32 firmware for wireless laser tag devices. Connects to ESPNet server via UDP and handles laser sensor input, manual tag buttons, and WiFi configuration through a captive portal.

For the server use [ESPNet Server](https://github.com/Daemonide/ESPNet-Server)

For the frontend use [ESPNet Server Frontend](https://github.com/Daemonide/ESPNet-Server-Frontend)

## Pin Configuration

```cpp
const int LASER_PIN = 15;   // Laser sensor input (active LOW)
const int BUTTON_PIN = 13;  // Manual tag button (active LOW)
const int BOOT_PIN = 0;     // Built-in BOOT button for WiFi reset
```

All pins use internal pull-ups, so sensors/buttons should pull to ground when triggered.

## Features

### WiFi Management
- **Captive Portal** - First boot or after reset shows WiFi config page
- **Credential Storage** - Saves WiFi SSID/password to ESP32 flash memory
- **Auto-reconnect** - Connects to saved network on subsequent boots
- **BOOT Button Reset** - Hold BOOT for 2 seconds at startup to clear WiFi

### Network Communication
- **UDP Discovery** - Broadcasts to find ESPNet server on local network
- **Heartbeat** - Sends status every 4 seconds to keep connection alive
- **Tag Events** - Reports laser hits to server immediately
- **Remote Commands** - Server can restart ESP32 or trigger WiFi reset

### Game Logic
- Laser sensor triggers tag event (300ms debounce)
- Manual button for testing/backup tagging
- LED/serial output for debugging

## Setup

### Software Requirements

- [PlatformIO](https://platformio.org/) (VSCode extension recommended)
- ESP32 board definitions installed

### Flashing Firmware

1. Connect ESP32 via USB
2. Open project in PlatformIO
3. Build and upload:

```bash
pio run --target upload
```

Or use PlatformIO IDE upload button in VSCode.


## First Boot Setup

1. Power on ESP32
2. Look for WiFi network: `ESP-LaserTag-XXXXXX` (last 6 MAC digits)
3. Connect to it from phone/laptop
4. Captive portal opens automatically (or go to 192.168.4.1)
5. Select your WiFi network and enter password
6. ESP32 saves credentials and reboots
7. Connects to your network and finds server

## WiFi Reset Procedure

If you need to reconfigure WiFi:

**Method 1: Boot Button**
1. Power off ESP32
2. Hold BOOT button
3. Power on while holding
4. Release after 2 seconds
5. Captive portal starts

**Method 2: Server Command**
- Use ESPNet dashboard "WiFi Reset" button
- ESP32 clears credentials and reboots to portal

## Network Protocol

ESP32 uses UDP for all server communication:

### Outgoing Messages (Port 8888)

```
DISCOVER_SERVER              # Broadcast to find server
HEARTBEAT|<MAC>|<IP>         # Every 4 seconds
EVENT|TAG|<MAC>              # When laser sensor triggers
```

### Incoming Messages (Port 8889)

```
ESPNet-Server-Online         # Server discovery response
HEARTBEAT_ACK                # Heartbeat confirmation
RESTART                      # Reboot command
WIFI_RESET                   # Clear WiFi and reboot
```

## File Structure

```
src/
└── main.cpp           # Main program loop

include/
├── CaptiveWifi.h      # WiFi manager and captive portal
└── UDPNet.h           # UDP networking and server discovery

platformio.ini         # PlatformIO configuration
```

## Code Overview

### CaptiveWifi.h
- Manages WiFi credentials using ESP32 Preferences
- Starts captive portal with web interface for WiFi config
- Scans and displays available networks
- Handles credential storage/retrieval

### UDPNet.h
- Server discovery via UDP broadcast
- Heartbeat management
- Tag event transmission
- Command processing from server

### main.cpp
- Boot button detection (2 second window)
- WiFi connection logic
- Laser sensor and button handling
- Status reporting to serial monitor

## Configuration

### Change UDP Ports

In `UDPNet.h`:

```cpp
const int localPort = 8889;   // ESP32 listening port
const int serverPort = 8888;  // Server listening port
```

### Adjust Debounce Time

In `main.cpp`:

```cpp
if (digitalRead(LASER_PIN) == LOW && (millis() - lastLaserTrigger > 300)) {
    // Change 300 to desired milliseconds
```

### Heartbeat Interval

In `UDPNet.h`:

```cpp
if (serverFound && (millis() - lastHeartbeatSent > 4000)) {
    // Change 4000 to desired milliseconds
```


## Serial Debug Output

```
+======================================+
|     ESPNet LASER TAG NODE v3.1       |
+======================================+

[BOOT] Press BOOT button within 2 seconds to reset WiFi...
[BOOT] ..........
[BOOT] No button press detected, continuing normal boot...
[WIFI] Connecting to: Sachi Pixel
............
[WIFI] Connection successful!
[WIFI] [OK] Connected to saved network!
==========================================
[WIFI] SSID: Sachi Pixel
[WIFI] IP: 10.150.182.162
[WIFI] MAC: B0:CB:D8:D7:20:58
[WIFI] RSSI: -48 dBm
==========================================
[UDP] Initializing network...
[UDP] Listening on port 8889
[SYSTEM] UDP Networking Ready
[SYSTEM] Searching for server...
==========================================

[UDP] WiFi connected, IP: 10.150.182.162
[UDP] Broadcasting DISCOVER_SERVER...
[STATUS] Uptime: 1s | WiFi: -46dBm | Server: Searching
[UDP] Broadcasting DISCOVER_SERVER...
[UDP] Broadcasting DISCOVER_SERVER...
```
