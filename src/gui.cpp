// ============================================================================
// gui.cpp — Graphical User Interface (TFT_eSPI Zeichen-Layer)
// ----------------------------------------------------------------------------
// EINZIGER Ort mit direktem Display-Zugriff. Liest den anzuzeigenden
// Zustand aus dem Logic-Layer (logic.h) und besitzt KEINE eigene
// Geschaeftslogik. Trennung:  Logic = WAS  |  GUI = WIE es gezeichnet wird.
//
// Ablauf im Programm:
//   guiBegin()        in setup(): Display-Init + einmaliges Vollbild
//   guiDrawScreen()   Voll-Refresh (statisches Layout + Status + FW-Version)
//   guiUpdateDynamic() nur aendernde Teile (Digitalanzeige, Slider, Status)
//   guiDrawDebugTouch()/guiDrawLoopCounter() Diagnose-Leiste unten/rechts
// ============================================================================

#include "gui.h"
#include "logic.h"
#include "types.h"
#include "config.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>

// ---- Globales Display-Objekt ---------------------------------------------
TFT_eSPI guiTft = TFT_eSPI();

// ---- Farbpalette ----------------------------------------------------------
constexpr uint16_t COLOR_BACKGROUND = TFT_DARKGREY;
constexpr uint16_t COLOR_PANEL      = 0x4208;
constexpr uint16_t COLOR_BUTTON     = 0xBDF7;
constexpr uint16_t COLOR_BLUE_LED   = 0x05FF;
constexpr uint16_t COLOR_INACTIVE   = 0x8410;

// ============================================================================
// Hilfsfunktionen (Zeichnen)
// ============================================================================

/**
 * @brief Zeichnet einen abgesetzten (bevelten) Button mit Rahmen.
 * @param x,y      Linke obere Ecke in Pixeln
 * @param w,h      Breite/Hoehe in Pixeln
 * @param fillColor     Fuellfarbe des Buttons
 * @param active   true = aktiver Zustand (gelber Rahmen + Highlight)
 * @return void
 * @note Wird von allen statischen Button-Zeichnungen (Licht, Adresse,
 *       Funktionen, untere Steuerbuttons) genutzt. Liegt in guiBegin-
 *       Vollbild und guiUpdateDynamic-Kontext.
 */
static void drawBeveledButton(int16_t x, int16_t y, int16_t w, int16_t h,
                              uint16_t fillColor, bool active = false)
{
    const uint16_t borderColor = active ? TFT_YELLOW : TFT_WHITE;
    guiTft.fillRoundRect(x, y, w, h, 3, fillColor);
    guiTft.drawRoundRect(x, y, w, h, 3, borderColor);
    guiTft.drawFastHLine(x + 3, y + 2, w - 6, TFT_WHITE);
    guiTft.drawFastVLine(x + 2, y + 3, h - 6, TFT_WHITE);
    guiTft.drawFastHLine(x + 3, y + h - 3, w - 6, TFT_DARKGREY);
    guiTft.drawFastVLine(x + w - 3, y + 3, h - 6, TFT_DARKGREY);
}

/**
 * @brief Zeichnet zentrierten Text (Middle-Center Datum).
 * @param text     Anzuzeigender String
 * @param centerX  Horizontale Mitte in Pixeln
 * @param centerY  Vertikale Mitte in Pixeln
 * @param color    Textfarbe
 * @param font     TFT-Fontnummer (1/2/4/6/7)
 * @param background Hintergrundfarbe (TFT_TRANSPARENT = keine)
 * @return void
 * @note Utility fuer alle beschrifteten Elemente; nutzt guiTft.
 */
static void drawCenteredText(const String& text, int16_t centerX, int16_t centerY,
                             uint16_t color, uint8_t font = 2,
                             uint16_t background = TFT_TRANSPARENT)
{
    guiTft.setTextDatum(MC_DATUM);
    if (background == TFT_TRANSPARENT) guiTft.setTextColor(color);
    else guiTft.setTextColor(color, background);
    guiTft.drawString(text, centerX, centerY, font);
}

