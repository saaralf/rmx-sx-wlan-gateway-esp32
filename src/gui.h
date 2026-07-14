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
// Layout (alle Koordinaten in Display-Pixeln, Rotation 0: 240x320)
// ============================================================================
namespace Layout
{
    constexpr int16_t screenWidth = 240;
    constexpr int16_t screenHeight = 320;

    constexpr int16_t margin = 4;
    constexpr int16_t gap = 4;

    constexpr Rect locomotive = {4, 4, 232, 38};
    constexpr Rect locomotiveName = {4, 46, 202, 30};
    constexpr Rect locomotiveDropDown = {209, 46, 27, 30};

    constexpr int16_t sideButtonW = 64;
    constexpr int16_t sideButtonH = 32;

    constexpr int16_t leftColumnX = 4;
    constexpr int16_t rightColumnX = 172;

    constexpr Rect lightButton = {leftColumnX, 80, sideButtonW, sideButtonH};
    constexpr Rect addressSelector = {rightColumnX, 80, sideButtonW, sideButtonH};
    constexpr Rect speedDisplay = {72, 80, 96, 32};

    constexpr int16_t functionStartY = 120;
    constexpr int16_t functionButtonW = sideButtonW;
    constexpr int16_t functionButtonH = sideButtonH;
    constexpr int16_t functionGap = 4;

    constexpr int16_t functionLeftX = leftColumnX;
    constexpr int16_t functionRightX = rightColumnX;

    constexpr Rect throttle = {76, 120, 38, 140};
    constexpr Rect speedGauge = {126, 120, 38, 140};

    constexpr Rect accelerateButton = {4, 268, 55, 48};
    constexpr Rect reverseButton = {63, 268, 55, 48};
    constexpr Rect forwardButton = {122, 268, 55, 48};
    constexpr Rect emergencyButton = {181, 268, 55, 48};
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
 *       TFT-init"), dann guiTft.init() + setRotation(0). Haengt init()
 *       (vermuteter Grund fuer weissen Schirm), sieht man NACH den 2 Blinks
 *       weiterhin WEISS. Gelingt init(), wird kurz "BOOT" (rot) gezeigt.
 */
void guiInitDisplay();

/**
 * @brief Zeigt eine Boot-Phase als Vollbild in einer Farbe mit Text.
 * @param color  Hintergrundfarbe (TFT_*-Konstante)
 * @param msg    Anzuzeigender Phasen-Text (z. B. "TOUCH", "LOGIC", "UI")
 * @return void
 * @note Setzt ein bereits initialisiertes TFT voraus (guiInitDisplay()).
 */
void guiBootPhase(uint16_t color, const char *msg);

/**
 * @brief Setzt die Display-Rotation (0=hoch, 1=waagerecht, 2, 3).
 * @param r  Rotationswert (0..3)
 * @return void
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

// Umbaue GUI verbessern: MKL
/**
 * Aktualisiert Geschwindigkeit, Zielgeschwindigkeit,
 * Slider und Ist-Geschwindigkeitsbalken.
 */
void guiUpdateSpeed();

/**
 * Aktualisiert Adresse und Loknamenanzeige.
 */
void guiUpdateAddress();

/**
 * Aktualisiert Licht und Funktionstasten.
 */
void guiUpdateFunctions();

/**
 * Aktualisiert Online-Status, Gateway-Version
 * und Encoder-Modusanzeige.
 */
void guiUpdateConnectionStatus();

/**
 * Erzwingt beim nächsten Update ein vollständiges
 * Neuzeichnen aller dynamischen Bereiche.
 */
void guiInvalidateDynamic();

// Umbaue GUI verbessern: MKL
/**
 * @brief Zeigt die Touch-Diagnose-Leiste unten am Bildschirm an.
 * @param touched  true wenn aktuell gedrueckt
 * @param rx,ry    kalibrierte Roh-X/Y (0..4095)
 * @param mx,my    gemappte Display-X/Y (0..239 / 0..319)
 * @return void
 * @note Dient der visuellen Verifikation der Touch-Koordinaten.
 */
/**
 * @brief Aktualisiert eine Encoder-Modus-Anzeige im Status-/Head-Bereich.
 * @param mode  0 = Speed, 1 = Adresse
 * @return void
 * @note Wird von main.cpp bei Moduswechsel aufgerufen.
 */
void guiDrawEncoderMode(uint8_t mode);

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
void touchDrawCalibCross(int16_t x, int16_t y, uint16_t color);



#endif // GUI_H
