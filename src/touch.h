// ============================================================================
// touch.h — Touchscreen-Treiber (XPT2046) API-Deklaration
// ----------------------------------------------------------------------------
// Das Touch-Modul kapselt den resisitiven XPT2046-Controller des CYD.
// Es nutzt BIT-BANG (kein Hardware-SPI), weil das 2.8" CYD den XPT2046 auf
// eigenen GPIOs (25/32/39/33/36) hat, die nicht mit dem Display-SPI
// zusammenliegen. Der Code ist an PacoMouseCYD angelehnt (bewaehrt).
//
// Ablauf im Programm:
//   - touchBegin() wird EINMAL in setup() aufgerufen (Pin-Init).
//   - loop() ruft regelmaessig touchSample() auf; das fuellt ggf. die
//     Auto-Kalibrierungsgrenzen und liefert den aktuellen Punkt zurueck.
//   - touchIsPressed() liefert true, solange genug Druck anliegt.
//   - touchGetCalibrated() mappt Rohwerte auf Display-Pixel (0..239, 0..319).
// ============================================================================

#ifndef TOUCH_H
#define TOUCH_H

#include <Arduino.h>
#include "types.h"
#include "config.h"

/**
 * @brief Initialisiert die Touch-Pins (Bit-Bang) und interne Zustaende.
 *
 * @param keine
 * @return void
 * @note Muss vor allen anderen touch*-Aufrufen in setup() erfolgen.
 *       Setzt die GPIOs als Output/Input und zieht CS auf HIGH (inaktiv).
 */
void touchBegin();

/**
 * @brief Liest den XPT2046 einmal aus (Bit-Bang) und aktualisiert die
 *        internen Rohwerte + Auto-Kalibrierungsgrenzen.
 *
 * @param[out] out  Zeiger auf TSPoint, der mit Roh-x/y/z gefuellt wird.
 * @return void
 * @note Hat eine ~4ms-Ratebegrenzung (MSEC_THRESHOLD), um den Bus zu
 *       entlasten. Aktualisiert bei Bedarf xMin/xMax/yMin/yMax.
 */
void touchSample(TSPoint* out);

/**
 * @brief Prueft, ob aktuell genug Druck auf dem Panel liegt.
 *
 * @param keine
 * @return true wenn Z (Druckstaerke) > TS_Z_THRESHOLD, sonst false.
 * @note Ruft intern touchSample() auf, daher nicht zusaetzlich sample() rufen.
 */
bool touchIsPressed();

/**
 * @brief Liefert die aktuell kalibrierten Display-Koordinaten.
 *
 * @param[out] px  Zeiger auf int16_t, erhaelt x in [0, screenWidth-1]
 * @param[out] py  Zeiger auf int16_t, erhaelt y in [0, screenHeight-1]
 * @return void
 * @note Nur sinnvoll, wenn kurz zuvor touchIsPressed()==true war.
 *       Nutzt die zur Laufzeit gelernten Min/Max-Grenzen (Auto-Kalibrierung).
 */
void touchGetCalibrated(int16_t* px, int16_t* py);

/**
 * @brief Edge-Detection (PacoMouseCYD-Prinzip): liefert genau EINMAL
 *        true, wenn ein neuer Finger-Down erkannt wurde.
 *
 * @param[out] px  Zeiger auf int16_t, erhaelt den *gemittelten* x-Wert
 * @param[out] py  Zeiger auf int16_t, erhaelt den *gemittelten* y-Wert
 * @return true genau beim Uebergang released->pressed; dann sind px/py gueltig.
 * @note Verhindert, dass ein gehaltener Finger (oder Sampleschwankung)
 *       mehrfach ausgewertet wird ("Nachbartaste schaltet mit").
 *       Die Koordinate wird intern ueber mehrere Samples gemittelt, um
 *       Rauschen des resistiven Panels zu glaetten.
 *       Erst beim naechsten Loslassen (released) wird wieder scharfgeschaltet.
 */
bool touchGetTap(int16_t* px, int16_t* py);

/**
 * @brief Gibt die aktuell gelernten Auto-Kalibrierungsgrenzen zurueck.
 *
 * @param[out] xMin,xMax,yMin,yMax  Roh-Grenzen des Panels
 * @return void
 * @note Nuetzlich fuer Debug/Anzeige der Kalibrierungsguete.
 */
void touchGetCalibration(uint16_t* xMin, uint16_t* xMax,
                         uint16_t* yMin, uint16_t* yMax);

#endif // TOUCH_H
