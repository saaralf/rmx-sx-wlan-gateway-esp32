// ============================================================================
// logic.cpp — Fahrregler-Geschaftslogik
// ----------------------------------------------------------------------------
// Implementiert den State und die Touch-Auswertung (siehe logic.h).
// Layout-Abhaengigkeiten (Button-Rechtecke) werden ueber die Layout-
// Konstanten aus gui.h bezogen, damit Logic unabhaengig von der konkreten
// Zeichenroutine bleibt — sie kennt nur "wo" die Regionen liegen.
// ============================================================================

#include "logic.h"
#include "gui.h"     // Layout-Konstanten (Button-Rects)

// ---- Zustandsvariablen ----------------------------------------------------
int       logicSpeed        = 0;
int       logicTargetSpeed  = 0;
uint8_t   logicAddress      = GW_ADDRESS;
bool      logicLightOn      = true;
Direction logicDirection    = Direction::FORWARD;
bool      logicOnline       = false;

uint8_t   logicVisibleFunctions[8] = { 1, 2, 3, 4, 9, 10, 11, 12 };
FunctionConfig logicFunctions[16] =
{
    {1,false},{2,false},{3,false},{4,false},
    {5,false},{6,false},{7,false},{8,false},
    {9,false},{10,false},{11,false},{12,false},
    {13,false},{14,false},{15,false},{16,false}
};

bool logicDirtyDrive  = false;
bool logicDirtySelect = false;

// ---- Hilfsfunktion --------------------------------------------------------
static bool pointInRect(int16_t x, int16_t y, const Rect& r)
{
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

// ============================================================================
void logicBegin()
{
    // Demo-Startzustand: F1 + F11 aktiv
    logicFunctions[0].active  = true;
    logicFunctions[10].active = true;
    logicAddress      = GW_ADDRESS;
    logicSpeed        = 0;
    logicTargetSpeed  = 0;
    logicLightOn      = true;
    logicDirection    = Direction::FORWARD;
    logicDirtyDrive   = false;
    logicDirtySelect  = false;
}

// ============================================================================
bool logicApplyTouch(int16_t px, int16_t py)
{
    const char* dirStr = (logicDirection == Direction::FORWARD) ? "forward" : "reverse";

    // --- Lok-Dropdown (Lok wechseln, Demo: Toggle 110 <-> 42) ---
    if (pointInRect(px, py, Layout::locomotiveDropDown))
    {
        logicAddress = (logicAddress == 110) ? 42 : 110;
        logicDirtySelect = true;
        logicSpeed = logicTargetSpeed = 0;
        return true;
    }

    // --- Licht (F0) ---
    if (pointInRect(px, py, Layout::lightButton))
    {
        logicLightOn = !logicLightOn;
        logicDirtyDrive = true;   // F0 muss gesendet werden
        return true;
    }

    // --- Adress-Pfeile (+/-) ---
    if (pointInRect(px, py, Layout::addressSelector))
    {
        const Rect& r = Layout::addressSelector;
        const int16_t separatorX = r.x + r.w - 18;
        if (px >= separatorX)
        {
            // Bereich 1..255 (DCC-kompatibel). Hinweis: Selectrix/RMX/SLX
            // kennt nur 112 Adressen (0..111); die Obergrenze ist bewusst
            // weiter gefasst, bis der Raspi die Protokoll-Grenze vorgibt.
            if (py < r.y + r.h / 2) logicAddress = min(255, (int)logicAddress + 1);
            else                     logicAddress = max(1,   (int)logicAddress - 1);
            logicDirtySelect = true;
            logicSpeed = logicTargetSpeed = 0;
        }
        return true;
    }

    // --- Funktionsspalten (2x4) ---
    for (int row = 0; row < 4; row++)
    {
        const int16_t y = Layout::functionStartY +
                          row * (Layout::functionButtonH + Layout::functionGap);
        const uint8_t lf = logicVisibleFunctions[row];
        if (lf >= 1 && lf <= 16 &&
            pointInRect(px, py, {Layout::functionLeftX, y,
                                 Layout::functionButtonW, Layout::functionButtonH}))
        {
            logicFunctions[lf - 1].active = !logicFunctions[lf - 1].active;
            logicDirtyDrive = true;
            return true;
        }
        const uint8_t rf = logicVisibleFunctions[row + 4];
        if (rf >= 1 && rf <= 16 &&
            pointInRect(px, py, {Layout::functionRightX, y,
                                 Layout::functionButtonW, Layout::functionButtonH}))
        {
            logicFunctions[rf - 1].active = !logicFunctions[rf - 1].active;
            logicDirtyDrive = true;
            return true;
        }
    }

    // --- Untere Steuerbuttons ---
    if (pointInRect(px, py, Layout::accelerateButton))
    {
        logicTargetSpeed = min(99, logicTargetSpeed + 5);
        logicDirtyDrive = true;
        return true;
    }
    if (pointInRect(px, py, Layout::forwardButton))
    {
        logicDirection = Direction::FORWARD;
        logicDirtyDrive = true;
        return true;
    }
    if (pointInRect(px, py, Layout::reverseButton))
    {
        logicDirection = Direction::REVERSE;
        logicDirtyDrive = true;
        return true;
    }
    if (pointInRect(px, py, Layout::emergencyButton))
    {
        logicTargetSpeed = 0;
        logicSpeed = 0;
        logicDirtyDrive = true;   // sendEmergencyStop erfolgt ueber comm
        return true;
    }

    // --- Throttle-Slide (vertikal -> Zielgeschwindigkeit) ---
    const Rect& slider = Layout::throttle;
    if (px >= slider.x - 6 && px <= slider.x + slider.w + 6 &&
        py >= slider.y && py <= slider.y + slider.h)
    {
        int mapped = map(py, slider.y + slider.h - 15, slider.y + 15, 0, 99);
        logicTargetSpeed = constrain(mapped, 0, 99);
        logicDirtyDrive = true;
        return true;
    }

    return false;   // ins Leere getippt
}

// ============================================================================
void logicSetState(uint8_t addr, int speed, const char* dir,
                   const bool fnStates[16])
{
    if (addr != logicAddress) return;   // nur unsere Lok
    logicSpeed = speed;
    if (logicTargetSpeed != speed) logicTargetSpeed = speed;
    logicDirection = (strcmp(dir, "reverse") == 0) ? Direction::REVERSE
                                                    : Direction::FORWARD;
    for (int i = 0; i < 16; i++)
        logicFunctions[i].active = fnStates[i];
}

void logicSetOnline(bool online)
{
    logicOnline = online;
}
