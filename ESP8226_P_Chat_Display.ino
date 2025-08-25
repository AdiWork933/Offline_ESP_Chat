 #include <ESP8266WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

// =================== Hardware Pinout ===================
// TFT Display (ILI9341) - Portrait Mode
// VCC  -> 3.3V
// GND  -> GND
// CS   -> D8 (GPIO15)    // TFT Chip Select
// DC   -> D0 (GPIO0)     // Data/Command pin
// RST  -> D4 (GPIO2)     // Reset pin
// MOSI -> D7 (GPIO13)    // SPI MOSI
// MISO -> D6 (GPIO12)    // SPI MISO
// SCK  -> D5 (GPIO14)    // SPI Clock

// Touchscreen (XPT2046)
// T_CS   -> D2 (GPIO4)    // Touch Chip Select
// T_CLK  -> D5 (GPIO14)   // Shared SPI Clock
// T_DIN  -> D7 (GPIO13)   // Shared SPI MOSI
// T_DO   -> D6 (GPIO12)   // Shared SPI MISO
// T_IRQ  -> D0 (GPIO16)   // Optional, improves touch response

// Notes:
// - TFT and Touch share hardware SPI (MOSI, MISO, SCK)  
// - CS pins must be separate (TFT_CS != TOUCH_CS)  
// - Ensure 3.3V power for both TFT and Touch  
// - Portrait orientation is handled in code with tft.setRotation(0) and ts.setRotation(0)

// ==== TFT Pins ====
#define TFT_CS   15  // D8
#define TFT_DC   0   // D3
#define TFT_RST  2   // D4
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// ==== Touch Pins ====
#define TOUCH_CS 4   // D2
#define TOUCH_IRQ 16 // D0
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// ==== Calibration values for touch ====
#define TS_MINX 200
#define TS_MAXX 3800
#define TS_MINY 200
#define TS_MAXY 3800

// ==== WiFi & Chat ====
#define STA_SSID   "ESPChat"
#define STA_PASS   "12345678"
#define HUB_IP     IPAddress(192,168,4,1)
#define HUB_PORT   5000
WiFiClient peer;
const char* DEVICE_NAME = "ESP8266";

// ==== Keyboard ====
#define KEY_ROWS 4
#define KEY_COLS 10
const char keys[KEY_ROWS][KEY_COLS] = {
    {'Q','W','E','R','T','Y','U','I','O','P'},
    {'A','S','D','F','G','H','J','K','L',' '},
    {'Z','X','C','V','B','N','M',' ','<',' '}, 
    {' ',' ',' ',' ',' ',' ',' ',' ',' ','E'}
};

String inputBuffer = "";
int cursorY = 30;

// ==================== UI Functions ====================
void drawStatusBar(bool connected) {
    tft.fillRect(0, 0, 320, 25, connected ? ILI9341_DARKGREEN : ILI9341_RED);
    tft.setCursor(5, 8);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_WHITE);
    tft.print(connected ? "Connected" : "Connecting...");
}

void drawKeyboard() {
    int keyW = 28, keyH = 24;
    int startY = 240; // Portrait layout, keyboard at bottom
    int rowOffsets[KEY_ROWS] = {0, 0, 0, 0};

    tft.fillRect(0, startY, 320, 80, ILI9341_DARKGREY);

    for (int r = 0; r < KEY_ROWS; r++) {
        for (int c = 0; c < KEY_COLS; c++) {
            char k = keys[r][c];
            if (k == ' ') continue;

            int x = c * keyW;
            int y = startY + r * keyH;
            int w = keyW - 2;
            if (k == '<' || k == 'E') w = 50;

            tft.fillRoundRect(x, y, w, keyH - 2, 3, ILI9341_BLACK);
            tft.drawRoundRect(x, y, w, keyH - 2, 3, ILI9341_WHITE);
            tft.setCursor(x + (w / 2) - 3, y + 4);
            tft.setTextSize(2);
            tft.setTextColor(ILI9341_WHITE);
            if (k == '<') tft.print("BS");
            else if (k == 'E') tft.print("->");
            else tft.print(k);
        }
    }
}

char getKey(int x, int y) {
    int keyW = 28, keyH = 24;
    int startY = 240;

    int row = (y - startY) / keyH;
    if (row < 0 || row >= KEY_ROWS) return 0;

    int col = -1;
    if (row == 2) col = (x > 320 - 52) ? 8 : x / keyW;
    else if (row == 3) col = (x > 320 - 52) ? 9 : x / keyW;
    else col = x / keyW;

    if (col < 0 || col >= KEY_COLS) return 0;
    return keys[row][col];
}

