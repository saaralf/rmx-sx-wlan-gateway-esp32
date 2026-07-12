// ============================================================================
// touch.cpp — XPT2046 Bit-Bang Treiber + affine Eck-Kalibrierung
// ----------------------------------------------------------------------------
// Resistiver Touchscreen-Treiber fuer das CYD 2.8".
// BIT-BANG (Software-SPI) auf den Pins 25/32/39/33/36, weil der XPT2046
// nicht am Display-SPI haengt. Code-Struktur angelehnt an PacoMouseCYD.
//
// KALIBRIERUNG: Statt lausiger Min/Max-Grenzen (die bei gedrehten/
// invertierten Touch-Achsen versagen) wird eine 2D-AFFINE ABBILDUNG
// (Least-Squares ueber 4 Eck-Messpunkte, siehe Adafruit/Bodmer) gelernt:
//     px = a*rx + b*ry + c
//     py = d*rx + e*ry + f
// Die Matrix wird in NVS gespeichert und ist nach dem Boot sofort aktiv.
// ============================================================================

#include "touch.h"
#include <Preferences.h>   // NVS-Speicher fuer dauerhafte Touch-Kalibrierung
#include <math.h>          // fabsf() fuer NVS-Gueltigkeitscheck

// --- Interne Zustaende -----------------------------------------------------
static uint16_t _xraw = 0, _yraw = 0, _zraw = 0;  // letzte Rohwerte
static uint32_t _msraw = 0;                        // Zeitstempel letzter Sample
static bool     _haveSample = false;               // wurde schon gesampelt?

// --- Affine Kalibrierungsmatrix (aus 4 Eck-Messungen gelernt) -------------
//   px = a*rx + b*ry + c
//   py = d*rx + e*ry + f
// ACHTUNG: nicht bloss Min/Max-Grenzen! Nur so werden bei diesem Board die
// gedrehten/invertierten Touch-Achsen sauber korrigiert.
static float _m[6] = { 1, 0, 0,  0, 1, 0 };  // Default = Identitaet

// Backup der gueltigen Matrix (bei abgebrochener Kalib wiederherstellen)
static float _mBak[6] = { 1, 0, 0,  0, 1, 0 };

// Gemessene Rohwerte der 4 Kalib-Ecken (Index = Tipp-Reihenfolge)
static uint16_t _calibRX[4] = {0,0,0,0};
static uint16_t _calibRY[4] = {0,0,0,0};

// Kalib-Modus-Zustand
static bool     _calibrating  = false;
static uint32_t _calibDeadline = 0;
static uint8_t  _calibSamples = 0;

// Tap-Erkennung (Debounce + Mittelung ueber mehrere Samples)
static uint32_t _tapSumX = 0, _tapSumY = 0;
static uint16_t _tapCount = 0;
static bool     _wasPressed = false;

// Kreuz-Zeichnung im Kalib-Modus: GUI-Callback (in gui.cpp definiert)
// wird nur aufgerufen, wenn das Display erreichbar ist.
extern void touchDrawCalibCross(int16_t x, int16_t y, uint16_t color);

// ----------------------------------------------------------------------------
// 2D-affine Matrix per Least-Squares aus 4 (Roh -> Display) Punkten loesen.
// Bildet  px = a*rx + b*ry + c   und   py = d*rx + e*ry + f.
// rx[],ry[] = gemessene Rohwerte; dx[],dy[] = Soll-Display-Pixel.
// Schreibt 6 Koeffizienten nach out[6].
// Verifiziert gegen numpy lstsq (1px Genauigkeit an den Ecken).
// Implementierung: Normalengleichungen 3x3, geloest per Gauss-Elimination
// (keine fehleranfaelligen Hand-Cramer-Formeln).
// ----------------------------------------------------------------------------
static void touchSolveAffine(const uint16_t rx[4], const uint16_t ry[4],
                             const int16_t dx[4], const int16_t dy[4],
                             float out[6])
{
    // Normalengleichungen fuellen: A^T A * [a,b,c]^T = A^T d  (px)
    // und analog fuer py. Da beide dasselbe A^T A nutzen, loesen wir
    // das 3x3-System zweimal mit d=dx bzw. d=dy.
    double M[3][3];
    double bx[3], by[3];
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            M[r][c] = 0;
    bx[0]=bx[1]=bx[2]=0; by[0]=by[1]=by[2]=0;

    // Spalten von A: [rx, ry, 1]
    for (int i = 0; i < 4; i++)
    {
        double a0 = rx[i], a1 = ry[i], a2 = 1.0;
        double d0 = dx[i], d1 = dy[i];
        M[0][0] += a0*a0; M[0][1] += a0*a1; M[0][2] += a0*a2;
        M[1][0] += a1*a0; M[1][1] += a1*a1; M[1][2] += a1*a2;
        M[2][0] += a2*a0; M[2][1] += a2*a1; M[2][2] += a2*a2;
        bx[0] += a0*d0; bx[1] += a1*d0; bx[2] += a2*d0;
        by[0] += a0*d1; by[1] += a1*d1; by[2] += a2*d1;
    }

    // Gauss-Elimination (mit Spaltenpivot) fuer 3x3, beide RHS zusammen
    // (M wuerde sonst beim 1. Durchlauf zerstoert und fuer die 2. RHS unbrauchbar)
    for (int col = 0; col < 3; col++)
    {
        int piv = col;
        for (int r = col+1; r < 3; r++)
            if (fabs(M[r][col]) > fabs(M[piv][col])) piv = r;
        if (piv != col) {
            for (int c = 0; c < 3; c++) { double t=M[col][c]; M[col][c]=M[piv][c]; M[piv][c]=t; }
            double t=bx[col]; bx[col]=bx[piv]; bx[piv]=t;
            double u=by[col]; by[col]=by[piv]; by[piv]=u;
        }
        double d = M[col][col];
        if (fabs(d) < 1e-9) { out[0]=out[1]=out[2]=out[3]=out[4]=out[5]=0; return; }
        for (int c = col; c < 3; c++) M[col][c] /= d;
        bx[col] /= d; by[col] /= d;
        for (int r = 0; r < 3; r++) {
            if (r == col) continue;
            double f = M[r][col];
            for (int c = col; c < 3; c++) M[r][c] -= f * M[col][c];
            bx[r] -= f * bx[col];
            by[r] -= f * by[col];
        }
    }
    out[0]=bx[0]; out[1]=bx[1]; out[2]=bx[2];
    out[3]=by[0]; out[4]=by[1]; out[5]=by[2];
}

