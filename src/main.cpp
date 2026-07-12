// ============================================================================
// ESP32-2432S028R (Cheap Yellow Display) WLAN-Lokregler
// ----------------------------------------------------------------------------
// Vereint:
//   * TFT_eSPI UI (übernommen aus WLANHandregler/src/main.cpp)
//   * XPT2046 Touchscreen-Steuerung (Gas / Vor / Rueck / STOP / Licht /
//     Funktionen F1..F16 / Adresswahler / Throttle-Slide)
//   * WiFi + WebSocket gegen rmx-sx-wlan-gateway
//     (Protokoll: docs/websocket-protocol.md, protocol.py)
//
// Build-Flags (platformio.ini):
//   GW_HOST, GW_PORT, WIFI_SSID, WIFI_PASSWORD, CLIENT_ID,
//   GW_INTERFACE, GW_BUS, GW_ADDRESS
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>

// Vorwaerts-Deklarationen (Event-Handler wird vor den Zeichen-Funktionen genutzt)
void drawStatusBar();
void updateDynamicParts();

// ---- Gateway-Verbindung (aus build_flags) --------------------------------
#ifndef GW_HOST
#define GW_HOST "192.168.50.1"
#endif
#ifndef GW_PORT
#define GW_PORT 8080
#endif
#ifndef WIFI_SSID
#define WIFI_SSID "Modellbahn-Fahrregler"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "Modellbahn2026"
#endif
#ifndef CLIENT_ID
#define CLIENT_ID "fahrregler-01"
#endif
#ifndef GW_INTERFACE
#define GW_INTERFACE "sim1"
#endif
#ifndef GW_BUS
#define GW_BUS "RMX0"
#endif
#ifndef GW_ADDRESS
#define GW_ADDRESS 110
#endif

// ---- TFT / Touch / SPI ---------------------------------------------------
TFT_eSPI tft = TFT_eSPI();

// Firmware-Version (auf Display oben links + bei jedem Flash genannt)
#define FW_VERSION "v0.1.12"

// Touch-Debug (letzte gemessene Roh/Mapped-Werte, unten angezeigt)
int16_t dbgRawX = 0, dbgRawY = 0;
int16_t dbgMapX = 0, dbgMapY = 0;
bool    dbgTouched = false;

// XPT2046 hat EIGENE Pins (laut CYD-Pinout-Doku + PacoMouseCYD Referenz):
//   CLK=25, MOSI=32, MISO=39, CS=33, IRQ=36
// Display (TFT_eSPI) nutzt VSPI 14/12/13 - wir nutzen BIT-BANG auf den
// Touch-Pins (bewaehrt fuer CYD 2.8" laut PacoMouseCYD).
#define TOUCH_CLK 25
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CS 33
#define TOUCH_IRQ 36

// --- Bit-Bang XPT2046 (kopiert aus PacoMouseCYD, bewaehrt fuer CYD 2.8") ---
uint16_t _xraw = 0, _yraw = 0, _zraw = 0;
uint32_t _msraw = 0;

uint16_t touchReadSPI(byte command) {
    uint16_t result = 0;
    for (int i = 7; i >= 0; i--) {
        digitalWrite(TOUCH_MOSI, command & (1 << i));
        digitalWrite(TOUCH_CLK, HIGH);
        delayMicroseconds(7);
        digitalWrite(TOUCH_CLK, LOW);
        delayMicroseconds(7);
    }
    for (int i = 11; i >= 0; i--) {
        digitalWrite(TOUCH_CLK, HIGH);
        delayMicroseconds(7);
        digitalWrite(TOUCH_CLK, LOW);
        delayMicroseconds(7);
        result |= (digitalRead(TOUCH_MISO) << i);
    }
    return result;
}

