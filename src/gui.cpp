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
#include "sdcard.h"

#include <FS.h>
#include <SD.h>
#include "gui.h"
#include "logic.h"
#include "types.h"
#include "config.h"
#include "comm.h" // gwVersion (Gateway-Version aus hello_ack)
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "locomotives.h"

// ---- Globales Display-Objekt ---------------------------------------------
TFT_eSPI guiTft = TFT_eSPI();
static bool drawBmp24(
    const char *path,
    int16_t x,
    int16_t y);
// ============================================================================
// Zuletzt gezeichneter GUI-Zustand
// ============================================================================

namespace
{
    bool guiCacheValid = false;

    int guiLastSpeed = -1;
    int guiLastTargetSpeed = -1;

    int guiLastAddress = -1;

    bool guiLastOnline = false;
    bool guiLastLightOn = false;

    EncoderMode guiLastEncoderMode =
        static_cast<EncoderMode>(255);

    bool guiLastFunctions[16] = {false};

    uint8_t guiLastVisibleFunctions[8] = {0};
}

// ---- Farbpalette ----------------------------------------------------------
constexpr uint16_t COLOR_BACKGROUND = TFT_DARKGREY;
constexpr uint16_t COLOR_PANEL = 0x4208;
constexpr uint16_t COLOR_BUTTON = 0xBDF7;
constexpr uint16_t COLOR_BLUE_LED = 0x05FF;
constexpr uint16_t COLOR_INACTIVE = 0x8410;

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
static void drawCenteredText(const String &text, int16_t centerX, int16_t centerY,
                             uint16_t color, uint8_t font = 2,
                             uint16_t background = TFT_TRANSPARENT)
{
    guiTft.setTextDatum(MC_DATUM);
    if (background == TFT_TRANSPARENT)
        guiTft.setTextColor(color);
    else
        guiTft.setTextColor(color, background);
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
static void drawLeftText(const String &text, int16_t x, int16_t centerY,
                         uint16_t color, uint8_t font = 2,
                         uint16_t background = TFT_TRANSPARENT)
{
    guiTft.setTextDatum(ML_DATUM);
    if (background == TFT_TRANSPARENT)
        guiTft.setTextColor(color);
    else
        guiTft.setTextColor(color, background);
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
    const Rect &r = Layout::locomotive;
    guiTft.fillCircle(r.x + r.w - 12, r.y + 19, 5,
                      logicOnline ? TFT_GREEN : TFT_RED);

    // Gateway-Version oben links UNTER der FW-Version (auf dem Lok-Panel bei
    // (6,15)). Bewusst NICHT unten rechts: die Debug-Leiste (T:N) liegt bei
    // y~305 und ueberschreibt dort alles. (6,15) ist auf COLOR_PANEL sicher.
    // gwVersion ist ein statischer Puffer in comm.cpp (siehe comm.h).
    // Textbereich (6,15) VOR dem Zeichnen mit Panel-Farbe loeschen, damit
    // kein Rest der VORHERIGEN Variante haengen bleibt. Wichtig: "GW: ?" ist
    // 6 Zeichen breit, die echte Version (z.B. "0.1.1") nur 5 — ohne Clear
    // bliebe das rote "?" rechts als Rest stehen (Artefakt nach hello_ack).
    guiTft.fillRect(6, 15, 44, 9, COLOR_PANEL);
    guiTft.setTextDatum(TL_DATUM);
    if (strlen(gwVersion) > 0)
    {
        guiTft.setTextColor(TFT_LIGHTGREY, COLOR_PANEL);
        guiTft.drawString(gwVersion, 6, 15, 1);
    }
    else
    {
        guiTft.setTextColor(TFT_RED, COLOR_PANEL);
        guiTft.drawString("GW: ?", 6, 15, 1);
    }

    // Encoder-Modusanzeige direkt neben der Gateway-Version.
    // Konvention: 0 = SPEED, 1 = ADDRESS.
    const char *modeText = (encoderMode == EncoderMode::SPEED) ? "S" : "A";
    guiTft.fillRect(54, 15, 10, 9, COLOR_PANEL);
    guiTft.setTextColor(TFT_YELLOW, COLOR_PANEL);
    guiTft.drawString(modeText, 54, 15, 1);
}

/**
 * @brief Zeichnet die Lok-Symbolleiste (oben, Panel mit Lok-Silhouette).
 * @param keine
 * @return void
 * @note Teil des Vollbilds (guiDrawScreen). Liest keine Logic-Werte.
 */
static void drawLocomotive()
{
    const Rect &r = Layout::locomotive;

    guiTft.fillRoundRect(
        r.x,
        r.y,
        r.w,
        r.h,
        4,
        COLOR_PANEL);

    guiTft.drawRoundRect(
        r.x,
        r.y,
        r.w,
        r.h,
        4,
        TFT_WHITE);

    const LocomotiveInfo *locomotive =
        locomotivesFindByAddress(
            logicAddress);

    if (
        locomotive != nullptr &&
        locomotive->imagePath[0] != '\0')
    {
        if (
            drawBmp24(
                locomotive->imagePath,
                r.x + 68,
                r.y + 4))
        {
            return;
        }
    }

    // Fallback: bisherige, programmatisch gezeichnete Lok.
    const int16_t x = r.x + 80;
    const int16_t y = r.y + 10;

    guiTft.fillRect(
        x,
        y,
        72,
        13,
        TFT_YELLOW);

    guiTft.fillRect(
        x + 4,
        y + 3,
        13,
        6,
        TFT_BLUE);

    guiTft.fillRect(
        x + 21,
        y + 3,
        13,
        6,
        TFT_BLUE);

    guiTft.fillRect(
        x + 38,
        y + 3,
        13,
        6,
        TFT_BLUE);

    guiTft.fillRect(
        x + 55,
        y + 3,
        13,
        6,
        TFT_BLUE);

    guiTft.fillRect(
        x,
        y + 13,
        72,
        5,
        TFT_BLUE);

    guiTft.fillCircle(
        x + 13,
        y + 19,
        4,
        TFT_BLACK);

    guiTft.fillCircle(
        x + 59,
        y + 19,
        4,
        TFT_BLACK);

    guiTft.drawLine(
        x + 28,
        y,
        x + 36,
        y - 7,
        TFT_RED);

    guiTft.drawLine(
        x + 36,
        y - 7,
        x + 44,
        y,
        TFT_RED);

    guiTft.drawFastHLine(
        x + 30,
        y - 8,
        18,
        TFT_RED);
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
    const Rect &nameRect = Layout::locomotiveName;
    const Rect &dropRect = Layout::locomotiveDropDown;

    const LocomotiveInfo *locomotive =
        locomotivesFindByAddress(logicAddress);

    String locomotiveName;

    if (locomotive != nullptr)
    {
        locomotiveName = locomotive->name;
    }
    else
    {
        locomotiveName = "Lok " + String(logicAddress);
    }

    drawBeveledButton(
        nameRect.x,
        nameRect.y,
        nameRect.w,
        nameRect.h,
        TFT_WHITE
    );

    drawLeftText(
        locomotiveName,
        nameRect.x + 7,
        nameRect.y + nameRect.h / 2,
        TFT_BLACK,
        4,
        TFT_WHITE
    );

    drawBeveledButton(
        dropRect.x,
        dropRect.y,
        dropRect.w,
        dropRect.h,
        COLOR_BUTTON
    );

    guiTft.fillTriangle(
        dropRect.x + 7,
        dropRect.y + 11,
        dropRect.x + 20,
        dropRect.y + 11,
        dropRect.x + 13,
        dropRect.y + 20,
        TFT_BLACK
    );
}

/**
 * @brief Zeichnet den Licht-Button (F0) mit Gluehbirnen-Symbol.
 * @param keine (liest logicLightOn aus logic.h)
 * @return void
 * @note Vollbild + guiUpdateDynamic (Zustand aenderbar).
 */
static void drawLightButton()
{
    const Rect &r = Layout::lightButton;
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
    const Rect &r = Layout::addressSelector;
    drawBeveledButton(r.x, r.y, r.w, r.h, TFT_WHITE);
    drawCenteredText(String(logicAddress), r.x + 24, r.y + r.h / 2,
                     TFT_BLACK, 2, TFT_WHITE);
    const int16_t sepX = r.x + r.w - 18;
    guiTft.drawFastVLine(sepX, r.y + 2, r.h - 4, TFT_DARKGREY);
    guiTft.fillTriangle(sepX + 9, r.y + 5, sepX + 4, r.y + 12, sepX + 14, r.y + 12, TFT_BLACK);
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
    const Rect &r = Layout::speedDisplay;
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
                               FunctionConfig &f)
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
    const Rect &slider = Layout::throttle;
    const Rect &gauge = Layout::speedGauge;

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
    if (fh > 0)
        guiTft.fillRect(gauge.x + 6, ib - fh, gauge.w - 12, fh, TFT_YELLOW);
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
    const Rect &gas = Layout::accelerateButton;
    const Rect &back = Layout::reverseButton;
    const Rect &front = Layout::forwardButton;
    const Rect &stop = Layout::emergencyButton;

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
// haveFunctionsChanged
// Hat sich Licht F0 geändert?
// Hat sich F1 bis F16 geändert?
// Hat sich geändert, welche acht Funktionen sichtbar sind?
// ============================================================================
static bool haveFunctionsChanged()
{
    if (!guiCacheValid)
    {
        return true;
    }

    if (logicLightOn != guiLastLightOn)
    {
        return true;
    }

    for (int i = 0; i < 16; i++)
    {
        if (logicFunctions[i].active != guiLastFunctions[i])
        {
            return true;
        }
    }

    for (int i = 0; i < 8; i++)
    {
        if (logicVisibleFunctions[i] != guiLastVisibleFunctions[i])
        {
            return true;
        }
    }

    return false;
}

static void rememberFunctions()
{
    guiLastLightOn = logicLightOn;

    for (int i = 0; i < 16; i++)
    {
        guiLastFunctions[i] = logicFunctions[i].active;
    }

    for (int i = 0; i < 8; i++)
    {
        guiLastVisibleFunctions[i] = logicVisibleFunctions[i];
    }
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
void guiInitDisplay()
{
    pinMode(TFT_BL, OUTPUT);
    // 2x blinken = "setup erreicht, VOR TFT-init" (Signal fuer weissen Schirm)
    for (int i = 0; i < 2; i++)
    {
        digitalWrite(TFT_BL, LOW);
        delay(150);
        digitalWrite(TFT_BL, HIGH);
        delay(150);
    }
    guiTft.init();         // <-- hier haengt es ggf. (weisser Schirm)
    guiTft.setRotation(0); // senkrechtes Layout (240x320)
    guiBootPhase(TFT_RED, "BOOT");
}

void guiBootPhase(uint16_t color, const char *msg)
{
    guiTft.fillScreen(color);
    guiTft.setTextDatum(MC_DATUM);
    guiTft.setTextColor(TFT_WHITE, color);
    guiTft.drawString(msg, 120, 160, 4);
}

void guiSetRotation(uint8_t r)
{
    guiTft.setRotation(r);
}

void guiBegin()
{
    // TFT ist bereits via guiInitDisplay() initialisiert; hier nur das
    // echte UI zeichnen (Backlight/Init sind bereits passiert).
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

    drawLocomotive();
    drawLocName();
    drawLightButton();
    drawAddressSelector();
    drawDigitalDisplay();
    drawFunctionColumns();
    drawThrottle();
    drawBottomControls();
    drawStatusBar();

    // FW-Version oben links: MUSS nach drawLocomotive() kommen, da das
    // Lok-Panel (Layout::locomotive = {4,4,...}) die Ecke (4,4) uebermalt.
    // Text auf Panel-Farbe (nicht auf BACKGROUND) legen -> Kontrast weiss.
    guiTft.setTextDatum(TL_DATUM);
    guiTft.setTextColor(TFT_WHITE, COLOR_PANEL);
    guiTft.drawString(FW_VERSION, 6, 6, 1);
    guiLastSpeed = logicSpeed;
    guiLastTargetSpeed = logicTargetSpeed;
    guiLastAddress = logicAddress;
    guiLastOnline = logicOnline;
    guiLastEncoderMode = encoderMode;

    rememberFunctions();

    guiCacheValid = true;
}

//===========================================================
// Wichtig: Diese Version zeichnet Slider und Balken nur neu, wenn sich logicSpeed oder logicTargetSpeed tatsächlich geändert haben.
//
// Die bestehende Zeichenfunktion drawThrottle() baut Slider und Gauge derzeit weiterhin komplett neu auf.
// Das ist zunächst in Ordnung. Entscheidend ist, dass sie nicht mehr bei unveränderten Werten aufgerufen wird.
// Der bisherige große Löschbereich war in guiUpdateDynamic() enthalten.
//===========================================================
void guiUpdateSpeed()
{
    const bool speedChanged =
        !guiCacheValid ||
        logicSpeed != guiLastSpeed ||
        logicTargetSpeed != guiLastTargetSpeed;

    if (!speedChanged)
    {
        return;
    }

    const Rect &slider = Layout::throttle;
    const Rect &gauge = Layout::speedGauge;

    // Nur Slider und Gauge löschen.
    // Die digitale Anzeige löscht ihren eigenen Bereich selbst.
    guiTft.fillRect(
        slider.x - 3,
        slider.y - 3,
        (gauge.x + gauge.w) - (slider.x - 3) + 3,
        slider.h + 6,
        COLOR_BACKGROUND);

    drawDigitalDisplay();
    drawThrottle();

    guiLastSpeed = logicSpeed;
    guiLastTargetSpeed = logicTargetSpeed;
}
// Damit werden Lokname und Adressfeld nur bei einer echten Adressänderung neu gezeichnet.
void guiUpdateAddress()
{
    const bool addressChanged =
        !guiCacheValid ||
        logicAddress != guiLastAddress;

    if (!addressChanged)
    {
        return;
    }
    drawLocomotive();
    drawLocName();
    drawAddressSelector();

    guiLastAddress = logicAddress;
}

// Damit zeichnet eine Geschwindigkeitsänderung nicht mehr automatisch alle acht Funktionstasten neu.
//
// Im aktuellen Code ruft guiUpdateDynamic() nach jedem dynamischen Update sowohl drawLightButton() als auch drawFunctionColumns() auf.

void guiUpdateFunctions()
{
    if (!haveFunctionsChanged())
    {
        return;
    }

    drawLightButton();
    drawFunctionColumns();

    rememberFunctions();
}

//======================================================
// Damit wird die kleine Statusanzeige nur geändert, wenn:

// der Online-Zustand wechselt oder
// der Encoder-Modus wechselt.
// Besonderheit Gateway-Version

// Die Gateway-Version gwVersion ist hier noch nicht im Cache enthalten.
// Sie kommt vermutlich nur einmal nach hello_ack.
// Deshalb lösen wir diese später gezielt über guiInvalidateDynamic() oder einen direkten Status-Refresh aus.
//======================================================
void guiUpdateConnectionStatus()
{
    const bool statusChanged =
        !guiCacheValid ||
        logicOnline != guiLastOnline ||
        encoderMode != guiLastEncoderMode;

    if (!statusChanged)
    {
        return;
    }

    drawStatusBar();

    guiLastOnline = logicOnline;
    guiLastEncoderMode = encoderMode;
}
// Diese Funktion wird benötigt, wenn Inhalte neu gezeichnet werden müssen, obwohl sich die Logic-Variablen nicht geändert haben – beispielsweise nach Empfang der Gateway-Version.
void guiInvalidateDynamic()
{
    guiCacheValid = false;
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
    guiUpdateSpeed();
    guiUpdateAddress();
    guiUpdateFunctions();
    guiUpdateConnectionStatus();

    guiCacheValid = true;
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
    const int16_t len = 10; // halbe Kreuz-Arm-Laenge
    guiTft.drawFastHLine(x - len, y, 2 * len, color);
    guiTft.drawFastVLine(x, y - len, 2 * len, color);
    guiTft.drawCircle(x, y, 12, color);
}

static uint16_t readBmp16(File &file)
{
    uint16_t value = file.read();

    value |=
        static_cast<uint16_t>(file.read()) << 8;

    return value;
}

static uint32_t readBmp32(File &file)
{
    uint32_t value = file.read();

    value |=
        static_cast<uint32_t>(file.read()) << 8;

    value |=
        static_cast<uint32_t>(file.read()) << 16;

    value |=
        static_cast<uint32_t>(file.read()) << 24;

    return value;
}

/**
 * Zeichnet ein unkomprimiertes 24-Bit-BMP von der SD-Karte.
 *
 * Voraussetzungen:
 * - 24 Bit Farbtiefe
 * - keine Kompression
 * - Bild ist bereits auf die passende Größe skaliert
 */
static bool drawBmp24(
    const char *path,
    int16_t x,
    int16_t y)
{
    if (!sdCardReady())
    {
        return false;
    }

    File bmp = SD.open(path, FILE_READ);

    if (!bmp)
    {
        Serial.printf(
            "[GUI] BMP nicht gefunden: %s\n",
            path);

        return false;
    }

    // BMP-Signatur muss "BM" beziehungsweise 0x4D42 sein.
    if (readBmp16(bmp) != 0x4D42)
    {
        Serial.printf(
            "[GUI] Keine BMP-Datei: %s\n",
            path);

        bmp.close();
        return false;
    }

    // Dateigröße und reservierte Bytes werden nicht benötigt.
    (void)readBmp32(bmp);
    (void)readBmp32(bmp);

    const uint32_t pixelOffset =
        readBmp32(bmp);

    const uint32_t dibSize =
        readBmp32(bmp);

    const int32_t width =
        static_cast<int32_t>(readBmp32(bmp));

    const int32_t heightRaw =
        static_cast<int32_t>(readBmp32(bmp));

    const uint16_t planes =
        readBmp16(bmp);

    const uint16_t depth =
        readBmp16(bmp);

    const uint32_t compression =
        readBmp32(bmp);

    // Nur normales 24-Bit-BMP ohne Kompression unterstützen.
    if (
        dibSize < 40 ||
        width <= 0 ||
        heightRaw == 0 ||
        planes != 1 ||
        depth != 24 ||
        compression != 0)
    {
        Serial.printf(
            "[GUI] BMP-Format nicht unterstützt: %s\n",
            path);

        bmp.close();
        return false;
    }

    const bool bottomUp =
        heightRaw > 0;

    const int32_t height =
        bottomUp
            ? heightRaw
            : -heightRaw;

    // Jede BMP-Zeile wird auf ein Vielfaches von vier Bytes aufgefüllt.
    const uint32_t rowSize =
        (static_cast<uint32_t>(width) * 3U + 3U) & ~3U;

    // Sicherheitsprüfung für den Kopfbereich.
    if (
        width > Layout::locomotive.w - 4 ||
        height > Layout::locomotive.h - 4)
    {
        Serial.printf(
            "[GUI] BMP zu groß: %ldx%ld\n",
            static_cast<long>(width),
            static_cast<long>(height));

        bmp.close();
        return false;
    }

    // Unser Übungsbild ist 96 Pixel breit.
    // 100 Pixel Reserve reichen für diesen Loader.
    uint8_t rowBuffer[3 * 100 + 4];

    if (rowSize > sizeof(rowBuffer))
    {
        Serial.println(
            "[GUI] BMP-Zeile ist größer als der Puffer");

        bmp.close();
        return false;
    }

    // Bildbereich im Display festlegen.
    guiTft.startWrite();

    guiTft.setAddrWindow(
        x,
        y,
        width,
        height);

    for (int32_t screenRow = 0;
         screenRow < height;
         screenRow++)
    {
        const int32_t sourceRow =
            bottomUp
                ? height - 1 - screenRow
                : screenRow;

        const uint32_t rowPosition =
            pixelOffset +
            static_cast<uint32_t>(sourceRow) * rowSize;

        bmp.seek(rowPosition);

        const size_t bytesRead =
            bmp.read(
                rowBuffer,
                rowSize);

        if (bytesRead != rowSize)
        {
            Serial.printf(
                "[GUI] BMP-Lesefehler in Zeile %ld\n",
                static_cast<long>(screenRow));

            guiTft.endWrite();
            bmp.close();

            return false;
        }

        for (int32_t column = 0;
             column < width;
             column++)
        {
            // BMP speichert 24-Bit-Pixel als Blau, Grün, Rot.
            const uint8_t blue =
                rowBuffer[column * 3 + 0];

            const uint8_t green =
                rowBuffer[column * 3 + 1];

            const uint8_t red =
                rowBuffer[column * 3 + 2];

            const uint16_t color565 =
                guiTft.color565(
                    red,
                    green,
                    blue);

            guiTft.pushColor(color565);
        }
    }

    guiTft.endWrite();
    bmp.close();

    Serial.printf(
        "[GUI] BMP geladen: %s (%ldx%ld)\n",
        path,
        static_cast<long>(width),
        static_cast<long>(height));

    return true;
}