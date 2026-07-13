// ============================================================================
// gui.h — Graphical User Interface (TFT_eSPI Zeichen-Layer)
// ----------------------------------------------------------------------------
// Dieses Modul ist der EINZIGE Ort, der direkt auf das TFT (TFT_eSPI)
// zeichnet. Es liest den anzuzeigenden Zustand aus dem Logic-Layer
// (logic.h) und besitzt keine eigene Geschaeftslogik. Trennung:
//   Logic = WAS angezeigt wird  |  GUI = WIE es gezeichnet wird.
//
// Ablauf im Programm:
//   - guiBegin(tft) in setup(): Display-Init + einmaliges Vollbild.
//   - guiDrawScreen() zeichnet das komplette statische Layout neu.
//   - guiUpdateDynamic() zeichnet nur die sich aendernden Teile
//     (Speed-Digital, Throttle-Slide, Statuspunkt, Debug-Leiste).
//   - guiDrawDebugTouch() / guiDrawLoopCounter() fuer die Diagnose-Leiste.
//
// Die Layout-Konstanten (Rects) sind hier zentral definiert, damit Logic
// (Button-Hit-Tests) und GUI (Zeichnen) dieselbe Geometrie nutzen.
// ============================================================================

#ifndef GUI_H
#define GUI_H

#include <Arduino.h>
#include "types.h"
#include "config.h"

// Vorausdeklaration des TFT-Objekts (instanziiert in gui.cpp)
class TFT_eSPI;
extern TFT_eSPI guiTft;

// ============================================================================
// Layout (alle Koordinaten in Display-Pixeln, Rotation 1: 320x240 waagerecht)
// Eigenstaendiges waagerechtes Layout: F-Tasten links + rechts am Rand,
// zwei senkrechte Balken (Throttle + Speed-Gauge) zentral dazwischen,
// oben Lok-Panel + Name/Dropdown/Speed/Licht/Adresse, unten 4 Steuerbuttons.
// Kein fremder Stil — dieselben Rects, nur anders verteilt.
// ============================================================================
namespace Layout
{
    constexpr int16_t screenWidth  = 320;   // waagerecht (Rotation 1)
    constexpr int16_t screenHeight = 240;

    constexpr int16_t margin = 4;
    constexpr int16_t gap    = 4;

    // --- Oben: Lok-Panel durchgehend ---
    constexpr Rect locomotive        = { 4, 4, 312, 36 };

    // --- Zeile 2: Lokname ueber volle Breite ---
    constexpr Rect locomotiveName    = { 4, 44, 312, 24 };
    constexpr Rect locomotiveDropDown= { 4, 44, 312, 24 };  // Dropdown im Namen integriert (unsichtbar)

    // --- Zeile 3: Licht (links), blaue Speed-Digital (zentral), Adresse (rechts) ---
    constexpr Rect lightButton     = { 4, 72, 88, 24 };
    constexpr Rect speedDisplay      = { 96, 72, 140, 24 };
    constexpr Rect addressSelector = { 235, 72, 81, 24 };

    // --- Mitte: zwei senkrechte Balken (Throttle + Speed-Gauge) zentral ---
    // Zwischen linker F-Spalte (x=4,w=88 -> bis 92) und rechter F-Spalte
    // (x=228): freier Bereich 92..228. Beide Balken darin zentriert.
    constexpr Rect throttle   = { 140, 100, 36, 86 };
    constexpr Rect speedGauge = { 190, 100, 36, 86 };

    // --- Links + rechts: je 4 Reihen Funktionen (F1..F8), sichtbar ---
    constexpr int16_t functionStartY   = 100;
    constexpr int16_t functionButtonW  = 88;
    constexpr int16_t functionButtonH  = 20;
    constexpr int16_t functionGap      = 2;
    constexpr int16_t functionLeftX    = 4;
    constexpr int16_t functionRightX   = 228;   // 2. Spalte (rechts)

    // --- Unten waagerecht: 4 Steuerbuttons nebeneinander ---
    constexpr Rect accelerateButton = { 4,   196, 74, 40 };
    constexpr Rect reverseButton    = { 82,  196, 74, 40 };
    constexpr Rect forwardButton    = { 160, 196, 74, 40 };
    constexpr Rect emergencyButton  = { 238, 196, 78, 40 };
}

// ============================================================================
// Oeffentliche GUI-API
// ============================================================================

/**
 * @brief Initialisiert das Display und zeichnet das Vollbild einmalig.
 * @param keine
 * @return void
 * @note Muss in setup() NACH touchBegin(), aber VOR commBegin() stehen,
 *       da das Vollbild den Online-Statuspunkt (logicOnline) zeigt.
 */
