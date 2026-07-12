// ============================================================================
// comm.cpp — Gateway-Kommunikation (WebSocket-Client)
// ----------------------------------------------------------------------------
// Implementiert das WebSocket-Protokoll gegen den rmx-sx-wlan-gateway Daemon.
// Eingehende "loco_status"/"state_snapshot" werden ueber den onState-Callback
// in den Logic-Layer gespiegelt; Verbindungswechsel ueber onOnline. Damit
// bleibt das Netzwerk-Wissen komplett in diesem Modul.
// ============================================================================

#include "comm.h"
#include "config.h"   // FW_VERSION, GW_* Defines
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

static WebSocketsClient ws;
static uint32_t   seqCounter = 0;
static CommCallbacks gCb;

// ---- WiFi ----------------------------------------------------------------
static void connectWiFi()
{
    Serial.printf("Connecting to %s ...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20)  // max ~10s
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

// ---- JSON-Sende-Helfer ---------------------------------------------------
static void sendJson(const JsonDocument& doc, const char* tag)
{
    String out;
    serializeJson(doc, out);
    ws.sendTXT(out);
    Serial.printf("[TX] %s\n", tag);
}

static void sendHello()
{
    JsonDocument doc;
    doc["type"] = "hello";
    doc["client_id"] = CLIENT_ID;
    doc["protocol_version"] = 1;
    doc["device"] = "ESP32-2432S028R";
    doc["firmware_version"] = FW_VERSION;
    sendJson(doc, "hello");
}

static void sendDriveMsg(uint8_t address, int speed, const char* dir)
{
    JsonDocument doc;
    doc["type"] = "drive";
    doc["interface"] = GW_INTERFACE;
    doc["bus"] = GW_BUS;
    doc["address"] = address;
    doc["speed"] = speed;
    doc["direction"] = dir;
    doc["sequence"] = ++seqCounter;
    sendJson(doc, "drive");
}

static void sendFunctionMsg(uint8_t address, int fn, bool state)
{
    JsonDocument doc;
    doc["type"] = "function";
    doc["interface"] = GW_INTERFACE;
    doc["bus"] = GW_BUS;
    doc["address"] = address;
    doc["function"] = fn;
    doc["state"] = state;
    doc["sequence"] = ++seqCounter;
    sendJson(doc, "function");
}

static void sendSelectMsg(uint8_t address)
{
    JsonDocument doc;
    doc["type"] = "select_loco";
    doc["interface"] = GW_INTERFACE;
    doc["bus"] = GW_BUS;
    doc["address"] = address;
    sendJson(doc, "select_loco");
}

static void sendRequestMsg(uint8_t address)
{
    JsonDocument doc;
    doc["type"] = "request_state";
    doc["interface"] = GW_INTERFACE;
    doc["bus"] = GW_BUS;
    doc["address"] = address;
    sendJson(doc, "request_state");
}

static void sendEmergency()
{
    JsonDocument doc;
    doc["type"] = "emergency_stop";
    doc["interface"] = GW_INTERFACE;
    doc["bus"] = GW_BUS;
    doc["sequence"] = ++seqCounter;
    sendJson(doc, "emergency_stop");
}

// ---- WebSocket-Event -----------------------------------------------------
static void webSocketEvent(WStype_t type, uint8_t* payload, size_t length)
{
    switch (type)
    {
        case WStype_CONNECTED:
            Serial.println("[WS] connected");
            if (gCb.onOnline) gCb.onOnline(true);
            sendHello();
            // Aktive Lok direkt anfragen (Adresse kommt aus Logic via Callback-
            // Kette nicht direkt; main.cpp setzt nach Begin die Adresse, hier
            // wird pauschal die konfigurierte GW_ADDRESS angefragt).
            sendSelectMsg(GW_ADDRESS);
            sendRequestMsg(GW_ADDRESS);
            break;

        case WStype_DISCONNECTED:
            Serial.println("[WS] disconnected");
            if (gCb.onOnline) gCb.onOnline(false);
            break;

        case WStype_TEXT:
        {
            JsonDocument doc;
            if (deserializeJson(doc, payload, length))
            {
                Serial.println("[WS] JSON parse error");
                break;
            }
            const char* t = doc["type"] | "";
            if (strcmp(t, "loco_status") == 0 || strcmp(t, "state_snapshot") == 0)
            {
                uint8_t addr = (int)doc["address"] | 0;
                int spd = (int)doc["speed"] | 0;
                const char* dir = doc["direction"] | "forward";
                bool fns[16] = {false};
                JsonObject fobj = doc["functions"];
                if (!fobj.isNull())
                {
                    for (int i = 0; i < 16; i++)
                        fns[i] = fobj[String(i + 1)] | false;
                }
                if (gCb.onState) gCb.onState(addr, spd, dir, fns);
                if (strcmp(t, "loco_status") == 0 && gCb.onRedraw) gCb.onRedraw();
            }
            else if (strcmp(t, "hello_ack") == 0)
            {
                Serial.println("[RX] hello_ack -> ready");
                if (gCb.onRedraw) gCb.onRedraw();
            }
            // command_ack: bewusst ohne Aktion
            break;
        }
        default:
            break;
    }
}

// ============================================================================
void commBegin(const CommCallbacks& cb)
{
    gCb = cb;
    connectWiFi();
    ws.begin(GW_HOST, GW_PORT, "/ws");
    ws.onEvent(webSocketEvent);
    ws.setReconnectInterval(2000);
}

void commLoop()
{
    static uint32_t lastPing = 0;
    ws.loop();
    uint32_t now = millis();
    if (now - lastPing > 10000)   // alle 10s echter WS-Ping (Heartbeat)
    {
        lastPing = now;
        ws.sendPing();
    }
}

void commSendDrive(uint8_t address, int speed, const char* dir)
{
    sendDriveMsg(address, speed, dir);
}

void commSendFunction(uint8_t address, int fn, bool state)
{
    sendFunctionMsg(address, fn, state);
}

void commSendSelectLoco(uint8_t address)
{
    sendSelectMsg(address);
}

void commSendRequestState(uint8_t address)
{
    sendRequestMsg(address);
}

void commSendEmergencyStop()
{
    sendEmergency();
}