void touchUpdate() {
    uint32_t now = millis();
    if (now - _msraw < 4) return;   // MSEC_THRESHOLD=4
    digitalWrite(TOUCH_CS, LOW);
    touchReadSPI(0xB0); touchReadSPI(0xB0); touchReadSPI(0xB0);
    int z1 = touchReadSPI(0xB0);
    _zraw = z1 + 4095;
    touchReadSPI(0xC0); touchReadSPI(0xC0); touchReadSPI(0xC0);
    int z2 = touchReadSPI(0xC0);
    _zraw -= z2;
    touchReadSPI(0x90); touchReadSPI(0x90); touchReadSPI(0x90);
    _xraw = touchReadSPI(0x90);
    touchReadSPI(0xD0); touchReadSPI(0xD0); touchReadSPI(0xD0);
    _yraw = touchReadSPI(0xD0);
    digitalWrite(TOUCH_CS, HIGH);
    _msraw = now;
    // Rotation 0: x,y tauschen + y invertieren (wie PacoMouseCYD)
    int t = 4095 - _yraw;
    _yraw = _xraw;
    _xraw = t;
}

bool touchPressed() {
    touchUpdate();
    return (_zraw > 300);   // Z_THRESHOLD=300
}
void touchReadRaw(uint16_t *x, uint16_t *y, uint16_t *z) {
    touchUpdate();
    *x = _xraw; *y = _yraw; *z = _zraw;
}

// Grobe Touch-Kalibrierung fuer CYD (240x320, Rotation 0).
// Bei abweichendem Panel die rohen Grenzen anpassen.
#define TS_MIN_X 200
#define TS_MAX_X 3700
#define TS_MIN_Y 240
#define TS_MAX_Y 3900

// ============================================================================
// Grundtypen
// ============================================================================
struct Rect
{
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
};

struct FunctionConfig
{
    uint8_t functionNumber;
    bool active;
};

// ============================================================================
// Farben
// ============================================================================
constexpr uint16_t COLOR_BACKGROUND  = TFT_DARKGREY;
constexpr uint16_t COLOR_PANEL       = 0x4208;
constexpr uint16_t COLOR_BUTTON      = 0xBDF7;
constexpr uint16_t COLOR_BLUE_LED    = 0x05FF;
constexpr uint16_t COLOR_INACTIVE    = 0x8410;

// ============================================================================
// Layout
// ============================================================================
namespace Layout
{
    constexpr int16_t screenWidth  = 240;
    constexpr int16_t screenHeight = 320;

    constexpr int16_t margin = 4;
    constexpr int16_t gap    = 4;

    constexpr Rect locomotive = { 4, 4, 232, 38 };
    constexpr Rect locomotiveName = { 4, 46, 202, 30 };
    constexpr Rect locomotiveDropDown = { 209, 46, 27, 30 };

    constexpr int16_t sideButtonW = 64;
    constexpr int16_t sideButtonH = 32;

    constexpr int16_t leftColumnX  = 4;
    constexpr int16_t rightColumnX = 172;

    constexpr Rect lightButton = { leftColumnX, 80, sideButtonW, sideButtonH };
    constexpr Rect addressSelector = { rightColumnX, 80, sideButtonW, sideButtonH };
    constexpr Rect speedDisplay = { 72, 80, 96, 32 };

    constexpr int16_t functionStartY  = 120;
    constexpr int16_t functionButtonW = sideButtonW;
    constexpr int16_t functionButtonH = sideButtonH;
    constexpr int16_t functionGap     = 4;

    constexpr int16_t functionLeftX  = leftColumnX;
    constexpr int16_t functionRightX = rightColumnX;

    constexpr Rect throttle  = { 76, 120, 38, 140 };
    constexpr Rect speedGauge = { 126, 120, 38, 140 };

    constexpr Rect accelerateButton = { 4, 268, 55, 48 };
    constexpr Rect reverseButton    = { 63, 268, 55, 48 };
    constexpr Rect forwardButton    = { 122, 268, 55, 48 };
    constexpr Rect emergencyButton  = { 181, 268, 55, 48 };
}

// ============================================================================
// Funktionskonfiguration
// ============================================================================
FunctionConfig allFunctions[16] =
{
    {1,  false}, {2,  false}, {3,  false}, {4,  false},
    {5,  false}, {6,  false}, {7,  false}, {8,  false},
    {9,  false}, {10, false}, {11, false}, {12, false},
    {13, false}, {14, false}, {15, false}, {16, false}
};

uint8_t visibleFunctions[8] = { 1, 2, 3, 4, 9, 10, 11, 12 };

// ============================================================================
// Fahrregler-Zustand
// ============================================================================
int currentSpeed = 0;
int targetSpeed  = 0;
int gwAddress    = GW_ADDRESS;
bool lightOn      = true;
bool online       = false;   // WebSocket verbunden?
String lastDir    = "forward";