// Roh -> Display-Pixel ueber die affine Matrix
static void touchApplyMatrix(uint16_t rx, uint16_t ry, int16_t* px, int16_t* py)
{
    const int16_t W = 240, H = 320;
    int16_t x = (int16_t)(_m[0]*rx + _m[1]*ry + _m[2]);
    int16_t y = (int16_t)(_m[3]*rx + _m[4]*ry + _m[5]);
    *px = constrain(x, 0, W-1);
    *py = constrain(y, 0, H-1);
}


// Hilfsfunktion: laden/speichern der affinen Matrix in NVS (6 Floats)
static void touchLoadCalibration()
{
    Preferences p;
    if (p.begin("touchcal", true))   // read-only
    {
        _m[0] = p.getFloat("ma", 1.0f);
        _m[1] = p.getFloat("mb", 0.0f);
        _m[2] = p.getFloat("mc", 0.0f);
        _m[3] = p.getFloat("md", 0.0f);
        _m[4] = p.getFloat("me", 1.0f);
        _m[5] = p.getFloat("mf", 0.0f);
        p.end();
    }
}
static void touchSaveCalibration()
{
    Preferences p;
    if (p.begin("touchcal", false))  // read-write
    {
        p.putFloat("ma", _m[0]);
        p.putFloat("mb", _m[1]);
        p.putFloat("mc", _m[2]);
        p.putFloat("md", _m[3]);
        p.putFloat("me", _m[4]);
        p.putFloat("mf", _m[5]);
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

    // Rohe XPT2046-Werte direkt uebernehmen. KEINE Rotation/Invertierung
    // hier! Die affine Matrix (touchSolveAffine) korrigiert Achsenlage,
    // Invertierung und Skalierung sauber aus den 4 Eck-Messungen. Eine
    // feste Rotation wuerde bei diesem Board die Achsen falsch herumlegen.
    _xraw = rx;
    _yraw = ry;
    _haveSample = true;
}

// ============================================================================
// Oeffentliche API
// ============================================================================

void touchBegin()
{
    // Pin-Init zuerst (auch wenn NVS-Load fehlschlaegt)
    pinMode(TOUCH_CLK,  OUTPUT);
    digitalWrite(TOUCH_CLK, LOW);
    pinMode(TOUCH_MOSI, OUTPUT);
    pinMode(TOUCH_MISO, INPUT);
    pinMode(TOUCH_CS,   OUTPUT);
    digitalWrite(TOUCH_CS, HIGH);
    pinMode(TOUCH_IRQ,  INPUT);
    _msraw = 0;
    _haveSample = false;

    // Ggf. gespeicherte affine Matrix aus NVS laden.
    // Die Matrix ist nach dem Boot sofort aktiv (kein Default-Ueberschreiben).
    float nvM[6];
    bool nvValid = false;
    {
        Preferences p;
        if (p.begin("touchcal", true))   // read-only
        {
            nvM[0] = p.getFloat("ma", 1.0f);
            nvM[1] = p.getFloat("mb", 0.0f);
            nvM[2] = p.getFloat("mc", 0.0f);
            nvM[3] = p.getFloat("md", 0.0f);
            nvM[4] = p.getFloat("me", 1.0f);
            nvM[5] = p.getFloat("mf", 0.0f);
            p.end();
            // Gueltigkeits-Check: nicht die unberuehrte Identitaets-Matrix
            nvValid = (fabsf(nvM[0]-1.0f) > 1e-4 || fabsf(nvM[4]-1.0f) > 1e-4 ||
                       fabsf(nvM[1]) > 1e-4 || fabsf(nvM[3]) > 1e-4 ||
                       fabsf(nvM[2]) > 1e-3 || fabsf(nvM[5]) > 1e-3);
        }
    }
    if (nvValid)
    {
        for (int i = 0; i < 6; i++) _m[i] = nvM[i];
        Serial.printf("[CALIB] NVS geladen: a=%.4f b=%.4f c=%.1f d=%.4f e=%.4f f=%.1f\n",
                       _m[0], _m[1], _m[2], _m[3], _m[4], _m[5]);
    }
    else
    {
        // Keine gueltige Kalib -> Identitaet (Pixel = Rohwert, unkalibriert)
        _m[0]=1; _m[1]=0; _m[2]=0; _m[3]=0; _m[4]=1; _m[5]=0;
        Serial.println("[CALIB] NVS leer/invalid -> Identitaets-Matrix (unkalibriert)");
    }
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
    touchApplyMatrix(_xraw, _yraw, px, py);
}

void touchGetCalibration(uint16_t* xMin, uint16_t* xMax,
                         uint16_t* yMin, uint16_t* yMax)
{
    // Bei affiner Matrix liefern wir die rohen Grenzen der 4 Ecken zurueck
    (void)xMin; (void)xMax; (void)yMin; (void)yMax;
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
        // 4 Ziel-Punkte auf dem Display (Pixel, Panel 240x320).
        // NICHT ganz in die Ecken — dort misst der resistiv Panel oft
        // unzuverlaessig (Kreuzungspunkt der beiden Achsen). 15 % Inset:
        //   0: oben links, 1: oben rechts, 2: unten rechts, 3: unten links
        const int16_t ex[4] = { 36, 204, 204, 36 };
        const int16_t ey[4] = { 48, 48, 272, 272 };

        // Aktive Ecke gelb zeichnen, bereits erledigte gruen
        // (RGB565: Gelb=0xFFE0, Gruen=0x07E0 — keine TFT_eSPI-Deps in touch.cpp)
        for (int i = 0; i < 4; i++)
        {
            uint16_t c = (i < _calibSamples) ? 0x07E0 : 0xFFE0;
            if (i == _calibSamples) c = 0xFFE0;   // aktive Ecke hervorheben
            touchDrawCalibCross(ex[i], ey[i], c);
        }

        if (millis() > _calibDeadline)
        {
            // Fenster abgelaufen -> ggf. speichern + beenden
            if (_calibSamples == 4)
            {
                // Soll-Display-Pixel der 4 Punkte (wie oben)
                int16_t dx[4] = { ex[0], ex[1], ex[2], ex[3] };
                int16_t dy[4] = { ey[0], ey[1], ey[2], ey[3] };
                touchSolveAffine(_calibRX, _calibRY, dx, dy, _m);
                touchSaveCalibration();
                Serial.printf("[CALIB] fertig -> a=%.4f b=%.4f c=%.1f d=%.4f e=%.4f f=%.1f (NVS)\n",
                               _m[0], _m[1], _m[2], _m[3], _m[4], _m[5]);
                Serial.printf("[CALIB] Probe: Ecke0 rx=%u ry=%u -> px %d py %d (soll %d,%d)\n",
                               _calibRX[0], _calibRY[0],
                               (int)(_m[0]*_calibRX[0]+_m[1]*_calibRY[0]+_m[2]),
                               (int)(_m[3]*_calibRX[0]+_m[4]*_calibRY[0]+_m[5]),
                               dx[0], dy[0]);
            }
            else
            {
                // Abgebrochen: gueltige Matrix wiederherstellen
                for (int i = 0; i < 6; i++) _m[i] = _mBak[i];
                Serial.printf("[CALIB] abgebrochen (%d/4 Ecken) -> alte Matrix restored\n", _calibSamples);
            }
            _calibrating = false;
            return false;
        }
        // bei jedem Finger-Down die aktuelle Ecke mitteln
        if (pressed && !_wasPressed && _tapCount > 0)
        {
            uint16_t rx = (uint16_t)(_tapSumX / _tapCount);
            uint16_t ry = (uint16_t)(_tapSumY / _tapCount);
            _calibRX[_calibSamples] = rx;   // Rohwert nach Tipp-Reihenfolge merken
            _calibRY[_calibSamples] = ry;
            _calibSamples++;
            Serial.printf("[CALIB] Ecke %d: RX=%u RY=%u (gemerkt)\n",
                           _calibSamples, rx, ry);
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
        // Auf Display-Pixel ueber die affine Matrix abbilden
        touchApplyMatrix(rx, ry, px, py);
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
    // Gueltige Matrix sichern (bei Abbruch wiederherstellen)
    for (int i = 0; i < 6; i++) _mBak[i] = _m[i];

    _calibrating  = true;
    _calibDeadline = millis() + 30000;   // 30 s Fenster (mehr Zeit zum Zielen)
    _calibSamples  = 0;
    _tapSumX = _tapSumY = 0;
    _tapCount = 0;
    _wasPressed = false;
    Serial.println("[CALIB] Starte: 4 Punkte nacheinander antippen (30 s)");
    Serial.println("[CALIB] Kreuze: gelb = noch, gruen = erledigt. Oben links -> rechts -> unten rechts -> unten links");
}
