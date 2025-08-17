/**************** ESP8266 CHAT PEER (connects to ESP32 AP, TCP client) ****************/
#include <ESP8266WiFi.h>

#define STA_SSID   "ESPChat"        // must match ESP32 AP SSID
#define STA_PASS   "12345678"
#define HUB_IP     IPAddress(192,168,4,1)
#define HUB_PORT   5000

WiFiClient peer;

const char* DEVICE_NAME = "ESP8266";

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(STA_SSID, STA_PASS);
  Serial.print("Connecting to "); Serial.print(STA_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected, IP: " + WiFi.localIP().toString());
}

void connectPeer() {
  Serial.print("Connecting to hub "); Serial.println(HUB_IP);
  while (!peer.connect(HUB_IP, HUB_PORT)) {
    delay(500); Serial.print(".");
  }
  peer.setNoDelay(true);
  Serial.println("\n✅ Peer connected to hub.");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  connectWifi();
  connectPeer();
  Serial.println("Type a message in Serial and press Enter to send.");
}

void loop() {
  // Keep WiFi alive
  if (WiFi.status() != WL_CONNECTED) connectWifi();

  // Reconnect to hub if lost
  if (!peer.connected()) connectPeer();

  // Read incoming from hub
  while (peer.connected() && peer.available()) {
    String line = peer.readStringUntil('\n'); 
    line.trim();
    if (line.length()) {
      Serial.println("[Hub] " + line);
    }
  }

  // Send user input to hub
  if (Serial.available()) {
    String s = Serial.readStringUntil('\n'); 
    s.trim();
    if (s.length()) {
      String line = String(DEVICE_NAME) + "|" + s + "|" + String(millis()) + "\n";
      peer.print(line);
      Serial.println("[You] " + s);  // echo
    }
  }
}