# 🛰️ ESP Chat Hub: An Offline Communication System

This repository contains code for an **ESP32-based chat hub** and a **complementary ESP8266 chat peer**.  
The ESP32 acts as a **central access point** and **web server**, hosting a single-page chat application accessible via Wi-Fi.  
It also functions as a **peer-to-peer hub**, allowing an ESP8266 to join the chat and exchange messages with connected clients.  

---

## ✨ Key Features
- **ESP32 Access Point (AP):** Creates a local Wi-Fi network that devices (phones, laptops, microcontrollers) can join.  
- **Web-based UI:** A browser-accessible chat interface hosted directly on the ESP32.  
- **WebSockets:** Enables real-time, low-latency communication between ESP32 and all clients.  
- **Peer-to-Peer Communication:** ESP8266 connects via TCP to the ESP32 hub and participates in chat.  
- **Message History:** Stores and shares the last **50 messages** with new clients.  
- **Offline Mode:** Works without internet—ideal for **local, offline communication**.  

---

## 🛠 Hardware Requirements
- ESP32 development board (e.g., ESP32-WROOM-32)  
- ESP8266 development board (e.g., NodeMCU, Wemos D1 Mini)  
- USB cable for each board  
- Computer with **Python** installed  

---

## ⚙️ Setup Instructions

### 1️⃣ Software Prerequisites
- **Arduino IDE** → [Download here](https://www.arduino.cc/en/software)  
- **ESP32 & ESP8266 Board Packages:**  
  - Go to `File > Preferences` → add the following URLs in **Additional Board Manager URLs**:  
    ```
    https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
    https://arduino.esp8266.com/stable/package_esp8266com_index.json
    ```
  - Then go to `Tools > Board > Boards Manager...` and install **esp32** and **esp8266**.  

- **Required Libraries (install via Library Manager):**  
  - [WebSockets by Markus Sattler](https://github.com/Links2004/arduinoWebSockets)  
  - ESPAsyncWebServer *(optional dependency, good to have for compatibility)*  

---

### 2️⃣ Python Dependencies
The **custom Serial Monitor GUI** requires:  

## ⚙️ Setup Instructions

### 3️⃣ ESP32 Chat Hub Setup
1. Open the ESP32 sketch from this repository in **Arduino IDE**.  
2. Adjust Wi-Fi settings if needed:  
   ```cpp
   #define AP_SSID "ESPChat"     // Wi-Fi name
   #define AP_PASS "12345678"    // Wi-Fi password (min. 8 chars)


### 4️⃣ ESP8266 Chat Peer Setup & Usage

## 🔧 ESP8266 Setup
1. Open the ESP8266 sketch from this repository in **Arduino IDE**.  
2. Ensure Wi-Fi settings match the ESP32 hub:  
   ```cpp
   #define STA_SSID "ESPChat"
   #define STA_PASS "12345678"
   #define HUB_IP IPAddress(192,168,4,1)
   
```bash
pip install pyserial