/**
 * @brief Zeichnet linksbuendigen Text (Middle-Left Datum).
 * @param text     Anzuzeigender String
 * @param x        Linke Textposition in Pixeln
 * @param centerY  Vertikale Mitte in Pixeln
 * @param color    Textfarbe
 * @param font     TFT-Fontnummer (1/2/4/6/7)
 * @param background Hintergrundfarbe (TFT_TRANSPARENT = keine)
 * @return void
 * @note Utility fuer Beschriftungen am linken Rand (Lokname, Licht).
 */
static void drawLeftText(const String& text, int16_t x, int16_t centerY,
                         uint16_t color, uint8_t font = 2,
                         uint16_t background = TFT_TRANSPARENT)
{
    guiTft.setTextDatum(ML_DATUM);
    if (background == TFT_TRANSPARENT) guiTft.setTextColor(color);
    else guiTft.setTextColor(color, background);
    guiTft.drawString(text, x, centerY, font);
}

/**
 * @brief Zeichnet einen Pfeil-Dreieck (links/rechts).
 * @param centerX,centerY  Spitzen-Mittelpunkt in Pixeln
 * @param size     Halbe Kantenlaenge
 * @param right    true = Pfeil nach rechts, false = nach links
 * @param color    Fuellfarbe
 * @return void
 * @note Wird fuer Vor/Rueck-Buttons verwendet.
 */
static void drawArrowTriangle(int16_t centerX, int16_t centerY,
                              int16_t size, bool right, uint16_t color)
{
    if (right)
        guiTft.fillTriangle(centerX - size, centerY - size,
                            centerX - size, centerY + size,
                            centerX + size, centerY, color);
    else
        guiTft.fillTriangle(centerX + size, centerY - size,
                            centerX + size, centerY + size,
                            centerX - size, centerY, color);
}

/**
 * @brief Zeichnet ein Gluehbirnen-Symbol (Licht).
 * @param x,y   Mittelpunkt der Birne in Pixeln
 * @param active true = eingeschaltet (gelb), false = aus (grau)
 * @return void
 * @note Wird von drawLightButton() genutzt.
 */
static void drawLightIcon(int16_t x, int16_t y, bool active)
{
    const uint16_t color = active ? TFT_YELLOW : COLOR_INACTIVE;
    guiTft.fillCircle(x, y, 5, color);
    guiTft.drawCircle(x, y, 6, TFT_BLACK);
    guiTft.drawLine(x, y - 10, x, y - 7, color);
    guiTft.drawLine(x, y + 7, x, y + 10, color);
    guiTft.drawLine(x - 10, y, x - 7, y, color);
    guiTft.drawLine(x + 7, y, x + 10, y, color);
}

// ============================================================================
// Statische Elemente
// ============================================================================

/**
 * @brief Zeichnet den Online-Statuspunkt oben rechts im Lok-Panel.
 * @param keine (liest logicOnline aus logic.h)
 * @return void
 * @note Wird in guiDrawScreen() und guiUpdateDynamic() aufgerufen.
 */
static void drawStatusBar()
{
    const Rect& r = Layout::locomotive;
    guiTft.fillCircle(r.x + r.w - 12, r.y + 19, 5,
                      logicOnline ? TFT_GREEN : TFT_RED);
}

/**
 * @brief Zeichnet die Lok-Symbolleiste (oben, Panel mit Lok-Silhouette).
 * @param keine
 * @return void
 * @note Teil des Vollbilds (guiDrawScreen). Liest keine Logic-Werte.
 */