// ============================================================================
// WebSocket
// ============================================================================
WebSocketsClient webSocket;
uint32_t lastPing = 0;
uint32_t seq = 0;

void sendHello()
{
    JsonDocument doc;
    doc["type"] = "hello";
    doc["client_id"] = CLIENT_ID;
    doc["protocol_version"] = 1;
    doc["device"] = "ESP32-2432S028R";
    doc["firmware_version"] = "0.2.0";
    String out;
    serializeJson(doc, out);
    webSocket.sendTXT(out);
    Serial.println("[TX] hello");
}

void sendDrive(int address, int speed, const char* dir)
{
    JsonDocument doc;
    doc["type"] = "drive";
    doc["interface"] = GW_INTERFACE;
    doc["bus"] = GW_BUS;
    doc["address"] = address;
    doc["speed"] = speed;
    doc["direction"] = dir;
    doc["sequence"] = ++seq;
    String out;
    serializeJson(doc, out);
    webSocket.sendTXT(out);
    Serial.printf("[TX] drive addr=%d speed=%d dir=%s seq=%u\n", address, speed, dir, seq);
}

void sendFunction(int address, int fn, bool state)
{
    JsonDocument doc;
    doc["type"] = "function";
    doc["interface"] = GW_INTERFACE;
    doc["bus"] = GW_BUS;
    doc["address"] = address;
    doc["function"] = fn;
    doc["state"] = state;
    doc["sequence"] = ++seq;
    String out;
    serializeJson(doc, out);
    webSocket.sendTXT(out);
    Serial.printf("[TX] function fn=%d state=%d seq=%u\n", fn, state, seq);
}

void sendEmergencyStop()
{
    JsonDocument doc;
    doc["type"] = "emergency_stop";
    doc["interface"] = GW_INTERFACE;
    doc["bus"] = GW_BUS;
    doc["sequence"] = ++seq;
    String out;
    serializeJson(doc, out);
    webSocket.sendTXT(out);
    Serial.println("[TX] emergency_stop");
}

void sendSelectLoco(int address)
{
    JsonDocument doc;
    doc["type"] = "select_loco";
    doc["interface"] = GW_INTERFACE;
    doc["bus"] = GW_BUS;
    doc["address"] = address;
    String out;
    serializeJson(doc, out);
    webSocket.sendTXT(out);
    Serial.printf("[TX] select_loco addr=%d\n", address);
}

void sendRequestState(int address)
{
    JsonDocument doc;
    doc["type"] = "request_state";
    doc["interface"] = GW_INTERFACE;
    doc["bus"] = GW_BUS;
    doc["address"] = address;
    String out;
    serializeJson(doc, out);
    webSocket.sendTXT(out);
}

void sendPing()
{
    // Echter WebSocket-Ping-Frame (opcode 0x9) statt JSON-Text,
    // damit aiohttp den Heartbeat erkennt und die Verbindung nicht
    // nach Inaktivitaet schliesst.
    webSocket.sendPing();
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length)
{
    switch (type)
    {
        case WStype_CONNECTED:
            Serial.println("[WS] connected");
            online = true;
            sendHello();
            sendSelectLoco(gwAddress);
            sendRequestState(gwAddress);
            break;
        case WStype_DISCONNECTED:
            Serial.println("[WS] disconnected");
            online = false;
            break;
        case WStype_TEXT:
        {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, payload, length);
            if (err)
            {
                Serial.printf("[WS] JSON parse error: %s\n", err.c_str());
                break;
            }
            const char* t = doc["type"];
            if (strcmp(t, "loco_status") == 0 || strcmp(t, "state_snapshot") == 0)
            {
                int addr = doc["address"] | 0;
                int spd  = doc["speed"] | 0;
                const char* dir = doc["direction"] | "forward";
                if (addr == gwAddress)
                {
                    currentSpeed = spd;
                    if (targetSpeed != spd) targetSpeed = spd;
                    lastDir = dir;
                    JsonObject fns = doc["functions"];
                    if (!fns.isNull())
                    {
                        for (int i = 0; i < 16; i++)
                        {
                            if (fns[String(i + 1)].is<bool>())
                                allFunctions[i].active = fns[String(i + 1)] | false;
                        }
                    }
                    if (strcmp(t, "loco_status") == 0) updateDynamicParts();
                }
            }
            else if (strcmp(t, "hello_ack") == 0)
            {
                Serial.println("[RX] hello_ack -> ready");
                drawStatusBar();
            }
            else if (strcmp(t, "command_ack") == 0)
            {
                // stille Bestaetigung
            }
            break;
        }
        default:
            break;
    }
}