void guiBegin();

/**
 * @brief Fruehes TFT-Init fuer visuelles Boot-Debugging.
 * @param keine
 * @return void
 * @note Muss GANZ AM ANFANG von setup() stehen (vor touchBegin/logicBegin).
 *       Schaltet Backlight an, blinkt 2x (Signal "setup erreicht, VOR
 *       TFT-init"), dann guiTft.init() + setRotation. Haengt init() (der
 *       vermutete Grund fuer den weissen Schirm), sieht man NACH den 2 Blinks
 *       weiterhin WEISS (Backlight an, kein fillScreen). Gelingt init(),
 *       wird kurz "BOOT" (rot) gezeigt. Danach kann guiBootPhase() Farben
 *       zeichnen, um die Boot-Phase sichtbar zu machen.
 */
void guiInitDisplay();

/**
 * @brief Zeigt eine Boot-Phase als Vollbild in einer Farbe mit Text.
 * @param color  Hintergrundfarbe (TFT_*-Konstante)
 * @param msg    Anzuzeigender Phasen-Text (z. B. "TOUCH", "LOGIC", "UI")
 * @return void
 * @note Setzt ein bereits initialisiertes TFT voraus (guiInitDisplay()).
 *       Wird in setup() zwischen den Init-Schritten aufgerufen, damit am
 *       Display sichtbar wird, bis wo der Boot kommt (statt weissem Schirm).
 */
void guiBootPhase(uint16_t color, const char* msg);

/**
 * @brief Setzt die Display-Rotation (0=hoch, 1=waagerecht, 2, 3).
 * @param r  Rotationswert (0..3)
 * @return void
 * @note Kapselt guiTft.setRotation(), damit main.cpp den TFT_eSPI-Typ nicht
 *       vollstaendig kennen muss. Nach dem Setzen Vollbild neu zeichnen.
 */
void guiSetRotation(uint8_t r);

/**
 * @brief Zeichnet das komplette statische Layout neu (Voll-Refresh).
 * @param keine
 * @return void
 * @note Nur bei grossen Aenderungen (z. B. Screenwechsel) aufrufen.
 */
void guiDrawScreen();

/**
 * @brief Zeichnet nur die dynamischen Elemente (Speed, Slider, Status,
 *        Debug-Leiste) neu, ohne das ganze Bild zu loeschen.
 * @param keine
 * @return void
 * @note Wird z. B. nach jeder Logic-Aenderung oder Statusmeldung aufgerufen.
 */
void guiUpdateDynamic();

/**
 * @brief Zeigt die Touch-Diagnose-Leiste unten am Bildschirm an.
 * @param touched  true wenn aktuell gedrueckt
 * @param rx,ry    kalibrierte Roh-X/Y (0..4095)
 * @param mx,my    gemappte Display-X/Y (0..239 / 0..319)
 * @return void
 * @note Dient der visuellen Verifikation der Touch-Koordinaten.
 */
void guiDrawDebugTouch(bool touched, int16_t rx, int16_t ry,
                      int16_t mx, int16_t my);

/**
 * @brief Zeigt den Loop-Zaehler oben rechts an (Laufzeit-Diagnose).
 * @param n  Anzahl der seit Start durchlaufenen loop()-Iterationen / 10.
 * @return void
 */
void guiDrawLoopCounter(uint32_t n);

/**
 * @brief Zeichnet ein kleines Kreuz + Kreis an eine Display-Ecke
 *        (waehrend der Touch-Kalibrierung).
 * @param x,y    Eck-Position in Pixeln (Rotation 0)
 * @param color  TFT_YELLOW = noch zu tippen, TFT_GREEN = erledigt
 * @return void
 * @note Wird von touch.cpp (Kalib-Modus) per extern-Callback gerufen.
 */
// Zeichen-Helfer (exportiert fuer wiederverwendende Module wie gui_horizontal.cpp)
void drawBeveledButton(int16_t x, int16_t y, int16_t w, int16_t h,
                       uint16_t fillColor, bool active = false);
void drawCenteredText(const String& text, int16_t centerX, int16_t centerY,
                      uint16_t color, uint8_t font = 2,
                      uint16_t background = 0x0120);  // TFT_TRANSPARENT
void drawLeftText(const String& text, int16_t x, int16_t centerY,
                  uint16_t color, uint8_t font = 2,
                  uint16_t background = 0x0120);  // TFT_TRANSPARENT
void drawArrowTriangle(int16_t centerX, int16_t centerY,
                       int16_t size, bool right, uint16_t color);
void drawLightBulb(int16_t x, int16_t y, bool active);

#endif // GUI_H