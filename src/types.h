// ============================================================================
// types.h — Allgemeine Datentypen und Grundstrukturen
// ----------------------------------------------------------------------------
// Enthaelt die reinen Datenstrukturen (POD), die von mehreren Modulen
// (GUI, Logic, Touch) gemeinsam genutzt werden. Bewusst KEIN Code, nur
// Typen + kurze Dokumentation, damit keine zyklischen Abhaengigkeiten
// zwischen den Modul-Headern entstehen.
// ============================================================================

#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

/**
 * @brief Rechteckige Bildschirmregion in Pixeln (Display-Koordinaten).
 *
 * Wird ueberall dort verwendet, wo UI-Elemente positioniert werden
 * (Buttons, Panels, Slider). Alle Werte in Display-Pixeln (0..239 x, 0..319 y).
 */
struct Rect
{
    int16_t x;   //!< Linke Kante (Pixel ab 0)
    int16_t y;   //!< Obere Kante (Pixel ab 0)
    int16_t w;   //!< Breite in Pixeln
    int16_t h;   //!< Hoehe in Pixeln
};

/**
 * @brief Konfiguration einer einzelnen Funktions-Taste (F1..F16).
 *
 * Repraesentiert den Schaltzustand einer Lokfunktion im Regler.
 * `functionNumber` ist 1-basiert (1 = F1, 16 = F16). F0 (Licht) wird
 * separat ueber `lightOn` gefuehrt.
 */
struct FunctionConfig
{
    uint8_t functionNumber;  //!< Funktionsnummer (1..16)
    bool    active;          //!< true = Funktion eingeschaltet
};

/**
 * @brief Ein einzelner resistiver Touch-Punkt (Rohdaten des XPT2046).
 *
 * Rohe 12-Bit-Werte (0..4095) plus Druckstaerke Z. Wird vom Touch-Treiber
 * gefuellt und ggf. vom Logic-Layer weiterverarbeitet.
 */
struct TSPoint
{
    uint16_t x;   //!< Roh-X (0..4095, nach Rotation)
    uint16_t y;   //!< Roh-Y (0..4095, nach Rotation)
    uint16_t z;   //!< Druckstaerke (Z-Differenz, je groesser desto fester)
};

/**
 * @brief Fahrtrichtung der Lok.
 */
enum class Direction : uint8_t
{
    FORWARD,   //!< Vorwaerts
    REVERSE    //!< Rueckwaerts
};

#endif // TYPES_H