static void drawLocomotive()
{
    const Rect& r = Layout::locomotive;
    guiTft.fillRoundRect(r.x, r.y, r.w, r.h, 4, COLOR_PANEL);
    guiTft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_WHITE);
    const int16_t x = r.x + 80, y = r.y + 10;
    guiTft.fillRect(x, y, 72, 13, TFT_YELLOW);
    guiTft.fillRect(x + 4,  y + 3, 13, 6, TFT_BLUE);
    guiTft.fillRect(x + 21, y + 3, 13, 6, TFT_BLUE);
    guiTft.fillRect(x + 38, y + 3, 13, 6, TFT_BLUE);
    guiTft.fillRect(x + 55, y + 3, 13, 6, TFT_BLUE);
    guiTft.fillRect(x, y + 13, 72, 5, TFT_BLUE);
    guiTft.fillCircle(x + 13, y + 19, 4, TFT_BLACK);
    guiTft.fillCircle(x + 59, y + 19, 4, TFT_BLACK);
    guiTft.drawLine(x + 28, y, x + 36, y - 7, TFT_RED);
    guiTft.drawLine(x + 36, y - 7, x + 44, y, TFT_RED);
    guiTft.drawFastHLine(x + 30, y - 8, 18, TFT_RED);
}

/**
 * @brief Zeichnet Loknamen-Feld + Dropdown-Pfeil.
 * @param keine (Name folgt der aktiven Adresse: "Lok <logicAddress>")
 * @return void
 * @note Teil des Vollbilds. Name ist generisch; ein echter Lokname koennte
 *       spaeter ueber den Gateway-State vom Raspi mitgeliefert werden.
 */
static void drawLocName()
{
    const Rect& nameRect = Layout::locomotiveName;
    const Rect& dropRect = Layout::locomotiveDropDown;
    drawBeveledButton(nameRect.x, nameRect.y, nameRect.w, nameRect.h, TFT_WHITE);
    drawLeftText("Lok " + String(logicAddress), nameRect.x + 7, nameRect.y + nameRect.h / 2,
                 TFT_BLACK, 4, TFT_WHITE);
    drawBeveledButton(dropRect.x, dropRect.y, dropRect.w, dropRect.h, COLOR_BUTTON);
    guiTft.fillTriangle(dropRect.x + 7,  dropRect.y + 11,
                        dropRect.x + 20, dropRect.y + 11,
                        dropRect.x + 13, dropRect.y + 20, TFT_BLACK);
}

/**
 * @brief Zeichnet den Licht-Button (F0) mit Gluehbirnen-Symbol.
 * @param keine (liest logicLightOn aus logic.h)
 * @return void
 * @note Vollbild + guiUpdateDynamic (Zustand aenderbar).
 */
static void drawLightButton()
{
    const Rect& r = Layout::lightButton;
    const uint16_t buttonColor = logicLightOn ? TFT_YELLOW : COLOR_BUTTON;
    drawBeveledButton(r.x, r.y, r.w, r.h, buttonColor, logicLightOn);
    drawLightIcon(r.x + 17, r.y + r.h / 2, logicLightOn);
    drawLeftText("Licht", r.x + 31, r.y + r.h / 2, TFT_BLACK, 2, buttonColor);
}

/**
 * @brief Zeichnet den Adress-Waehler (+/- Pfeile, aktuelle Adresse).
 * @param keine (liest logicAddress aus logic.h)
 * @return void
 * @note Vollbild + guiUpdateDynamic.
 */
static void drawAddressSelector()
{
    const Rect& r = Layout::addressSelector;
    drawBeveledButton(r.x, r.y, r.w, r.h, TFT_WHITE);
    drawCenteredText(String(logicAddress), r.x + 24, r.y + r.h / 2,
                     TFT_BLACK, 2, TFT_WHITE);
    const int16_t sepX = r.x + r.w - 18;
    guiTft.drawFastVLine(sepX, r.y + 2, r.h - 4, TFT_DARKGREY);
    guiTft.fillTriangle(sepX + 9, r.y + 5,  sepX + 4, r.y + 12, sepX + 14, r.y + 12, TFT_BLACK);
    guiTft.fillTriangle(sepX + 9, r.y + r.h - 5, sepX + 4, r.y + r.h - 12,
                        sepX + 14, r.y + r.h - 12, TFT_BLACK);
}

/**
 * @brief Zeichnet die digitale Geschwindigkeitsanzeige (3-stellig).
 * @param keine (liest logicSpeed aus logic.h)
 * @return void
 * @note Vollbild + guiUpdateDynamic (Speed aendert sich haeufig).
 */