void printChatMessage(String msg, bool isYou) {
    int bubbleColor = isYou ? ILI9341_NAVY : ILI9341_DARKGREEN;
    tft.setTextWrap(true);
    tft.setTextSize(1);
    int width = 300;
    int lines = 1 + (msg.length() * 6) / width;
    int bubbleWidth = (msg.length() * 6) + 12;
    if (bubbleWidth > width) bubbleWidth = width;
    int bubbleHeight = lines * 10 + 8;

    int x = isYou ? 320 - bubbleWidth - 5 : 5;
    int y = cursorY;
    tft.fillRect(0, y, 320, bubbleHeight + 2, ILI9341_BLACK);
    tft.fillRoundRect(x, y, bubbleWidth, bubbleHeight, 5, bubbleColor);
    tft.setCursor(x + 5, y + 4);
    tft.setTextColor(ILI9341_WHITE);
    tft.print(msg);

    cursorY += bubbleHeight + 5;
    if (cursorY > 220) { // leave room for keyboard
        tft.fillRect(0, 25, 320, 215, ILI9341_BLACK);
        cursorY = 30;
    }
}

void showTypingBuffer() {
    tft.fillRect(0, 230, 320, 30, ILI9341_WHITE);
    tft.drawRect(0, 230, 320, 30, ILI9341_BLACK);
    tft.setCursor(5, 238);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_BLACK);
    tft.print(inputBuffer);
}

// ==================== WiFi ====================
void connectWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(STA_SSID, STA_PASS);
    Serial.print("Connecting to "); Serial.print(STA_SSID);
    while (WiFi.status() != WL_CONNECTED) {
        drawStatusBar(false);
        Serial.print(".");
        delay(500);
    }
    drawStatusBar(true);
    Serial.println("\n✅ WiFi connected, IP: " + WiFi.localIP().toString());
}

void connectPeer() {
    Serial.print("Connecting to hub "); Serial.println(HUB_IP);
    while (!peer.connect(HUB_IP, HUB_PORT)) {
        drawStatusBar(false);
        Serial.print(".");
        delay(500);
    }
    peer.setNoDelay(true);
    drawStatusBar(true);
    Serial.println("\n✅ Peer connected to hub.");
}

// ==================== Setup ====================
void setup() {
    Serial.begin(115200);
    delay(200);

    tft.begin();
    tft.setRotation(0); // Portrait
    tft.fillScreen(ILI9341_BLACK);

    ts.begin();
    ts.setRotation(0); // Portrait mapping

    drawStatusBar(false);
    connectWifi();
    connectPeer();

    showTypingBuffer();
    drawKeyboard();
    Serial.println("Type a message in Serial and press Enter to send.");
}

// ==================== Loop ====================
void loop() {
    if (WiFi.status() != WL_CONNECTED) connectWifi();
    if (!peer.connected()) connectPeer();

    // ===== Touch Input =====
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        int x = map(p.x, TS_MINX, TS_MAXX, 0, 320);
        int y = map(p.y, TS_MINY, TS_MAXY, 0, 320);

        char key = getKey(x, y);
        if (key) {
            if (key == '<' && inputBuffer.length() > 0) inputBuffer.remove(inputBuffer.length() - 1);
            else if (key == 'E' && inputBuffer.length()) {
                String line = String(DEVICE_NAME) + "|" + inputBuffer + "|" + String(millis()) + "\n";
                peer.print(line);
                printChatMessage("You: " + inputBuffer, true);
                Serial.println("[You] " + inputBuffer);
                inputBuffer = "";
            } else inputBuffer += key;

            showTypingBuffer();
        }
        delay(150);
    }

    // ===== Incoming messages =====
    while (peer.connected() && peer.available()) {
        String line = peer.readStringUntil('\n');
        line.trim();
        if (line.length()) {
            printChatMessage("Peer: " + line, false);
            Serial.println("[Hub] " + line);
        }
    }

    // ===== Serial input =====
    if (Serial.available()) {
        String s = Serial.readStringUntil('\n');
        s.trim();
        if (s.length()) {
            String line = String(DEVICE_NAME) + "|" + s + "|" + String(millis()) + "\n";
            peer.print(line);
            Serial.println("[You] " + s);
            printChatMessage("You: " + s, true);
        }
    }
}