void connectWiFi()
{
    Serial.printf("Connecting to %s ...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20)  // max 10s, dann weitermachen
    {
        delay(500);
        tries++;
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED)
        Serial.printf("\nWiFi connected, IP=%s\n", WiFi.localIP().toString().c_str());
    else
        Serial.println("\nWiFi NOT connected (weiter ohne WLAN)");
}

// ============================================================================
// Zeichen-Hilfsfunktionen (aus WLANHandregler uebernommen)
// ============================================================================
void drawBeveledButton(int16_t x, int16_t y, int16_t w, int16_t h,
                       uint16_t fillColor, bool active = false)
{
    const uint16_t borderColor = active ? TFT_YELLOW : TFT_WHITE;
    tft.fillRoundRect(x, y, w, h, 3, fillColor);
    tft.drawRoundRect(x, y, w, h, 3, borderColor);
    tft.drawFastHLine(x + 3, y + 2, w - 6, TFT_WHITE);
    tft.drawFastVLine(x + 2, y + 3, h - 6, TFT_WHITE);
    tft.drawFastHLine(x + 3, y + h - 3, w - 6, TFT_DARKGREY);
    tft.drawFastVLine(x + w - 3, y + 3, h - 6, TFT_DARKGREY);
}

void drawCenteredText(const String& text, int16_t centerX, int16_t centerY,
                      uint16_t color, uint8_t font = 2,
                      uint16_t background = TFT_TRANSPARENT)
{
    tft.setTextDatum(MC_DATUM);
    if (background == TFT_TRANSPARENT) tft.setTextColor(color);
    else tft.setTextColor(color, background);
    tft.drawString(text, centerX, centerY, font);
}

void drawLeftText(const String& text, int16_t x, int16_t centerY,
                  uint16_t color, uint8_t font = 2,
                  uint16_t background = TFT_TRANSPARENT)
{
    tft.setTextDatum(ML_DATUM);
    if (background == TFT_TRANSPARENT) tft.setTextColor(color);
    else tft.setTextColor(color, background);
    tft.drawString(text, x, centerY, font);
}

void drawArrowTriangle(int16_t centerX, int16_t centerY, int16_t size,
                       bool right, uint16_t color)
{
    if (right)
        tft.fillTriangle(centerX - size, centerY - size, centerX - size, centerY + size, centerX + size, centerY, color);
    else
        tft.fillTriangle(centerX + size, centerY - size, centerX + size, centerY + size, centerX - size, centerY, color);
}

void drawLightIcon(int16_t x, int16_t y, bool active)
{
    const uint16_t color = active ? TFT_YELLOW : COLOR_INACTIVE;
    tft.fillCircle(x, y, 5, color);
    tft.drawCircle(x, y, 6, TFT_BLACK);
    tft.drawLine(x, y - 10, x, y - 7, color);
    tft.drawLine(x, y + 7, x, y + 10, color);
    tft.drawLine(x - 10, y, x - 7, y, color);
    tft.drawLine(x + 7, y, x + 10, y, color);
}

// ============================================================================
// Statusleiste (oben, Verbindungsstatus) -- NEU
// ============================================================================
void drawStatusBar()
{
    // kleiner Punkt oben rechts im Lok-Panel
    const Rect& r = Layout::locomotive;
    tft.fillCircle(r.x + r.w - 12, r.y + 19, 5, online ? TFT_GREEN : TFT_RED);
}

// ============================================================================
// Lokdarstellung
// ============================================================================
void drawLocomotive()
{
    const Rect& r = Layout::locomotive;
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, COLOR_PANEL);
    tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_WHITE);

    const int16_t x = r.x + 80;
    const int16_t y = r.y + 10;

    tft.fillRect(x, y, 72, 13, TFT_YELLOW);
    tft.fillRect(x + 4, y + 3, 13, 6, TFT_BLUE);
    tft.fillRect(x + 21, y + 3, 13, 6, TFT_BLUE);
    tft.fillRect(x + 38, y + 3, 13, 6, TFT_BLUE);
    tft.fillRect(x + 55, y + 3, 13, 6, TFT_BLUE);
    tft.fillRect(x, y + 13, 72, 5, TFT_BLUE);
    tft.fillCircle(x + 13, y + 19, 4, TFT_BLACK);
    tft.fillCircle(x + 59, y + 19, 4, TFT_BLACK);
    tft.drawLine(x + 28, y, x + 36, y - 7, TFT_RED);
    tft.drawLine(x + 36, y - 7, x + 44, y, TFT_RED);
    tft.drawFastHLine(x + 30, y - 8, 18, TFT_RED);
}

