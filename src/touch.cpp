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
#include <Preferences.h>   // NVS-Speicher fuer dauerhafte Touch-Kalibrierung

// --- Interne Zustaende -----------------------------------------------------
static uint16_t _xraw = 0, _yraw = 0, _zraw = 0;  // letzte Rohwerte
static uint32_t _msraw = 0;                        // Zeitstempel letzter Sample
static bool     _haveSample = false;               // wurde schon gesampelt?

// Auto-Kalibrierungsgrenzen (starten bei den Startwerten aus config.h)
static uint16_t _xMin = TS_MIN_X, _xMax = TS_MAX_X;
static uint16_t _yMin = TS_MIN_Y, _yMax = TS_MAX_Y;

// Edge-Detection-Zustand (PacoMouseCYD-Prinzip)
static bool _wasPressed = false;     // war im letzten Zyklus gedrueckt?
static uint32_t _tapSumX = 0, _tapSumY = 0;   // Akkumulator fuer Mittelung
static uint8_t  _tapCount = 0;     // Anzahl gemittelter Samples

// --- Manuelle Ecken-Kalibrierung (Bodmer/PacoMouseCYD-Prinzip) ---
// Besser als reine Auto-Calib, weil NVS-gespeichert (nicht fluechtig)
// und die Start-Range NICHT nur erweitert, sondern exakt auf die echte
// Panel-Range gesetzt wird -> keine systematische Y-Verschiebung mehr.
static bool     _calibrating = false;
static uint32_t _calibDeadline = 0;     // wann das Fenster schliesst
static uint8_t  _calibSamples = 0;     // wieviele Ecken-Samples erfasst
static uint32_t _calX = 0, _calY = 0; // Akkumulator fuer aktuelle Ecke
// Backup der gueltigen Limits (bei abgebrochener Kalib wiederherstellen)
static uint16_t _xMinBak = TS_MIN_X, _xMaxBak = TS_MAX_X;
static uint16_t _yMinBak = TS_MIN_Y, _yMaxBak = TS_MAX_Y;

// Hilfsfunktion: laden/speichern der kalibrierten Grenzen in NVS
static void touchLoadCalibration()
{
    Preferences p;
    if (p.begin("touchcal", true))   // read-only
    {
        _xMin = p.getUShort("xmin", TS_MIN_X);
        _xMax = p.getUShort("xmax", TS_MAX_X);
        _yMin = p.getUShort("ymin", TS_MIN_Y);
        _yMax = p.getUShort("ymax", TS_MAX_Y);
        p.end();
    }
}
static void touchSaveCalibration()
{
    Preferences p;
    if (p.begin("touchcal", false))  // read-write
    {
        p.putUShort("xmin", _xMin);
        p.putUShort("xmax", _xMax);
        p.putUShort("ymin", _yMin);
        p.putUShort("ymax", _yMax);
        p.end();
    }
}

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
    // Ggf. gespeicherte Ecken-Kalibrierung aus NVS laden
    touchLoadCalibration();

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
    int16_t x = (_xMax > _xMin) ? map((int)_xraw, (int)_xMin, (int)_xMax, 0, W - 1) : (W - 1) / 2;
    int16_t y = (_yMax > _yMin) ? map((int)_yraw, (int)_yMin, (int)_yMax, 0, H - 1) : (H - 1) / 2;
    *px = constrain(x, 0, W - 1);
    *py = constrain(y, 0, H - 1);
}

void touchGetCalibration(uint16_t* xMin, uint16_t* xMax,
                         uint16_t* yMin, uint16_t* yMax)
{
    *xMin = _xMin; *xMax = _xMax;
    *yMin = _yMin; *yMax = _yMax;
}