static void drawDigitalDisplay()
{
    const Rect& r = Layout::speedDisplay;
    guiTft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_NAVY);
    guiTft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
    char buf[4];
    snprintf(buf, sizeof(buf), "%03d", logicSpeed);
    drawCenteredText(buf, r.x + r.w / 2, r.y + r.h / 2, COLOR_BLUE_LED, 4, TFT_NAVY);
}

/**
 * @brief Zeichnet einen einzelnen Funktions-Button (F1..F16).
 * @param x,y,w,h  Geometrie des Buttons
 * @param f        FunctionConfig aus logic.h (functionNumber + active)
 * @return void
 * @note Wird von drawFunctionColumns() fuer alle 8 sichtbaren Funktionen
 *       aufgerufen. Liest f.active fuer Highlight.
 */
static void drawFunctionButton(int16_t x, int16_t y, int16_t w, int16_t h,
                               FunctionConfig& f)
{
    const uint16_t bc = f.active ? TFT_YELLOW : COLOR_BUTTON;
    drawBeveledButton(x, y, w, h, bc, f.active);
    drawCenteredText("F" + String(f.functionNumber), x + w / 2, y + h / 2,
                     TFT_BLACK, 4, bc);
}

/**
 * @brief Zeichnet die 2x4 Funktionsspalte (aus logicVisibleFunctions).
 * @param keine (liest logicVisibleFunctions[8] + logicFunctions[16])
 * @return void
 * @note Vollbild + guiUpdateDynamic (Funktionen schaltbar).
 */
static void drawFunctionColumns()
{
    for (int row = 0; row < 4; row++)
    {
        const int16_t y = Layout::functionStartY +
                          row * (Layout::functionButtonH + Layout::functionGap);
        uint8_t lf = logicVisibleFunctions[row];
        if (lf >= 1 && lf <= 16)
            drawFunctionButton(Layout::functionLeftX, y, Layout::functionButtonW,
                               Layout::functionButtonH, logicFunctions[lf - 1]);
        uint8_t rf = logicVisibleFunctions[row + 4];
        if (rf >= 1 && rf <= 16)
            drawFunctionButton(Layout::functionRightX, y, Layout::functionButtonW,
                               Layout::functionButtonH, logicFunctions[rf - 1]);
    }
}

/**
 * @brief Zeichnet Throttle-Slider + Speed-Gauge (Balkenanzeige).
 * @param keine (liest logicTargetSpeed + logicSpeed aus logic.h)
 * @return void
 * @note Vollbild + guiUpdateDynamic (Beide Werte aendern sich bei Fahrt).
 */
static void drawThrottle()
{
    const Rect& slider = Layout::throttle;
    const Rect& gauge  = Layout::speedGauge;

    guiTft.fillRoundRect(slider.x, slider.y, slider.w, slider.h, 5, TFT_BLACK);
    guiTft.drawRoundRect(slider.x, slider.y, slider.w, slider.h, 5, TFT_WHITE);
    guiTft.drawFastVLine(slider.x + slider.w / 2, slider.y + 8, slider.h - 16, TFT_LIGHTGREY);
    for (int i = 0; i <= 10; i++)
    {
        int16_t markY = slider.y + 8 + i * ((slider.h - 16) / 10);
        guiTft.drawFastHLine(slider.x + 6, markY, 6, TFT_DARKGREY);
        guiTft.drawFastHLine(slider.x + slider.w - 12, markY, 6, TFT_DARKGREY);
    }
    int16_t sp = map(logicTargetSpeed, 0, 99,
                     slider.y + slider.h - 15, slider.y + 15);
    guiTft.fillRoundRect(slider.x + 3, sp - 9, slider.w - 6, 19, 4, TFT_LIGHTGREY);
    guiTft.drawRoundRect(slider.x + 3, sp - 9, slider.w - 6, 19, 4, TFT_WHITE);
    guiTft.fillRect(slider.x + 7, sp - 2, slider.w - 14, 5, TFT_YELLOW);

    guiTft.fillRoundRect(gauge.x, gauge.y, gauge.w, gauge.h, 5, TFT_BLACK);
    guiTft.drawRoundRect(gauge.x, gauge.y, gauge.w, gauge.h, 5, TFT_WHITE);
    int16_t it = gauge.y + 6, ib = gauge.y + gauge.h - 6, ih = ib - it;
    int16_t fh = map(logicSpeed, 0, 99, 0, ih);
    if (fh > 0) guiTft.fillRect(gauge.x + 6, ib - fh, gauge.w - 12, fh, TFT_YELLOW);
    for (int i = 1; i <= 9; i++)
    {
        int16_t segY = it + i * ih / 10;
        guiTft.drawFastHLine(gauge.x + 6, segY, gauge.w - 12, TFT_ORANGE);
    }
}

