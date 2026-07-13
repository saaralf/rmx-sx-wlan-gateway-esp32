// ============================================================================
// encoder.cpp — EC11/KY-040 Drehregler Treiber
// ----------------------------------------------------------------------------
// Pollt CLK/DT/SW und erzeugt EncoderEvent-Strukturen fuer Logic/GUI.
//
// Besonderheiten:
//   - CLK/DT ueber 4-Phasen-Zustandsmaschine
//   - SW ist Input Only -> Edge-Auswertung + Entprellung + Longpress
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

static uint8_t  g_prevState   = 0;

// Transitionstabelle fuer EC11-Quadratur (index = prev*4 + curr).
// +1 = CW, -1 = CCW, 0 = kein Schritt.
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

    // SW: IO35 ist Input Only und hat KEINE internen Pull-ups.
    // Das Modul sollte einen externen Pull-up nach 3.3V haben.
    pinMode(ENC_SW, INPUT);

    g_swLevel    = false;
    g_swPressed  = false;
    g_swDownMs   = 0;
    g_swLongSent = false;
    g_prevState  = readState();
}

bool encoderPoll(EncoderEvent* out)
{
    if (out)
    {
        out->steps     = 0;
        out->pressed   = false;
        out->released  = false;
        out->longPress = false;
    }

    const uint8_t state = readState();
    bool event = false;

    // --- Drehung -------------------------------------------------------------
    if (state != g_prevState)
    {
        const int8_t delta = g_encoderDelta[g_prevState * 4 + state];
        if (delta != 0)
        {
            if (out) out->steps = (int32_t)delta * (int32_t)ENC_STEPS_PER_CLICK;
            event = true;
        }
        g_prevState = state;
    }

    // --- Taster SW -----------------------------------------------------------
    const bool swNow = digitalRead(ENC_SW) == HIGH;
    if (!g_swLevel && swNow)
    {
        g_swPressed  = true;
        g_swDownMs   = millis();
        g_swLongSent = false;
        if (out) out->pressed = true;
        event = true;
    }

    if (g_swLevel && !swNow && g_swPressed)
    {
        g_swPressed = false;
        if (out) out->released = true;
        event = true;
    }

    if (g_swPressed && !g_swLongSent &&
        (millis() - g_swDownMs >= (uint32_t)ENC_SW_LONG_MS))
    {
        g_swLongSent = true;
        if (out) out->longPress = true;
        event = true;
    }

    g_swLevel = swNow;
    return event;
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
