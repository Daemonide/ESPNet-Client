# ESPNet-Client 🛰️

`ESPNet-Client` is a powerful, ready-to-use WiFi management and networking library for the ESP32. It eliminates the need for hardcoded credentials by providing a modern Captive Portal for on-the-fly configuration. Each device identifies itself uniquely using its MAC address, making it perfect for multi-device environments.

## 📋 Table of Contents
1. [Features](#features)
2. [Folder Structure](#folder-structure)
3. [Installation & Setup](#installation--setup)
4. [Usage Guide](#usage-guide)
5. [Connecting to the Device](#connecting-to-the-device)
6. [Roadmap (TODO)](#roadmap-todo)

---

## ✨ Features
* **Zero-Config WiFi:** Captive portal for SSID and Password entry.
* **Unique Identity:** Automatically generates SSIDs like `espnet-client-A1B2C3` using the device MAC address.
* **Boot Bypass:** A 5-second window at startup to force-open the portal via a physical button (GPIO 0).
* **Persistent Storage:** Uses presistance to remember WiFi credentials through power cycles.
* **Modern UI:** Clean, responsive CSS-based web interface.
* **OS Compatibility:** Compatible with Apple, Android, and Windows.

---

## 📂 Folder Structure
This project is optimized for **PlatformIO**.

```text
espnet-client/
├── include/
│   └── CaptiveWifi.h       # The core networking & portal class
├── src/
│   └── main.cpp            # Entry point with boot-timer logic
├── platformio.ini          # Project configuration
└── README.md

```

---

## 🛠️ Installation & Setup

1. **Clone the Repo:**
```bash
git clone https://github.com/Daemonide/ESPNet-Client.git

```


2. **Open in PlatformIO:**
Open the project folder in VS Code with the PlatformIO extension installed.
3. **Hardware Wiring:**
* By default, the **BOOT** button (GPIO 0) is used to trigger the portal.
* If using a custom button, wire it between your chosen GPIO and GND.


4. **Build & Flash:**
Connect your ESP32 and click the **Upload** button in PlatformIO.

---

## 🚀 Usage Guide

### Boot Modes

* **Normal Boot:** If WiFi is saved, it connects automatically.
* **Configuration Boot:** Hold the BOOT button during the first 5 seconds of power-on to force the portal to open.
* **Fallback:** If no WiFi is saved or connection fails, the portal opens automatically.

---

## 📱 Connecting to the Device

1. Power on your ESP32.
2. On your phone/PC, scan for WiFi networks.
3. Look for a network named `espnet-client-XXXXXX` (where X is your unique ID).
4. Once connected, a "Sign in to network" window should appear automatically. If not, navigate to `http://192.168.4.1`.
5. Select your network, enter the password, and click **Connect**.

---

## 🗺️ Roadmap (TODO)

* [x] **Captive Portal for Wifi Config:** Implement a captive portal for connecting the esp to wifi
* [ ] **UDP Broadcasting:** Send a broadcast message to let the server know the ip address of esp
* [ ] **Esp Heartbeat:** Implement a heartbeat system to let the server know the esp online status
* [ ] **Data Transfer:** Implement Tcp for data transfer
---