/**
 * @brief Zeichnet die unteren Steuerbuttons (Gas / Rueck / Vor / STOP).
 * @param keine
 * @return void
 * @note Vollbild. Buttons sind statisch (Hit-Test in logic.cpp).
 */
static void drawBottomControls()
{
    const Rect& gas  = Layout::accelerateButton;
    const Rect& back = Layout::reverseButton;
    const Rect& front= Layout::forwardButton;
    const Rect& stop = Layout::emergencyButton;

    drawBeveledButton(gas.x, gas.y, gas.w, gas.h, COLOR_BUTTON);
    guiTft.fillTriangle(gas.x + gas.w / 2, gas.y + 6,
                        gas.x + 15, gas.y + 27,
                        gas.x + gas.w - 15, gas.y + 27, TFT_GREEN);
    drawCenteredText("Gas", gas.x + gas.w / 2, gas.y + gas.h - 8, TFT_BLACK, 1, COLOR_BUTTON);

    drawBeveledButton(back.x, back.y, back.w, back.h, COLOR_BUTTON);
    drawArrowTriangle(back.x + back.w / 2, back.y + 20, 10, false, TFT_WHITE);
    drawCenteredText("Rueck", back.x + back.w / 2, back.y + back.h - 8, TFT_BLACK, 1, COLOR_BUTTON);

    drawBeveledButton(front.x, front.y, front.w, front.h, COLOR_BUTTON);
    drawArrowTriangle(front.x + front.w / 2, front.y + 20, 10, true, TFT_YELLOW);
    drawCenteredText("Vor", front.x + front.w / 2, front.y + front.h - 8, TFT_BLACK, 1, COLOR_BUTTON);

    drawBeveledButton(stop.x, stop.y, stop.w, stop.h, COLOR_BUTTON);
    guiTft.fillTriangle(stop.x + stop.w / 2, stop.y + 29,
                        stop.x + 15, stop.y + 7,
                        stop.x + stop.w - 15, stop.y + 7, TFT_RED);
    drawCenteredText("STOP", stop.x + stop.w / 2, stop.y + stop.h - 8, TFT_BLACK, 1, COLOR_BUTTON);
}

// ============================================================================
// Oeffentliche GUI-API
// ============================================================================

/**
 * @brief Initialisiert das Display und zeichnet das Vollbild einmalig.
 * @param keine
 * @return void
 * @note Muss in setup() NACH touchBegin() + logicBegin(), VOR commBegin()
 *       stehen, da das Vollbild logicOnline anzeigt (noch false).
 */
void guiBegin()
{
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    guiTft.init();
    guiTft.setRotation(0);
    guiTft.fillScreen(COLOR_BACKGROUND);
    guiDrawScreen();
}

/**
 * @brief Zeichnet das komplette statische Layout neu (Voll-Refresh).
 * @param keine
 * @return void
 * @note Nur bei grossen Aenderungen (Screenwechsel / setup) aufrufen.
 */
void guiDrawScreen()
{
    guiTft.fillScreen(COLOR_BACKGROUND);
    // FW-Version oben links
    guiTft.setTextDatum(TL_DATUM);
    guiTft.setTextColor(TFT_WHITE, COLOR_BACKGROUND);
    guiTft.drawString(FW_VERSION, 4, 4, 1);

    drawLocomotive();
    drawLocName();
    drawLightButton();
    drawAddressSelector();
    drawDigitalDisplay();
    drawFunctionColumns();
    drawThrottle();
    drawBottomControls();
    drawStatusBar();
}