void drawLocName()
{
    const Rect& nameRect = Layout::locomotiveName;
    const Rect& dropRect = Layout::locomotiveDropDown;

    drawBeveledButton(nameRect.x, nameRect.y, nameRect.w, nameRect.h, TFT_WHITE);
    drawLeftText("BR 110 238-3", nameRect.x + 7, nameRect.y + nameRect.h / 2,
                 TFT_BLACK, 4, TFT_WHITE);

    drawBeveledButton(dropRect.x, dropRect.y, dropRect.w, dropRect.h, COLOR_BUTTON);
    tft.fillTriangle(dropRect.x + 7, dropRect.y + 11, dropRect.x + 20, dropRect.y + 11,
                     dropRect.x + 13, dropRect.y + 20, TFT_BLACK);
}

void drawLightButton()
{
    const Rect& r = Layout::lightButton;
    const uint16_t buttonColor = lightOn ? TFT_YELLOW : COLOR_BUTTON;
    drawBeveledButton(r.x, r.y, r.w, r.h, buttonColor, lightOn);
    drawLightIcon(r.x + 17, r.y + r.h / 2, lightOn);
    drawLeftText("Licht", r.x + 31, r.y + r.h / 2, TFT_BLACK, 2, buttonColor);
}

void drawAddressSelector()
{
    const Rect& r = Layout::addressSelector;
    drawBeveledButton(r.x, r.y, r.w, r.h, TFT_WHITE);
    drawCenteredText(String(gwAddress), r.x + 24, r.y + r.h / 2, TFT_BLACK, 2, TFT_WHITE);

    const int16_t separatorX = r.x + r.w - 18;
    tft.drawFastVLine(separatorX, r.y + 2, r.h - 4, TFT_DARKGREY);
    tft.fillTriangle(separatorX + 9, r.y + 5, separatorX + 4, r.y + 12, separatorX + 14, r.y + 12, TFT_BLACK);
    tft.fillTriangle(separatorX + 9, r.y + r.h - 5, separatorX + 4, r.y + r.h - 12, separatorX + 14, r.y + r.h - 12, TFT_BLACK);
}

void drawDigitalDisplay()
{
    const Rect& r = Layout::speedDisplay;
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_NAVY);
    tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
    char speedBuffer[4];
    snprintf(speedBuffer, sizeof(speedBuffer), "%03d", currentSpeed);
    drawCenteredText(speedBuffer, r.x + r.w / 2, r.y + r.h / 2, COLOR_BLUE_LED, 4, TFT_NAVY);
}

void drawFunctionButton(int16_t x, int16_t y, int16_t width, int16_t height,
                        FunctionConfig& function)
{
    const uint16_t buttonColor = function.active ? TFT_YELLOW : COLOR_BUTTON;
    drawBeveledButton(x, y, width, height, buttonColor, function.active);
    drawCenteredText("F" + String(function.functionNumber), x + width / 2, y + height / 2,
                     TFT_BLACK, 4, buttonColor);
}

void drawFunctionColumns()
{
    for (int row = 0; row < 4; row++)
    {
        const int16_t y = Layout::functionStartY + row * (Layout::functionButtonH + Layout::functionGap);
        const uint8_t leftFunctionNumber = visibleFunctions[row];
        if (leftFunctionNumber >= 1 && leftFunctionNumber <= 16)
            drawFunctionButton(Layout::functionLeftX, y, Layout::functionButtonW,
                               Layout::functionButtonH, allFunctions[leftFunctionNumber - 1]);
        const uint8_t rightFunctionNumber = visibleFunctions[row + 4];
        if (rightFunctionNumber >= 1 && rightFunctionNumber <= 16)
            drawFunctionButton(Layout::functionRightX, y, Layout::functionButtonW,
                               Layout::functionButtonH, allFunctions[rightFunctionNumber - 1]);
    }
}