bool touchGetTap(int16_t* px, int16_t* py)
{
    bool pressed = touchIsPressed();

    if (pressed)
    {
        // Rohwerte sammeln fuer Mittelung (nur wenn gueltiges Sample)
        if (_haveSample)
        {
            _tapSumX += _xraw;
            _tapSumY += _yraw;
            _tapCount++;
        }
    }

    // --- Kalibrier-Modus: Ecke nach Ecke erfassen ---
    if (_calibrating)
    {
        if (millis() > _calibDeadline)
        {
            // Fenster abgelaufen -> ggf. speichern + beenden
            if (_calibSamples == 4)
            {
                touchSaveCalibration();
                Serial.println("[CALIB] fertig -> in NVS gespeichert");
            }
            else
            {
                // Abgebrochen: gueltige Limits wiederherstellen
                _xMin = _xMinBak; _xMax = _xMaxBak;
                _yMin = _yMinBak; _yMax = _yMaxBak;
                Serial.printf("[CALIB] abgebrochen (%d/4 Ecken) -> alte Limits restored\n", _calibSamples);
            }
            _calibrating = false;
            return false;
        }
        // bei jedem Finger-Down die aktuelle Ecke mitteln
        if (pressed && !_wasPressed && _tapCount > 0)
        {
            uint16_t rx = (uint16_t)(_tapSumX / _tapCount);
            uint16_t ry = (uint16_t)(_tapSumY / _tapCount);
            // Grenzen exakt auf die echte Panel-Range setzen
            if (_calibSamples == 0 || rx < _xMin) _xMin = rx;
            if (_calibSamples == 0 || rx > _xMax) _xMax = rx;
            if (_calibSamples == 0 || ry < _yMin) _yMin = ry;
            if (_calibSamples == 0 || ry > _yMax) _yMax = ry;
            _calibSamples++;
            Serial.printf("[CALIB] Ecke %d: RX=%u RY=%u (MinX=%u MaxX=%u MinY=%u MaxY=%u)\n",
                           _calibSamples, rx, ry, _xMin, _xMax, _yMin, _yMax);
        }
        // Akkumulatoren zuruecksetzen (wie bei normalem Tap)
        if (!pressed)
        {
            _wasPressed = false;
            _tapSumX = _tapSumY = 0;
            _tapCount = 0;
        }
        else
        {
            _wasPressed = true;
        }
        return false;   // im Kalib-Modus keine normalen Taps auswerten
    }

    if (pressed && !_wasPressed)
    {
        // Wenn wir mind. 1 Sample haben, gemittelte Rohwerte nehmen
        uint16_t rx = _xraw, ry = _yraw;
        if (_tapCount > 0)
        {
            rx = (uint16_t)(_tapSumX / _tapCount);
            ry = (uint16_t)(_tapSumY / _tapCount);
        }
        // Auf Display-Pixel mappen (Auto-Calib-Grenzen)
        // Guard gegen min==max (wuerde sonst map()-Fehler ausloesen)
        const int16_t W = 240, H = 320;
        int16_t mx = (_xMax > _xMin) ? (int)map((int)rx, (int)_xMin, (int)_xMax, 0, W - 1) : (W - 1) / 2;
        int16_t my = (_yMax > _yMin) ? (int)map((int)ry, (int)_yMin, (int)_yMax, 0, H - 1) : (H - 1) / 2;
        *px = constrain(mx, 0, W - 1);
        *py = constrain(my, 0, H - 1);
        _wasPressed = true;
        return true;   // << ein Tap, genau einmal
    }

    // Finger losgelassen -> Zustaende zuruecksetzen fuer naechsten Tap
    if (!pressed)
    {
        _wasPressed = false;
        _tapSumX = _tapSumY = 0;
        _tapCount = 0;
    }
    return false;
}

void touchStartCalibration()
{
    // Gueltige Limits sichern (bei Abbruch wiederherstellen)
    _xMinBak = _xMin; _xMaxBak = _xMax;
    _yMinBak = _yMin; _yMaxBak = _yMax;

    _calibrating  = true;
    _calibDeadline = millis() + 12000;   // 12 s Fenster
    _calibSamples  = 0;
    _tapSumX = _tapSumY = 0;
    _tapCount = 0;
    _wasPressed = false;
    // Start-Range zuruecksetzen, damit Ecke 1 die Baseline setzt
    _xMin = 4095; _xMax = 0; _yMin = 4095; _yMax = 0;
    Serial.println("[CALIB] Starte: alle 4 Ecken nacheinander antippen (12 s)");
}
