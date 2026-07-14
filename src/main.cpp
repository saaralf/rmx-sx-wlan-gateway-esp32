// ============================================================================
// main.cpp — Orchestrierung (setup/loop) des ESP32-2432S028R Lokreglers
// ----------------------------------------------------------------------------
// Dies ist der einzige Einstiegspunkt (Arduino entry). Er verdrahtet die
// vier Schichten:
//   1. Touch  (touch.cpp)   - Rohdaten + Auto-Kalibrierung
//   2. Logic  (logic.cpp)   - Fahrregler-State + Touch-Auswertung
//   3. Comm   (comm.cpp)    - Gateway WebSocket
//   4. GUI    (gui.cpp)     - TFT-Anzeige
//
// Ablauf:
//   setup():  guiInitDisplay -> touchBegin -> logicBegin -> guiBegin -> commBegin(callbacks)
//   loop():   commLoop() + Touch-Sample -> Logic -> ggf. Comm-Senden + GUI
//
// Wichtige Korrektur gegenueber v0.1.12: Der Screen wird NICHT mehr
// dauerhaft gruen (Green-Bug). Statt fillScreen(GREEN) zeigen wir nur die
// Debug-Leiste mit "T:Y" + kalibrierte Koordinaten. Sobald der Touch
// losgelassen wird (touchIsPressed()==false), erscheinen wieder die
// normalen Werte, das Bild bleibt aber unveraendert.
// ============================================================================

#include <Arduino.h>
#include "types.h"
#include "config.h"
#include "touch.h"
#include "logic.h"
#include "encoder.h"
#include "comm.h"
#include "gui.h"
#include "sdcard.h"
#include "locomotives.h"

// ---- Verdrahtung: Comm -> Logic ------------------------------------------
// Wird vom Gateway gemeldeter Lok-Status in den Logic-State gespiegelt.
static void onGatewayState(uint8_t addr, int speed, const char *dir,
                           const bool fnStates[16])
{
    logicSetState(addr, speed, dir, fnStates);

    guiUpdateSpeed();
    guiUpdateAddress();
    guiUpdateFunctions();
}

static void onGatewayOnline(bool online)
{
    logicSetOnline(online);
    guiUpdateConnectionStatus();
}
static void onGatewayRedraw()
{
    guiInvalidateDynamic();
    guiUpdateDynamic();
}

// ============================================================================
// Arduino setup
// ============================================================================
void setup()
{
    Serial.begin(115200);

    guiInitDisplay(); // TFT-Init + Backlight an (+ 2x Blink) MUSS vor touchBegin/logicBegin stehen (gui.h)
    touchBegin();     // Touch-Pins (Bit-Bang) initialisieren
    logicBegin();     // Demo-Startzustaende
    encoderBegin();   // Drehregler initialisieren

    if (sdCardBegin())
    {
        sdCardList("/", 2);

        if (locomotivesBegin())
        {
            locomotivesPrintAll();
        }
    }
    guiBegin(); // Vollbild UI (TFT ist bereits via guiInitDisplay() initialisiert)

    commBegin({onGatewayState, onGatewayOnline, onGatewayRedraw});

    Serial.printf("ESP32 Lok-Fahrregler %s (refactored) gestartet\n", FW_VERSION);
}

