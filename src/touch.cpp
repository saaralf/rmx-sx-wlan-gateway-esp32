// ============================================================================
// touch.cpp — XPT2046 Bit-Bang Treiber + Auto-Kalibrierung
// ----------------------------------------------------------------------------
// Implementiert den resisitiven Touchscreen-Treiber fuer das CYD 2.8".
// BIT-BANG (Software-SPI) auf den Pins 25/32/39/33/36, weil der XPT2046
// nicht am Display-SPI haengt. Code-Struktur angelehnt an PacoMouseCYD
// (bewaehrt fuer dieses Panel).
//
// Auto-Kalibrierung: Beim Beruehren werden die beobachteten Roh-Min/Max
// Werte laufend erweitert (xMin sinkt, xMax steigt etc.). Nach dem
// Antippen aller Ecken ist das Panel praktisch kalibriert, ohne dass der
// User ein separatens Kalibrier-Menue durchlaufen muss.
// ============================================================================

#include "touch.h"

// --- Interne Zustaende -----------------------------------------------------
static uint16_t _xraw = 0, _yraw = 0, _zraw = 0;  // letzte Rohwerte
static uint32_t _msraw = 0;                        // Zeitstempel letzter Sample
static bool     _haveSample = false;               // wurde schon gesampelt?

// Auto-Kalibrierungsgrenzen (starten bei den Startwerten aus config.h)
static uint16_t _xMin = TS_MIN_X, _xMax = TS_MAX_X;
static uint16_t _yMin = TS_MIN_Y, _yMax = TS_MAX_Y;

// ============================================================================
// Bit-Bang Low-Level: ein Command-Byte (8 Bit) + 12 Datenbits lesen
// ============================================================================
static uint16_t touchReadSPI(uint8_t command)
{
    uint16_t result = 0;
    // 1) Command-Byte senden (MSB first)
    for (int i = 7; i >= 0; i--)
    {
        digitalWrite(TOUCH_MOSI, (command & (1u << i)) ? HIGH : LOW);
        digitalWrite(TOUCH_CLK, HIGH);
        delayMicroseconds(7);
        digitalWrite(TOUCH_CLK, LOW);
        delayMicroseconds(7);
    }
    // 2) 12 Datenbits lesen (MSB first)
    for (int i = 11; i >= 0; i--)
    {
        digitalWrite(TOUCH_CLK, HIGH);
        delayMicroseconds(7);
        digitalWrite(TOUCH_CLK, LOW);
        delayMicroseconds(7);
        result |= (uint16_t)(digitalRead(TOUCH_MISO) << i);
    }
    return result;
}

// ============================================================================
// Vollstaendiger XPT2046-Lesezyklus (Z1/Z2, X, Y) + Rotation + Auto-Calib
// ============================================================================
static void touchUpdate()
{
    uint32_t now = millis();
    if (now - _msraw < 4) return;   // ~4ms Ratebegrenzung (MSEC_THRESHOLD)
    _msraw = now;

    digitalWrite(TOUCH_CS, LOW);
    // Z-Messung (z1 + 4095 - z2 = Druckstaerke)
    touchReadSPI(0xB0); touchReadSPI(0xB0); touchReadSPI(0xB0);
    int z1 = touchReadSPI(0xB0);
    _zraw = z1 + 4095;
    touchReadSPI(0xC0); touchReadSPI(0xC0); touchReadSPI(0xC0);
    int z2 = touchReadSPI(0xC0);
    _zraw -= z2;
    // X-Messung (Control 0x90)
    touchReadSPI(0x90); touchReadSPI(0x90); touchReadSPI(0x90);
    uint16_t rx = touchReadSPI(0x90);
    // Y-Messung (Control 0xD0)
    touchReadSPI(0xD0); touchReadSPI(0xD0); touchReadSPI(0xD0);
    uint16_t ry = touchReadSPI(0xD0);
    digitalWrite(TOUCH_CS, HIGH);

    // Rotation 0 (wie PacoMouseCYD): x/y tauschen und y invertieren
    int t = 4095 - ry;
    _xraw = (uint16_t)t;
    _yraw = rx;
    _haveSample = true;

    // Auto-Kalibrierung: Grenzen lazily erweitern
    if (_xraw < _xMin) _xMin = _xraw;
    if (_xraw > _xMax) _xMax = _xraw;
    if (_yraw < _yMin) _yMin = _yraw;
    if (_yraw > _yMax) _yMax = _yraw;
}

// ============================================================================
// Oeffentliche API
// ============================================================================

void touchBegin()
{
    pinMode(TOUCH_CLK,  OUTPUT);
    digitalWrite(TOUCH_CLK, LOW);
    pinMode(TOUCH_MOSI, OUTPUT);
    pinMode(TOUCH_MISO, INPUT);
    pinMode(TOUCH_CS,   OUTPUT);
    digitalWrite(TOUCH_CS, HIGH);
    pinMode(TOUCH_IRQ,  INPUT);
    _msraw = 0;
    _haveSample = false;
    _xMin = TS_MIN_X; _xMax = TS_MAX_X;
    _yMin = TS_MIN_Y; _yMax = TS_MAX_Y;
}

void touchSample(TSPoint* out)
{
    touchUpdate();
    if (out)
    {
        out->x = _xraw;
        out->y = _yraw;
        out->z = _zraw;
    }
}

bool touchIsPressed()
{
    // Nur neu sampeln, wenn noch kein frisches Sample in dieser Rate vorliegt
    touchUpdate();
    return (_zraw > TS_Z_THRESHOLD);
}

void touchGetCalibrated(int16_t* px, int16_t* py)
{
    const int16_t W = 240, H = 320;   // Layout::screenWidth/Height (siehe gui.h)
    int16_t x = map((int)_xraw, (int)_xMin, (int)_xMax, 0, W - 1);
    int16_t y = map((int)_yraw, (int)_yMin, (int)_yMax, 0, H - 1);
    *px = constrain(x, 0, W - 1);
    *py = constrain(y, 0, H - 1);
}

void touchGetCalibration(uint16_t* xMin, uint16_t* xMax,
                         uint16_t* yMin, uint16_t* yMax)
{
    *xMin = _xMin; *xMax = _xMax;
    *yMin = _yMin; *yMax = _yMax;
}
