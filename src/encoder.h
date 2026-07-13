// ============================================================================
// encoder.h — EC11/KY-040 Drehregler API
// ----------------------------------------------------------------------------
// Kapselt Polling und Tasterentprellung fuer CLK/DT/SW.
//
// Ablauf im Programm:
//   - encoderBegin() einmal in setup() aufrufen.
//   - loop() ruft regelmaessig encoderPoll(&event) auf.
//   - Events werden in logicApplyEncoder(event) ausgewertet.
// ============================================================================

#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>
#include "types.h"

// ============================================================================
// Oeffentliche API
// ============================================================================

/**
 * @brief Initialisiert GPIOs fuer CLK/DT/SW.
 *
 * @note Muss in setup() vor dem ersten encoderPoll() stehen.
 */
void encoderBegin();

/**
 * @brief Liefert ein Encoder-Ereignis, falls eine Aktion erkannt wurde.
 *
 * @param out  Ausgabeereignis
 * @return true wenn ein neues Ereignis vorliegt
 *
 * @note Drehung liefert steps als +1/-1 pro definierter Raste.
 *       Tasterereignisse werden entprellt und in pressed/released/
 *       longPress aufgeteilt.
 */
bool encoderPoll(EncoderEvent* out);

/**
 * @brief Wechselt den aktuellen Encoder-Modus.
 *
 * @return neuer Modus nach Wechsel
 */
EncoderMode encoderToggleMode();

/**
 * @brief Gibt den aktuellen Modus zurueck.
 *
 * @return aktueller Encoder-Modus
 */
EncoderMode encoderCurrentMode();

#endif // ENCODER_H