/**
 * @brief Zeichnet nur die dynamischen Elemente neu (ohne Vollbild-Loeschen).
 * @param keine (liest logicSpeed, logicTargetSpeed, logicOnline, logicLightOn,
 *              logicFunctions, logicVisibleFunctions aus logic.h)
 * @return void
 * @note Wird nach jeder Logic-Aenderung / Statusmeldung aufgerufen.
 *       Zeichnet: Clear-Bereich um Slider+Gauge, Digitalanzeige, Slider,
 *       Statuspunkt. (Debug-Leiste wird separat von main.cpp gesetzt.)
 */
void guiUpdateDynamic()
{
    const Rect& slider = Layout::throttle;
    const Rect& gauge  = Layout::speedGauge;
    // Bereich um Slider + Gauge loeschen
    guiTft.fillRect(slider.x - 3, slider.y - 3,
                    (gauge.x + gauge.w) - (slider.x - 3) + 3,
                    (slider.y + slider.h) - (slider.y - 3) + 3,
                    COLOR_BACKGROUND);

    drawDigitalDisplay();
    drawThrottle();
    drawStatusBar();
    // Licht-Button + Funktionen koennen sich aendern -> auch neu zeichnen
    drawLightButton();
    drawFunctionColumns();
}

/**
 * @brief Zeigt die Touch-Diagnose-Leiste unten am Bildschirm an.
 * @param touched  true wenn aktuell gedrueckt
 * @param rx,ry    kalibrierte Roh-X/Y (0..4095)
 * @param mx,my    gemappte Display-X/Y (0..239 / 0..319)
 * @return void
 * @note Dient der visuellen Verifikation der Touch-Koordinaten (ersetzt
 *       den alten Green-Bug: kein fillScreen mehr, nur Textzeile).
 */
void guiDrawDebugTouch(bool touched, int16_t rx, int16_t ry,
                      int16_t mx, int16_t my)
{
    guiTft.fillRect(0, 305, 240, 12, TFT_BLACK);
    guiTft.setTextDatum(TL_DATUM);
    guiTft.setTextColor(TFT_WHITE, TFT_BLACK);
    char buf[60];
    snprintf(buf, sizeof(buf), "T:%c RX%d RY%d MX%d MY%d",
             touched ? 'Y' : 'n', rx, ry, mx, my);
    guiTft.drawString(buf, 2, 306, 1);
}

/**
 * @brief Zeigt den Loop-Zaehler oben rechts an (Laufzeit-Diagnose).
 * @param n  Anzahl der seit Start durchlaufenen loop()-Iterationen / 10.
 * @return void
 * @note Wird ca. alle 150ms von main.cpp mit inkrementierendem Wert gerufen.
 */
void guiDrawLoopCounter(uint32_t n)
{
    guiTft.fillRect(180, 0, 60, 12, TFT_BLACK);
    guiTft.setTextDatum(TL_DATUM);
    guiTft.setTextColor(TFT_WHITE, TFT_BLACK);
    guiTft.drawString("L" + String(n), 182, 2, 1);
}

/**
 * @brief Zeichnet ein kleines Kreuz + Kreis an eine Display-Ecke
 *        (waehrend der Touch-Kalibrierung).
 * @param x,y      Eck-Position in Pixeln (Rotation 0)
 * @param color    TFT_YELLOW = noch zu tippen, TFT_GREEN = erledigt
 * @return void
 * @note Wird von touch.cpp (Kalib-Modus) per extern-Callback gerufen.
 *       Ein 20px-Kreuz + 12px-Kreis macht die Zielposition gut sichtbar.
 */
void touchDrawCalibCross(int16_t x, int16_t y, uint16_t color)
{
    const int16_t len = 10;   // halbe Kreuz-Arm-Laenge
    guiTft.drawFastHLine(x - len, y, 2 * len, color);
    guiTft.drawFastVLine(x, y - len, 2 * len, color);
    guiTft.drawCircle(x, y, 12, color);
}
