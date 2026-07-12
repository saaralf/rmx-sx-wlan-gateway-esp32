// ============================================================================
// comm.h — Gateway-Kommunikation (WebSocket-Client)
// ----------------------------------------------------------------------------
// Kapselt die gesamte Netzwerk-Kommunikation mit dem rmx-sx-wlan-gateway
// Daemon (WebSocket, Protokoll: docs/websocket-protocol.md). Dieses Modul
// kennt den Logic-State nur ueber die uebergebenen Callback-Zeiger; es
// veraendert selbst keine globalen Zustaende direkt (entkoppelt).
//
// Ablauf im Programm:
//   - commBegin(callbacks) in setup(): WiFi + WS initialisieren, Handler
//     registrieren. Die Callbacks verdrahten eingehende Nachrichten mit
//     dem Logic-Layer (siehe comm.cpp).
//   - commLoop() in loop(): WS-Ping + Reconnect + Heartbeat.
//   - commSend*(): von main.cpp nach einer Logic-Aenderung aufgerufen, um
//     drive / function / select / emergency_stop an den Daemon zu senden.
// ============================================================================

#ifndef COMM_H
#define COMM_H

#include <Arduino.h>

// Callback-Typen: entkoppeln Comm von Logic
typedef void (*CbState)(uint8_t addr, int speed, const char* dir,
                        const bool fnStates[16]);
typedef void (*CbOnline)(bool online);
typedef void (*CbRedraw)(void);   // GUI neu zeichnen (z. B. nach hello_ack)

struct CommCallbacks
{
    CbState  onState;   //!< Gateway meldet Lok-Status
    CbOnline onOnline;  //!< Verbindungsstatus aenderung
    CbRedraw onRedraw;  //!< Statusleiste/screen neu zeichnen
};

/**
 * @brief Initialisiert WiFi + WebSocket und registriert Callbacks.
 * @param cb  Struktur mit den drei Verdrahtungs-Callbacks.
 * @return void
 * @note Muss in setup() NACH logicBegin() aufgerufen werden.
 */
void commBegin(const CommCallbacks& cb);

/**
 * @brief Regelmaessiger WebSocket-Dienst (ping, reconnect).
 * @param keine
 * @return void
 * @note Aus loop() aufrufen (moeglichst oft).
 */
void commLoop();

/**
 * @brief Sendet Geschwindigkeit + Richtung an die aktive Lok.
 * @param address  Lokadresse
 * @param speed    0..99
 * @param dir      "forward" / "reverse"
 * @return void
 */
void commSendDrive(uint8_t address, int speed, const char* dir);

/**
 * @brief Schaltet eine Lokfunktion (F0..F16).
 * @param address  Lokadresse
 * @param fn       Funktionsnummer (0 = Licht, 1..16)
 * @param state    true = ein
 * @return void
 */
void commSendFunction(uint8_t address, int fn, bool state);

/**
 * @brief Waehlt eine andere Lok im Gateway aus.
 * @param address  Lokadresse
 * @return void
 */
void commSendSelectLoco(uint8_t address);

/**
 * @brief Fragt den aktuellen Status der Lok vom Gateway an.
 * @param address  Lokadresse
 * @return void
 */
void commSendRequestState(uint8_t address);

/**
 * @brief Sendet Nothalt (Geschwindigkeit sofort 0).
 * @param keine
 * @return void
 */
void commSendEmergencyStop();

#endif // COMM_H
