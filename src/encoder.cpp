// ============================================================================
// encoder.cpp — EC11/KY-040 Drehregler Treiber
// ----------------------------------------------------------------------------
// Pollt CLK/DT/SW und erzeugt EncoderEvent-Strukturen fuer Logic/GUI.
//
// Besonderheiten:
//   - CLK/DT ueber 4-Phasen-Zustandsmaschine mit gueltiger Phasendifferenz
//   - SW ist Input Only -> Edge-Auswertung + Entprellung + Longpress
//   - Zusätzliche Rate-Limiter gegen Prellen und Mehrfach-Events
// ============================================================================

#include "encoder.h"
#include "config.h"
#include <Arduino.h>

// ============================================================================
// Zustandsvariablen
// ============================================================================
static EncoderMode g_mode = EncoderMode::SPEED;

static bool     g_swLevel     = false;
static bool     g_swPressed   = false;
static uint32_t g_swDownMs    = 0;
static bool     g_swLongSent  = false;

// Vorheriger gültiger Encoder-Phasenstand (0..3).
// Ungültige Übergänge durch Prellen werden verworfen.
static uint8_t  g_prevState   = 0;
static uint32_t g_lastEventMs = 0;

// Gültige Quadratur-Übergangstabelle: index = prev*4 + curr.
// +1 = CW, -1 = CCW, 0 = ungültig/Prellen.
static const int8_t g_encoderDelta[16] =
{
     0, -1, +1,  0,
    +1,  0,  0, -1,
    -1,  0,  0, +1,
     0, +1, -1,  0
};

// ============================================================================
// Hilfsfunktionen
// ============================================================================
static uint8_t readState()
{
    const uint8_t clk = digitalRead(ENC_CLK) == HIGH ? 1u : 0u;
    const uint8_t dt  = digitalRead(ENC_DT)  == HIGH ? 2u : 0u;
    return (uint8_t)(clk | dt);
}

// ============================================================================
// Oeffentliche API
// ============================================================================
void encoderBegin()
{
    pinMode(ENC_CLK, INPUT_PULLUP);
    pinMode(ENC_DT,  INPUT_PULLUP);

    // SW: IO35 ist Input Only. Mit aktivem Pull-up nach 3.3V liegt hier
    // HIGH = Taster offen, LOW = Taster gedrueckt.
    pinMode(ENC_SW, INPUT);

    g_swLevel    = false;
    g_swPressed  = false;
    g_swDownMs   = 0;
    g_swLongSent = false;
    g_prevState  = readState();
    g_lastEventMs = 0;
}

int32_t encoderPollSteps()
{
    const uint32_t now = millis();
    const uint8_t state = readState();
    int32_t steps = 0;

    if (state != g_prevState)
    {
        if (now - g_lastEventMs >= (uint32_t)ENC_MIN_EVENT_MS)
        {
            const int8_t delta = g_encoderDelta[g_prevState * 4 + state];
            if (delta != 0)
            {
                steps = (int32_t)delta * (int32_t)ENC_STEPS_PER_CLICK;
                g_lastEventMs = now;
            }
        }
        g_prevState = state;
    }

    return steps;
}

bool encoderPollSw(bool* pressed, bool* released, bool* longPress)
{
    if (pressed)   *pressed   = false;
    if (released)  *released  = false;
    if (longPress) *longPress = false;

    const uint32_t now = millis();
    // Mit Pull-up nach 3.3V: HIGH = offen, LOW = gedrueckt.
    const bool swNow = digitalRead(ENC_SW) == HIGH;

    // Fallende Flanke: Taster wurde gedrueckt.
    if (g_swLevel && !swNow)
    {
        g_swPressed  = true;
        g_swDownMs   = now;
        g_swLongSent = false;
        if (pressed) *pressed = true;
    }

    // Steigende Flanke: Taster wurde losgelassen.
    if (!g_swLevel && swNow && g_swPressed)
    {
        g_swPressed = false;
        if (released) *released = true;
    }

    // Automatischer Langpress nach definierter Zeit.
    if (g_swPressed && !g_swLongSent &&
        (now - g_swDownMs >= (uint32_t)ENC_SW_LONG_MS))
    {
        g_swLongSent = true;
        if (longPress) *longPress = true;
    }

    g_swLevel = swNow;
    return pressed && *pressed || released && *released || longPress && *longPress;
}

EncoderMode encoderToggleMode()
{
    g_mode = (g_mode == EncoderMode::SPEED) ? EncoderMode::ADDRESS : EncoderMode::SPEED;
    return g_mode;
}

EncoderMode encoderCurrentMode()
{
    return g_mode;
}