void drawThrottle()
{
    const Rect& slider = Layout::throttle;
    const Rect& gauge  = Layout::speedGauge;

    tft.fillRoundRect(slider.x, slider.y, slider.w, slider.h, 5, TFT_BLACK);
    tft.drawRoundRect(slider.x, slider.y, slider.w, slider.h, 5, TFT_WHITE);
    tft.drawFastVLine(slider.x + slider.w / 2, slider.y + 8, slider.h - 16, TFT_LIGHTGREY);
    for (int i = 0; i <= 10; i++)
    {
        const int16_t markY = slider.y + 8 + i * ((slider.h - 16) / 10);
        tft.drawFastHLine(slider.x + 6, markY, 6, TFT_DARKGREY);
        tft.drawFastHLine(slider.x + slider.w - 12, markY, 6, TFT_DARKGREY);
    }

    const int16_t sliderPosition = map(targetSpeed, 0, 99, slider.y + slider.h - 15, slider.y + 15);
    tft.fillRoundRect(slider.x + 3, sliderPosition - 9, slider.w - 6, 19, 4, TFT_LIGHTGREY);
    tft.drawRoundRect(slider.x + 3, sliderPosition - 9, slider.w - 6, 19, 4, TFT_WHITE);
    tft.fillRect(slider.x + 7, sliderPosition - 2, slider.w - 14, 5, TFT_YELLOW);

    tft.fillRoundRect(gauge.x, gauge.y, gauge.w, gauge.h, 5, TFT_BLACK);
    tft.drawRoundRect(gauge.x, gauge.y, gauge.w, gauge.h, 5, TFT_WHITE);
    const int16_t innerTop = gauge.y + 6;
    const int16_t innerBottom = gauge.y + gauge.h - 6;
    const int16_t innerHeight = innerBottom - innerTop;
    const int16_t fillHeight = map(currentSpeed, 0, 99, 0, innerHeight);
    if (fillHeight > 0)
        tft.fillRect(gauge.x + 6, innerBottom - fillHeight, gauge.w - 12, fillHeight, TFT_YELLOW);
    for (int i = 1; i < 10; i++)
    {
        const int16_t segmentY = innerTop + i * innerHeight / 10;
        tft.drawFastHLine(gauge.x + 6, segmentY, gauge.w - 12, TFT_ORANGE);
    }
}

void drawBottomControls()
{
    const Rect& gasRect = Layout::accelerateButton;
    const Rect& backRect = Layout::reverseButton;
    const Rect& frontRect = Layout::forwardButton;
    const Rect& stopRect = Layout::emergencyButton;

    drawBeveledButton(gasRect.x, gasRect.y, gasRect.w, gasRect.h, COLOR_BUTTON);
    tft.fillTriangle(gasRect.x + gasRect.w / 2, gasRect.y + 6, gasRect.x + 15, gasRect.y + 27,
                     gasRect.x + gasRect.w - 15, gasRect.y + 27, TFT_GREEN);
    drawCenteredText("Gas", gasRect.x + gasRect.w / 2, gasRect.y + 39, TFT_BLACK, 1, COLOR_BUTTON);

    drawBeveledButton(backRect.x, backRect.y, backRect.w, backRect.h, COLOR_BUTTON);
    drawArrowTriangle(backRect.x + backRect.w / 2, backRect.y + 20, 10, false, TFT_WHITE);
    drawCenteredText("Rueck", backRect.x + backRect.w / 2, backRect.y + 39, TFT_BLACK, 1, COLOR_BUTTON);

    drawBeveledButton(frontRect.x, frontRect.y, frontRect.w, frontRect.h, COLOR_BUTTON);
    drawArrowTriangle(frontRect.x + frontRect.w / 2, frontRect.y + 20, 10, true, TFT_YELLOW);
    drawCenteredText("Vor", frontRect.x + frontRect.w / 2, frontRect.y + 39, TFT_BLACK, 1, COLOR_BUTTON);

    drawBeveledButton(stopRect.x, stopRect.y, stopRect.w, stopRect.h, COLOR_BUTTON);
    tft.fillTriangle(stopRect.x + stopRect.w / 2, stopRect.y + 29, stopRect.x + 15, stopRect.y + 7,
                     stopRect.x + stopRect.w - 15, stopRect.y + 7, TFT_RED);
    drawCenteredText("STOP", stopRect.x + stopRect.w / 2, stopRect.y + 39, TFT_BLACK, 1, COLOR_BUTTON);
}