// ============================================================================
// Arduino loop
// ============================================================================
void loop()
{
    commLoop(); // WebSocket Heartbeat + Reconnect

    // Temporären Adressfokus automatisch beenden.
    if (logicUpdateEncoderFocus())
    {
        guiUpdateConnectionStatus();
    }
    // Encoder bei JEDEM loop()-Durchlauf abfragen.
    // Nicht an das langsamere Touch-Intervall koppeln.
    EncoderEvent encoderEvent{};

    if (encoderPoll(&encoderEvent))
    {
        const bool changed = logicApplyEncoder(encoderEvent);

        if (changed)
        {
            if (logicDirtySelect)
            {
                commSendSelectLoco(logicAddress);
                commSendRequestState(logicAddress);
                logicDirtySelect = false;
            }

            if (logicDirtyDrive)
            {
                const char *dir =
                    logicDirection == Direction::FORWARD
                        ? "forward"
                        : "reverse";

                if (logicEmergencyStopRequested)
                {
                    commSendEmergencyStop();
                    logicEmergencyStopRequested = false;
                }
                else
                {
                    commSendDrive(
                        logicAddress,
                        logicTargetSpeed,
                        dir);
                }

                logicDirtyDrive = false;
            }

            guiUpdateDynamic();
        }
    }
    // --- Touch-Verarbeitung ---
    // PacoMouseCYD-Prinzip: EIN Finger-Down = EINE Aktion (Edge-Detection
    // + Mittelung in touchGetTap). So kann ein gehaltener Finger (oder
    // Sampleschwankung) nicht benachbarte Buttons mehrfach toggeln.
    static uint32_t lastTouch = 0;
    if (millis() - lastTouch > 30)
    {
        lastTouch = millis();
        int16_t px = 0, py = 0;
        if (touchGetTap(&px, &py)) // true NUR beim Uebergang released->pressed
        {
            bool changed = logicApplyTouch(px, py);

            // 2) Wenn geaendert -> Gateway informieren (Comm)
            if (changed)
            {
                if (logicDirtySelect)
                {
                    commSendSelectLoco(logicAddress);
                    commSendRequestState(logicAddress);
                    logicDirtySelect = false;
                }
                if (logicDirtyDrive)
                {
                    const char *dir = (logicDirection == Direction::FORWARD)
                                          ? "forward"
                                          : "reverse";
                    // STOP-Taster: emergency_stop senden (sofortiger Halt).
                    // Nur wenn der Taster explizit gedrueckt wurde — ein
                    // normales "Speed 0" ueber Slider/Gas sendet drive speed=0.
                    if (logicEmergencyStopRequested)
                    {
                        commSendEmergencyStop();
                        logicEmergencyStopRequested = false;
                    }
                    else
                    {
                        commSendDrive(logicAddress, logicTargetSpeed, dir);
                    }
                    // Licht (F0) gesondert senden
                    commSendFunction(logicAddress, 0, logicLightOn);
                    // ALLE Zusatzfunktionen F1..F16 explicit senden (true/false),
                    // nicht nur die aktiven — sonst bleibt ein AUS-Toggle beim
                    // Raspi/Zentrale haengen (wir muessen unsere Aenderungen
                    // vollstaendig zurueckschreiben).
                    for (int i = 0; i < 16; i++)
                        commSendFunction(logicAddress,
                                         logicFunctions[i].functionNumber,
                                         logicFunctions[i].active);
                    logicDirtyDrive = false;
                }
            }

            // 3) GUI: statische/dynamische Teile neu zeichnen
            guiUpdateDynamic();
        }
    }

    // --- Diagnose-Leiste (alle ~150ms, NUR bei DEBUG_OVERLAY=1) ----------
    // T:N (Touch) + L (Loop-Counter) sind reine Debug-Overlays. Per Default
    // AUS (DEBUG_OVERLAY=0 in config.h). Einschalten: platformio.ini
    // -D DEBUG_OVERLAY=1. ACHTUNG: #if (Wertpruefung), nicht #ifdef —
    // das Makro ist immer definiert (Wert 0), sonst liefe der Code trotzdem.
#if DEBUG_OVERLAY
    static uint32_t lastDbg = 0;
    static uint32_t loopCount = 0;
    loopCount++;
    if (millis() - lastDbg > 150)
    {
        lastDbg = millis();
        TSPoint d;
        touchSample(&d);
        int16_t px = 0, py = 0;
        touchGetCalibrated(&px, &py);
        bool pressed = (d.z > TS_Z_THRESHOLD);
        guiDrawDebugTouch(pressed, (int16_t)d.x, (int16_t)d.y, px, py);
        guiDrawLoopCounter(loopCount / 10);
    }
#endif

    // --- Serial-Kommando "calib": manuelle Touch-Ecken-Kalibrierung ---
    static String serialLine = "";
    while (Serial.available())
    {
        char c = Serial.read();
        if (c == '\n' || c == '\r')
        {
            serialLine.trim();
            if (serialLine == "calib")
            {
                touchStartCalibration(); // 12 s Fenster, 4 Ecken antippen
            }
            serialLine = "";
        }
        else
        {
            serialLine += c;
        }
    }
}
