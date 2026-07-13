// ============================================================================
// logic.h — Fahrregler-Geschaftslogik (Business-Layer, kein UI/IO)
// ----------------------------------------------------------------------------
// Dieses Modul haelt den Zustand des Lokreglers (Geschwindigkeit, Richtung,
// Funktionen, Adresse) und wendet Touch-Eingaben auf diesen Zustand an.
// Es ist bewusst frei von jeglichem TFT-/WebSocket-Code: Die GUI zeichnet
// nur, was hier im State steht, und die Comm-Schicht sendet nur, was hier
// als "muss gesendet werden" markiert wird.
//
// Ablauf im Programm:
//   - logicBegin() initialisiert Demo-Startzustaende (setup).
//   - logicApplyTouch(px, py) wertet einen kalibrierten Touch-Punkt aus und
//     aendert ggf. den State (z. B. Funktion umschalten, Zieltempo setzen).
//     Rueckgabe: true wenn sich etwas geaendert hat (-> GUI neu zeichnen,
//     falls noetig) und ein "dirty"-Flag fuer die Comm-Schicht.
//   - Die WebSocket-Callbacks (comm.cpp) schreiben ueber logicSet*() den
//     vom Gateway gemeldeten Zustand zurueck in den State.
// ============================================================================

#ifndef LOGIC_H
#define LOGIC_H

#include <Arduino.h>
#include "types.h"

// ---- Fahrregler-Basiszustaende (im Logic-Modul zentral verwaltet) ---------
extern int       logicSpeed;        //!< Aktuelle Ist-Geschwindigkeit 0..99
extern int       logicTargetSpeed;  //!< Ziel-Geschwindigkeit 0..99
extern uint8_t   logicAddress;      //!< Aktive Lokadresse (1..255)
extern bool      logicLightOn;      //!< F0 / Licht an?
extern Direction logicDirection;    //!< Fahrtrichtung
extern bool      logicOnline;       //!< Gateway-Verbindung steht?
extern EncoderMode encoderMode;     //!< Aktueller Drehregler-Modus

// Sichtbare Funktionen in den 8 Faechern (2 Spalten x 4 Reihen)
extern uint8_t   logicVisibleFunctions[8];
// Alle 16 Funktionskonfigurationen (Index 0 = F1 ... 15 = F16)
extern FunctionConfig logicFunctions[16];

// "Dirty"-Flags: was muss an das Gateway gesendet werden?
extern bool logicDirtyDrive;        //!< Geschwindigkeit/Richtung aenderung offen
extern bool logicDirtySelect;       //!< Lokwechsel offen
extern bool logicEmergencyStopRequested;  //!< STOP-Taster: emergency_stop senden

/**
 * @brief Wendet ein Encoder-Ereignis auf den Fahrregler-State an.
 *
 * @param ev  Encoder-Ereignis
 * @return true wenn eine Aenderung ansteht, die gesendet/gezeichnet werden soll
 */
bool logicApplyEncoder(const EncoderEvent& ev);

/**
 * @brief Initialisiert den Logic-Zustand mit Demo-Werten.
 * @param keine
 * @return void
 * @note Setzt F1+F11 aktiv und Standardadresse aus config.h (GW_ADDRESS).
 */
void logicBegin();

/**
 * @brief Wertet einen kalibrierten Touch-Punkt (Display-Pixel) aus.
 *
 * @param px  X in [0, 239]
 * @param py  Y in [0, 319]
 * @return true wenn eine Aktion ausgeloest wurde (State geaendert),
 *         false bei Touch ins "Leere".
 * @note Implementiert alle Button-Regionen (Licht, Adresse, Funktionen,
 *       Vor/Rueck/Gas/Stop, Throttle-Slide). Setzt ggf. dirty-Flags, die
 *       main.cpp/comm.cpp abarbeiten. Zeichnet NICHT selbst (nur State).
 */
bool logicApplyTouch(int16_t px, int16_t py);

/**
 * @brief Uebernimmt einen vom Gateway gemeldeten Lok-Status.
 *
 * @param addr  Lokadresse des Status
 * @param speed Ist-Geschwindigkeit 0..99
 * @param dir   "forward" / "reverse"
 * @param fnStates  Array der Laenge 16 mit Funktionszustaenden (true=an)
 * @return void
 * @note Adoptiert Adressen, die der Raspi (kuenftige Multiprotokoll-Zentrale)
 *       vorgibt — die UI folgt fremden Adress-/Status-Vorgaben (Single Source
 *       of Truth liegt beim Raspi). Speed/Funktionen/Direction werden 1:1
 *       uebernommen.
 */
void logicSetState(uint8_t addr, int speed, const char* dir,
                   const bool fnStates[16]);

/**
 * @brief Setzt die Online-Kennung (Gateway-Verbindung).
 * @param online  true = verbunden
 * @return void
 */
void logicSetOnline(bool online);

#endif // LOGIC_H