void drawCompleteScreen()
{
    tft.fillScreen(COLOR_BACKGROUND);
    drawLocomotive();
    drawLocName();
    drawLightButton();
    drawAddressSelector();
    drawDigitalDisplay();
    drawFunctionColumns();
    drawThrottle();
    drawBottomControls();
    drawStatusBar();

    // Firmware-Version oben links
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND);
    tft.drawString(FW_VERSION, 4, 4, 1);
}

void drawDebugTouch()
{
    // Debug-Leiste unten: letzte Roh/Mapped-Touch-Werte
    const int16_t y = 305;
    tft.fillRect(0, y, 240, 12, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    char buf[40];
    snprintf(buf, sizeof(buf), "T:%s RX%d RY%d MX%d MY%d",
             dbgTouched ? "Y" : "n", dbgRawX, dbgRawY, dbgMapX, dbgMapY);
    tft.drawString(buf, 2, y + 2, 1);
}

void drawLoopCounter(uint32_t n)
{
    char lc[12];
    snprintf(lc, sizeof(lc), "L%d", (int)n);
    tft.fillRect(180, 0, 60, 12, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(lc, 182, 2, 1);
}

void updateDynamicParts()
{
    drawDebugTouch();   // zuerst, damit die Leiste IMMER sichtbar ist

    drawDigitalDisplay();

    const int16_t clearX = Layout::throttle.x - 3;
    const int16_t clearY = Layout::throttle.y - 3;
    const int16_t clearRight = Layout::speedGauge.x + Layout::speedGauge.w + 3;
    const int16_t clearBottom = Layout::throttle.y + Layout::throttle.h + 3;
    tft.fillRect(clearX, clearY, clearRight - clearX, clearBottom - clearY, COLOR_BACKGROUND);
    drawThrottle();
}

// ============================================================================
// Touch-Hilfen
// ============================================================================
bool pointInRect(int16_t x, int16_t y, const Rect& r)
{
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

void applyTouch(int16_t tx, int16_t ty)
{
    // Lok-Dropdown
    if (pointInRect(tx, ty, Layout::locomotiveDropDown))
    {
        // (Demo) naechste sichtbare Lok-Adresse -- hier einfach Toggle 110<->42
        gwAddress = (gwAddress == 110) ? 42 : 110;
        drawAddressSelector();
        drawLocName();
        sendSelectLoco(gwAddress);
        sendRequestState(gwAddress);
        return;
    }
    // Licht
    if (pointInRect(tx, ty, Layout::lightButton))
    {
        lightOn = !lightOn;
        drawLightButton();
        sendFunction(gwAddress, 0, lightOn);   // F0 = Licht
        return;
    }
    // Adress-Pfeile
    if (pointInRect(tx, ty, Layout::addressSelector))
    {
        const Rect& r = Layout::addressSelector;
        const int16_t separatorX = r.x + r.w - 18;
        if (tx >= separatorX)
        {
            if (ty < r.y + r.h / 2) gwAddress = min(255, gwAddress + 1);
            else gwAddress = max(1, gwAddress - 1);
            drawAddressSelector();
            sendSelectLoco(gwAddress);
            sendRequestState(gwAddress);
        }
        return;
    }
    // Funktionsspalten
    for (int row = 0; row < 4; row++)
    {
        const int16_t y = Layout::functionStartY + row * (Layout::functionButtonH + Layout::functionGap);
        const uint8_t lf = visibleFunctions[row];
        if (lf >= 1 && lf <= 16 && pointInRect(tx, ty, {Layout::functionLeftX, y, Layout::functionButtonW, Layout::functionButtonH}))
        {
            allFunctions[lf - 1].active = !allFunctions[lf - 1].active;
            drawFunctionButton(Layout::functionLeftX, y, Layout::functionButtonW, Layout::functionButtonH, allFunctions[lf - 1]);
            sendFunction(gwAddress, lf, allFunctions[lf - 1].active);
            return;
        }
        const uint8_t rf = visibleFunctions[row + 4];
        if (rf >= 1 && rf <= 16 && pointInRect(tx, ty, {Layout::functionRightX, y, Layout::functionButtonW, Layout::functionButtonH}))
        {
            allFunctions[rf - 1].active = !allFunctions[rf - 1].active;
            drawFunctionButton(Layout::functionRightX, y, Layout::functionButtonW, Layout::functionButtonH, allFunctions[rf - 1]);
            sendFunction(gwAddress, rf, allFunctions[rf - 1].active);
            return;
        }
    }
    // Untere Steuerbuttons
    if (pointInRect(tx, ty, Layout::accelerateButton))
    {
        targetSpeed = min(99, targetSpeed + 5);
        updateDynamicParts();
        sendDrive(gwAddress, targetSpeed, lastDir.c_str());
        return;
    }
    if (pointInRect(tx, ty, Layout::forwardButton))
    {
        lastDir = "forward";
        sendDrive(gwAddress, targetSpeed, "forward");
        return;
    }
    if (pointInRect(tx, ty, Layout::reverseButton))
    {
        lastDir = "reverse";
        sendDrive(gwAddress, targetSpeed, "reverse");
        return;
    }
    if (pointInRect(tx, ty, Layout::emergencyButton))
    {
        targetSpeed = 0;
        currentSpeed = 0;
        updateDynamicParts();
        sendEmergencyStop();
        return;
    }
    // Throttle-Slide: vertikale Position im Regler -> Zielgeschwindigkeit
    const Rect& slider = Layout::throttle;
    if (tx >= slider.x - 6 && tx <= slider.x + slider.w + 6 &&
        ty >= slider.y && ty <= slider.y + slider.h)
    {
        int mapped = map(ty, slider.y + slider.h - 15, slider.y + 15, 0, 99);
        targetSpeed = constrain(mapped, 0, 99);
        updateDynamicParts();
        sendDrive(gwAddress, targetSpeed, lastDir.c_str());
        return;
    }
}

// ============================================================================
// Arduino Setup
// ============================================================================
void setup()
{
    Serial.begin(115200);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    tft.init();
    tft.setRotation(0);

    // Touch ueber Bit-Bang auf korrekten CYD-Pins (25/32/39/33/36)
    pinMode(TOUCH_CLK, OUTPUT);
    digitalWrite(TOUCH_CLK, LOW);
    pinMode(TOUCH_MOSI, OUTPUT);
    pinMode(TOUCH_MISO, INPUT);
    pinMode(TOUCH_CS, OUTPUT);
    digitalWrite(TOUCH_CS, HIGH);
    pinMode(TOUCH_IRQ, INPUT);

    // Demo-Startzustand
    allFunctions[0].active = true;   // F1
    allFunctions[10].active = true;  // F11

    drawCompleteScreen();

    Serial.println("ESP32 Lok-Fahrregler (WLAN) gestartet");

    connectWiFi();
    webSocket.begin(GW_HOST, GW_PORT, "/ws");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(2000);
}

// ============================================================================
// Arduino Loop
// ============================================================================
void loop()
{
    webSocket.loop();

    // DIAGNOSE v0.1.4: Debug-Leiste + Loop-Zaehler (oben rechts)
    static uint32_t lastDbg = 0;
    static uint32_t loopCount = 0;
    loopCount++;
    if (millis() - lastDbg > 150)
    {
        lastDbg = millis();
        drawDebugTouch();
        drawLoopCounter(loopCount / 10);
    }

    static uint32_t lastTouchCheck = 0;
    if (millis() - lastTouchCheck > 30)
    {
        lastTouchCheck = millis();
        if (touchPressed())
        {
            uint16_t tx = 0, ty = 0, tz = 0;
            touchReadRaw(&tx, &ty, &tz);
            dbgRawX = tx; dbgRawY = ty; dbgMapX = tz;
            dbgTouched = true;
            tft.fillScreen(TFT_GREEN);
        }
        else
        {
            dbgTouched = false;
        }
    }
}
