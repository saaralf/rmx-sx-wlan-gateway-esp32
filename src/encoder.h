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
 * @brief Liefert nur Dreh-Impulse ohne Taster-Events.
 *
 * @return Anzahl Schritte seit letztem Aufruf, 0 = keine Drehung
 * @note SW wird NICHT ausgewertet. Trennung: Rotation vs. Taster.
 */
int32_t encoderPollSteps();

/**
 * @brief Liefert nur Taster-Events, ohne Drehung.
 *
 * @param pressed   true bei gerade gedrueckt
 * @param released  true bei gerade losgelassen
 * @param longPress true bei gehalten >= ENC_SW_LONG_MS
 * @return true wenn eines der Ereignisse eingetreten ist
 */
bool encoderPollSw(bool* pressed, bool* released, bool* longPress);

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
