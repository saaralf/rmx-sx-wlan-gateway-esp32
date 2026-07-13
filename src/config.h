// ============================================================================
// config.h — Build-Konfiguration, Hardware-Pins, Firmware-Version
// ----------------------------------------------------------------------------
// Sammelt alle compile-time Konstanten: Gateway-Zugangsdaten (aus
// platformio.ini build_flags), Hardware-Pinbelegung des CYD, Firmware-
// Version und Touch-Kalibrierungs-Startwerte.
//
// Ablauf im Programm:
//   - platformio.ini liefert GW_HOST, GW_PORT, WIFI_*, GW_INTERFACE,
//     GW_BUS, GW_ADDRESS als -D Defines. Fallen diese weg, greifen hier
//     die Defaults (z. B. fuer lokales Kompilieren ohne build_flags).
//   - setup() (main.cpp) ruft die Pin-Initialisierung der Module auf;
//     die hier definierten Pins werden von touch.cpp verwendet.
// ============================================================================

#ifndef CONFIG_H
#define CONFIG_H

#ifndef DEBUG_OVERLAY
#define DEBUG_OVERLAY 0            //!< Debug-Overlay (T:N + L) per Default AUS
#endif

#include <Arduino.h>

// ---- Lokale WLAN-Credentials (gitignored) ---------------------------------
// WIFI_SSID + WIFI_PASSWORD werden per -D Build-Flag aus gitignored
// platformio.local.ini (extra_configs in platformio.ini) gesetzt. Quelle:
// ~/.hermes/.env WLANPASS, einheitlich mit dem Pi-AP. Fehlt platformio.local.ini
// (z.B. fremder Klon ohne Secret), greift der Platzhalter weiter unten.
// Die Datei wird NICHT committet (.gitignore), damit das Passwort nicht ins
// Repo gelangt. ACHTUNG: der Build-Flag heisst WIFI_PASSWORD (nicht WLAN_*)!

// ---- Firmware-Version ------------------------------------------------------
// Wird oben links auf dem Display gedruckt und sollte vor jedem funktionalen
// Firmware-Test gebumppt werden (Vorgabe im Projekt).
#define FW_VERSION "v0.2.20"

// ---- Gateway-Verbindung (ueberschreibbar via platformio.ini build_flags) ---
#ifndef GW_HOST
#define GW_HOST "192.168.50.1"     //!< IP des rmx-sx-wlan-gateway Daemons (Pi AP, wlan0)
#endif
#ifndef GW_PORT
#define GW_PORT 8080               //!< WebSocket-Port des Daemons
#endif
#ifndef WIFI_SSID
#define WIFI_SSID "Modellbahn-Fahrregler"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "CHANGE_ME_WIFI_PASSWORD"
#endif
#ifndef CLIENT_ID
#define CLIENT_ID "fahrregler-01"  //!< Eindeutige Client-Kennung fuer den Daemon
#endif
#ifndef GW_INTERFACE
#define GW_INTERFACE "sim1"        //!< Gateway-Interface (sim1 = RMX-Simulator)
#endif
#ifndef GW_BUS
#define GW_BUS "RMX0"              //!< SX-Bus-Selektion im Gateway
#endif
#ifndef GW_ADDRESS
#define GW_ADDRESS 110             //!< Standard-Lokadresse (BR 110)
#endif

// ---- Hardware-Pins des ESP32-2432S028R (Cheap Yellow Display) -------------
// Display (ILI9341 ueber TFT_eSPI / VSPI):
#define TFT_BL   21                //!< Backlight-Pin (HIGH = an)
// TFT_eSPI nutzt intern: CS=15, DC=2, MOSI=13, SCK=14, MISO=12 (in User_Setup.h)

// XPT2046 Touchscreen hat EIGENE Pins (nicht den Display-SPI!):
// Quelle: CYD-Pinout-Doku + PacoMouseCYD Referenz. Bewaehrt fuer 2.8" CYD.
#define TOUCH_CLK  25              //!< T_CLK  (Bit-Bang Takt)
#define TOUCH_MOSI 32              //!< T_DIN  (Bit-Bang MOSI)
#define TOUCH_MISO 39              //!< T_OUT  (Bit-Bang MISO)
#define TOUCH_CS   33              //!< T_CS   (Chip-Select, active LOW)
#define TOUCH_IRQ  36              //!< T_IRQ  (Touch-Interrupt, active LOW)

// ---- Drehregler (EC11/KY-040, CN1/P3) ----------------------------------
// Quelle: https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/PINS.md
//   CLK -> IO22, DT -> IO27, SW -> IO35
// SW ist Input Only ohne interne Pull-ups; das Modul sollte einen externen
// Pull-up haben, sonst muss ein 10k nach 3.3V ergänzt werden.
#define ENC_CLK             22                 //!< CLK an CN1/P3
#define ENC_DT              27                 //!< DT  an CN1
#define ENC_SW              35                 //!< SW  an P3 (Input Only)
#define ENC_DEBOUNCE_MS     5                  //!< Taster-Entprellung
#define ENC_SW_LONG_MS      500                //!< Grenze kurz vs. lang
#define ENC_STEPS_PER_CLICK 1                  //!< Encoder-Auflösung pro Raste

// ---- Touch-Kalibrierung (Startwerte, werden zur Laufzeit auto-kalibriert) -
// Grobe Roh-Grenzen fuer das 2.8" CYD Panel (0..4095). Werden beim Boot
// aus NVS geladen, falls eine manuelle "calib" durchgefuehrt wurde.
// Fallback-Startwerte: aus echten Eckmessungen (RX 205..3797 / RY 190..3769).
#define TS_MIN_X 200
#define TS_MAX_X 3800
#define TS_MIN_Y 190
#define TS_MAX_Y 3900
#define TS_Z_THRESHOLD 300         //!< Mindest-Druckstaerke fuer "gedrueckt"

#endif // CONFIG_H
