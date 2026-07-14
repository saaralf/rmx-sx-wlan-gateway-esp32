# Changelog

Alle nennenswerten Änderungen an der ESP32-Firmware (CYD 2432S028R Lok-Fahrregler)
werden in dieser Datei dokumentiert.

Format: [Keep a Changelog](https://keepachangelog.com/),
Versionierung: [SemVer](https://semver.org/).
Jede Version trägt ein Git-Tag (`vX.Y.Z`) und verweist auf Branch/Commit, damit
FW_VERSION, Tag und Log jederzeit nachvollziehbar sind.

## [v0.2.25] - 2026-07-14

### Hinzugefuegt (SD-Karte und Lokbild aus BMP-Datei)

* **MicroSD-Unterstuetzung fuer das CYD 2432S028R eingebaut.**
* **`src/sdcard.h/.cpp`:** Neues Modul fuer:

  * Initialisierung einer FAT/FAT32-SD-Karte
  * Erkennung des Kartentyps
  * Ausgabe von Kartengroesse, belegtem und freiem Speicher
  * rekursive Auflistung von Dateien und Verzeichnissen
  * zentrale Statusabfrage ueber `sdCardReady()`
* **SD-Pinbelegung:**

  * SCLK = IO18
  * MISO = IO19
  * MOSI = IO23
  * CS = IO5
* **`platformio.ini`:** TFT_eSPI mit `USE_HSPI_PORT=1` auf HSPI gelegt.
  Die SD-Karte verwendet separat VSPI, damit Display und SD-Karte sich
  nicht gegenseitig umkonfigurieren.
* **`src/gui.cpp`:** Loader fuer unkomprimierte 24-Bit-BMP-Dateien ergaenzt.

  * BMP-Header und Format werden geprueft.
  * RGB888-Pixel werden beim Zeichnen nach RGB565 konvertiert.
  * BMP-Zeilenpadding und Bottom-up-Bildreihenfolge werden beruecksichtigt.
  * Bild wird zeilenweise direkt von der SD-Karte auf das TFT uebertragen.
* Lokbild wird aus folgender Datei geladen:
  `/loks/diesel.bmp`
* Erwartetes Uebungsformat des Lokbildes:

  * 96 x 30 Pixel
  * 24 Bit RGB
  * unkomprimiertes BMP
* Falls SD-Karte oder BMP-Datei fehlen, wird weiterhin automatisch die bisherige
  programmatisch gezeichnete gelb-blaue Lok als Fallback angezeigt.
* SD-Kartenfehler blockieren den Fahrregler nicht; Display, Encoder, WLAN und
  Gateway-Kommunikation koennen ohne SD-Karte weiterlaufen.

### Geaendert (GUI-Aktualisierung und Encoder-Steuerung)

* **GUI-Aktualisierung auf einzelne Bereiche aufgeteilt**, damit nicht mehr bei
  jeder Statusaenderung der komplette dynamische Bildschirmbereich geloescht
  und neu gezeichnet wird.
* Geschwindigkeitsregler, Statusanzeige, Funktionstasten und Adressbereich
  werden nur noch aktualisiert, wenn sich der jeweilige Wert geaendert hat.
* Doppelte beziehungsweise unnoetige GUI-Neuzeichnungen nach empfangenen
  Gateway-Statusmeldungen reduziert.
* Encoder-Polling aus dem langsameren Touch-Zeitfenster herausgeloest.
  Der Encoder wird jetzt unabhaengig und regelmaessiger in der Hauptschleife
  abgefragt.
* Ein Rastschritt des Encoders entspricht einem Geschwindigkeitswert.
* Lokale Geschwindigkeitsaenderungen werden fuer eine kurze Zeit gegen
  verzoegerte Gateway-Rueckmeldungen geschuetzt.
  Dadurch springt der Sollwert nicht mehr auf einen alten Wert zurueck.

### Behoben (Encoder-Rueckspruenge und unbeabsichtigte Lokauswahl)

* Zeitbasierter Encoder-Rate-Limiter entfernt, weil dadurch schnelle
  Quadraturwechsel verloren gingen.
* Quadraturauswertung verwendet einen Phasenakkumulator und wertet erst einen
  vollstaendigen Rastschritt als Bewegung.
* Rueckwaertsspruenge beziehungsweise ungleichmaessige Bewegungen des
  Geschwindigkeitsreglers reduziert.
* **Standardfokus des Encoders ist jetzt immer SPEED.**
* Adress- beziehungsweise Lokauswahl per Encoder wird nur aktiviert, nachdem
  das entsprechende Auswahlfeld auf dem Touchscreen beruehrt wurde.
* Adressfokus wird nach einer kurzen Inaktivitaet automatisch beendet.
  Danach steuert der Encoder wieder die Geschwindigkeit.
* Kurzer Druck auf den Encoder-Taster wechselt nicht mehr unbeabsichtigt
  zwischen Geschwindigkeits- und Adressmodus.
* Interner Encodermodus und sichtbare Modusanzeige werden synchron gehalten.
* Hinweis zur Hardware: Der Encoder-Taster wurde bereits auf IO4 verschoben,
  da IO35 beim klassischen ESP32 keinen internen Pull-up besitzt.

### Technik / Verifikation

* SD-Karte muss FAT oder FAT32 formatiert sein.

* Erwartete Dateistruktur:

  ```
  /
  └── loks
      └── diesel.bmp
  ```

* Serielle Diagnoseausgaben fuer SD-Initialisierung, Verzeichnisinhalt,
  fehlende Dateien und nicht unterstuetzte BMP-Formate ergaenzt.

* BMP-Hilfsfunktionen bleiben intern in `src/gui.cpp` und werden nicht in
  `gui.h` exportiert.

* SD-Karte wird vor `guiBegin()` initialisiert, damit das Lokbild bereits beim
  ersten vollstaendigen Bildschirmaufbau verfuegbar ist.

* Hardwaretest mit mehreren SD-Karten und laengerem Parallelbetrieb von
  TFT, Encoder, WLAN und Gateway-Kommunikation steht noch aus.

* **FW_VERSION:** v0.2.24 -> v0.2.25.



## [v0.2.24] - 2026-07-13
### Behoben (Encoder-SW-Pin)
- **ENC_SW von IO35 auf IO4 geaendert**, weil IO35 auf dem CYD als
  Digitaleingang nicht stabil ist und dauerhaft flackert.

## [v0.2.23] - 2026-07-13
### Behoben (Encoder nach PacoMouseCYD-Prinzip)
- **`src/encoder.cpp`:**
  - SW LOW-aktiv mit Pegelwechsel-Erkennung wie PacoMouseCYD `readButtons()`
  - Encoder-Step-Guarding: nur A- oder B-Aenderung ohne gleichzeitige
    Gegenphase wird als gueltiger Schritt gewertet
  - Rate-Limit `ENC_MIN_EVENT_MS` bleibt erhalten
- **FW_VERSION:** v0.2.22 -> v0.2.23

## [v0.2.22] - 2026-07-13
### Behoben (Encoder-Taster polaritaet)
- **Pull-up nach 3.3V:** Taster LOW-aktiv auswerten.
- **SW-Kurz:** toggelt Modus S/A.
- **SW-Lang:** sendet **nur** `emergency_stop`; keine Blockade der GUI/Loop.
- API auf Rotation (`encoderPollSteps()`) und Taster (`encoderPollSw()`) getrennt.

## [v0.2.21] - 2026-07-13
### Behoben (Encoder-Prellen, Ruecksprünge, Modus nicht sichtbar)
- **`src/encoder.cpp`:** Rate-Limiter `ENC_MIN_EVENT_MS=35` + saubere
  4-Phasen-Quadratur-Auswertung. Ungültige Phasenwechsel werden verworfen,
  aber als neuer Ausgangspunkt genommen, damit der Encoder nicht hängt.
- **`src/gui.cpp`:** Modus-Anzeige direkt neben der Gateway-Version:
  "S" = Speed-Modus, "A" = Adress-Modus. Keine Debug-Overlay-Abhängigkeit mehr.
- **`src/gui.h`:** Doppelte deklaration für `guiDrawEncoderMode()` entfernt.
- **`src/config.h`:** `ENC_MIN_EVENT_MS` ergänzt.
- **FW_VERSION:** v0.2.20 -> v0.2.21.

## [v0.2.20] - 2026-07-13
### Hinzugefuegt (Rotary Encoder CLK/DT/SW auf CN1/P3)
- **Hardware:** EC11/KY-040 angeschlossen an IO22/IO27/IO35
  (CLK/Dual-Step-Quadratur + SW als Taster).
- **`src/encoder.h/.cpp`:** Neues Modul fuer Polling, 4-Phasen-Quadratur-Auswertung,
  SW-Edge-Detection, Entprellung und Longpress.
- **`src/types.h`:** `EncoderMode` + `EncoderEvent` ergaenzt.
- **`src/logic.h/.cpp`:** `encoderMode`, `logicApplyEncoder()`
  - kurzer SW-Druck: Moduswechsel SPEED <-> ADDRESS
  - lange SW-Druck: Notstopp
  - Drehung: +2/-2 Speed im Speed-Modus, +1/-1 Adresse im Adress-Modus
- **`src/main.cpp`:** Encoder-Polling neben Touch; Sendepflege ueber bestehende
  dirty-Flags, kein neuer Websocket-Pfad notwendig.
- **Touch bleibt erhalten**, Rotation/Taster sind parallel zum Touch laid out.
- **FW_VERSION:** v0.2.19 -> v0.2.20.

## [v0.2.19] - 2026-07-13
### Geändert (Standard-UI zurück auf SENKRECHT, horizontaler Stand gesichert)
- **Entscheidung (USER):** Senkrechte UI (Rotation 0, 240x320) sieht besser aus
  als die waagerechte (Rotation 1, 320x240). Standard ist wieder senkrecht.
- **Sicherung waagerechtes Layout:** Vollständiger horizontaler Stand
  (Lokname volle Breite, Licht+Adresse neben Speed, F-Tasten 2x4 links/rechts,
  zwei senkrechte Balken zentral, Schrift verkleinert) als Commit
  `c109371` auf Branch `feature/horizontal-ui` gesichert + Hermes-Skill
  `wlanhandregler-horizontal-ui` (für späteres Wiederaufnehmen).
- **Boot-Debug in Senkrechte zurückportiert:** `guiInitDisplay()` +
  `guiBootPhase()` + `guiSetRotation()` aus dem waagerechten Stand in die
  senkrechte `gui.cpp`/`gui.h` übernommen (verhindert weißen/schwarzen Schirm,
  2x BL-Blink + roter "BOOT"-Screen als Lebenszeichen).
- **touch.cpp:** Wieder senkrechte Display-Masse (240x320) — der waagerechte
  Clamp (320x240) aus dem horizontalen Stand wäre in der Senkrechten falsch.
- **FW_VERSION:** v0.2.18 -> v0.2.19.

## [v0.2.18] - 2026-07-13
### Behoben (schwarzer Schirm — guiInitDisplay() wurde nie aufgerufen)
- **Befund (USER, nach v0.2.17):** Display schwarz (Backlight aus, keine UI).
- **Ursache:** Branch-Split in `gui.cpp` entkoppelte `guiBegin()` vom TFT-Init.
  `guiBegin()` machte nur noch `guiDrawScreen()` (kein Init, kein Backlight an).
  Das eigentliche Init (`pinMode(TFT_BL,OUTPUT)`, 2x-Blink, `guiTft.init()`,
  roter BOOT-Screen) steckte in `guiInitDisplay()` — und wurde **nirgends**
  aufgerufen. `main.cpp` rief nur `guiBegin()` → Backlight blieb aus → schwarz.
- **Fix:** `main.cpp setup()` ruft `guiInitDisplay()` GANZ AM ANFANG (vor
  touchBegin/logicBegin), danach `guiBegin()` (gemäß `gui.h`-Vertrag).
  Veraltete Ablauf-Doku in `main.cpp` korrigiert.
- **Verifiziert:** Build SUCCESS, geflasht; User bestätigt echte UI
  (v0.2.18 oben links) + 2x Backlight-Blink + roter BOOT-Screen vor UI.
- **FW_VERSION:** v0.2.17 → v0.2.18 (Pflicht-Bump).

## [v0.2.17] - 2026-07-13
### Behoben (WURZELURSACHE: weisser Schirm — build_flags-Override in platformio.local.ini)
- **Befund (USER):** Display bleibt weiss (nur Backlight an), UI fehlt.
- **Ursache:** `platformio.local.ini` enthielt eine eigene `build_flags =`-Zeile
  (fuer WIFI_PASSWORD). PlatformIO haengt `extra_configs` NICHT an `build_flags`
  an, sondern **ersetzt** sie. Dadurch fielen ALLE TFT_eSPI-Pin-Defines
  (TFT_MISO, ILI9341_DRIVER, …) aus dem Build → TFT_eSPI lief auf internen
  Default-SPI-Pins → falsche Pins → Panel bekam keine gueltigen Pixel →
  **weisser Schirm**. Core-unabhaengig (Core 2 wie Core 3 betroffen).
- **Fix:**
  - `platformio.local.ini`: statt `build_flags =` jetzt eigene Sektion
    `[wlanpass]` mit `password = …`. Ueberschreibt die Haupt-`build_flags`
    (TFT-Pins!) nicht mehr.
  - `platformio.ini`: Passwort wird am Ende der bestehenden `build_flags` als
    `-D WIFI_PASSWORD="${wlanpass.password}"` sicher interpoliert →
    TFT-Pins **und** WLAN-Passwort gelangen beide in den Build
    (verifiziert per `pio run -v`).
  - `src/config.h`: FW_VERSION v0.2.16 → v0.2.17 (Pflicht-Bump).
- **Verifiziert:** verbose-Build zeigt alle `-D TFT_*` + `WIFI_PASSWORD`;
  Farbtest (Rot→Gruen→Blau) am Panel sichtbar; echte UI (v0.2.17) bootet,
  kein weisser Schirm mehr.
- **HINWEIS:** Niemals in `platformio.local.ini` erneut `build_flags =`
  schreiben — das tilgt die TFT-Pins. Immer eigene Sektion oder Anhängen.

## [v0.2.12] - 2026-07-13
### Behoben (WURZELURSACHE: falscher Gateway-Host)
- **Befund (USER, v0.2.11):** Display zeigt nur rotes "GW: ?", KEIN graues
  "0.1.1". Ursache war NICHT die Anzeige, sondern die Netzverbindung:
  `GW_HOST` war auf "192.168.50.1" gesetzt.
- **KORREKTUR (WIDERRUF v0.2.12):** v0.2.12 aenderte GW_HOST auf
  "192.168.0.87" (Pi eth0) — das war FALSCH. Der ESP haengt am Pi-AP
  (wlan0) im Subnetz 192.168.50.x (ESP-IP laut Daemon-Log 192.168.50.199).
  Vom ESP-Subnetz aus ist 192.168.0.87 nicht routbar -> weiter rotes "GW: ?".
  RICHTIG ist die Pi-AP-Adresse **192.168.50.1** (dhcpcd.conf static
  ip_address=192.168.50.1/24; dnsmasq dhcp-range 192.168.50.100-200).
- **Verifiziert (Pi-Log /var/log/rmx-sx-gateway/rmx-sx-gateway.log):**
  `client registered client: fahrregler-01 fw: v0.2.3` von IP 192.168.50.199
  + `hello`. Daemon liefert hello_ack mit server_version 0.1.0 (OK).
- **Fix v0.2.13:** `GW_HOST` zurueck auf "192.168.50.1"
  (platformio.ini massgebend + config.h).
- `config.h` FW_VERSION v0.2.12 -> v0.2.13 (Pflicht-Bump).
### Technik / Verifikation
- `pio run` SUCCESS. GW_HOST 192.168.50.1 (Pi-AP wlan0, ESP-Subnetz).

## [v0.2.13] - 2026-07-13
### Behoben (GW_HOST-Korrektur: Pi-AP-Adresse 192.168.50.1)
- WIDERRUF v0.2.12: GW_HOST 192.168.0.87 war falsch (ESP-Subnetz
  192.168.50.x kann 192.168.0.87 eth0 nicht routen).
- Richtig: Pi-AP (wlan0) = 192.168.50.1 (dhcpcd.conf static / dnsmasq).
- ESP-IP laut Daemon-Log 192.168.50.199 -> GW_HOST 192.168.50.1 erreichbar.
- config.h FW_VERSION v0.2.12 -> v0.2.13 (Pflicht-Bump).

## [v0.2.11] - 2026-07-13
### Behoben (rotes "?"-Artefakt nach hello_ack)
- **Befund (USER, v0.2.10):** Nach hello_ack wurde grau "0.1.1" gezeigt,
  ABER rechts davon blieb ein rotes "?" stehen. Ursache: das Boot-Fallback
  "GW: ?" (6 Zeichen, rot) wurde durch die kuerzere Version (5 Zeichen, grau)
  nicht vollstaendig uebermalt -> roter "?"-Rest blieb haengen.
- **Fix:** Vor dem Text wird der Bereich (6,15,44,9) mit COLOR_PANEL
  geloescht, damit keine Reste der vorherigen Variante stehen bleiben.
  Damit: hello_ack da -> nur "0.1.1" (grau); nicht da -> nur "GW: ?" (rot).
- **Kern-Erkenntnis:** Der hello_ack-Pfad FUNKTIONIERT (v0.2.10). Die
  Gateway-Version wurde also schon seit v0.2.10 korrekt empfangen/gezeigt.
- `config.h` FW_VERSION v0.2.10 -> v0.2.11 (Pflicht-Bump).
### Technik / Verifikation
- `pio run` SUCCESS. Funktions-Abschluss von drawStatusBar() verifiziert.

## [v0.2.10] - 2026-07-13
### Behoben (GW-Version Sichtbarkeit + Diagnose-Fallback)
- **Sichtbares Fallback:** `drawStatusBar()` zeigt bei leerem `gwVersion`
  (noch KEIN hello_ack empfangen — z.B. Gateway nicht erreichbar) nun
  **"GW: ?" in ROT** statt gar nichts. So ist am Display erkennbar, ob der
  WebSocket-Handshake fehlschlaegt, OHNE Serial-Log. Bei erfolgreichem
  `hello_ack` steht die echte Version (z.B. "0.1.1") in Grau.
- **Draw-Order gesichert:** `drawStatusBar()` läuft in `guiUpdateDynamic()`
  jetzt ALS LETZTES (nach `drawLightButton`/`drawFunctionColumns`), damit
  die GW-Version-Ecke (6,15) nicht uebermalt wird.
- Position (6,15) auf COLOR_PANEL verifiziert: kein anderes Panel-Element
  ueberlappt diese Stelle (lightButton y=80, locName y=46, Panel bis y=42).
- `config.h` FW_VERSION v0.2.9 -> v0.2.10 (Pflicht-Bump).
### Hinweis (Root-Cause offen)
- Bei v0.2.9 war GW-Version weiterhin nicht sichtbar, T:N/L aber weg (Fix
  bestaetigt). Ursache hierfuer noch nicht 100% klar: entweder kommt
  `hello_ack` nicht an, oder `server_version` wird nicht gesetzt. v0.2.10
  liefert durch "GW: ?" die Diagnose am Display. pio run SUCCESS.

## [v0.2.9] - 2026-07-13
### Behoben (zwei echte Bugs aus Hardware-Test v0.2.8)
- **Bug 1 — Debug-Overlay lief trotzdem:** `main.cpp` nutzte `#ifdef DEBUG_OVERLAY`.
  Aber `config.h` definiert das Makro MIT Wert 0 → `#ifdef`=TRUE → T:N + L
  kompilierten immer rein. Fix: `#if DEBUG_OVERLAY` (Wertpruefung).
  Bestaetigt durch USER: T:N + L waren bei v0.2.8 noch sichtbar.
- **Bug 2 — GW-Version Dangling Pointer:** `comm.cpp` speicherte
  `gwVersion = doc["server_version"]` — `doc` ist lokal im Event, nach dem
  Event weg → `gwVersion` zeigte auf freigegebenen Speicher (Müll, nichts
  sichtbar). Fix: statischer `char gwVersionBuf[16]` + `strncpy`. comm.h:
  `gwVersion` bleibt `const char*` (zeigt auf Puffer).
- `config.h` FW_VERSION v0.2.8 -> v0.2.9 (Pflicht-Bump).
### Technik / Verifikation
- `pio run` SUCCESS. Static Analysis: gwVersionBuf lives for program duration.

## [v0.2.8] - 2026-07-13
### Behoben (Gateway-Version sichtbar) + Debug-Flag
- **Bugfix:** Gateway-Version (aus `hello_ack.server_version`) lag bei (235,312)
  — genau im Bereich der `T:N`-Debug-Leiste (y~305), die sie bei jedem 150ms-Tick
  ueberschrieb. Neu: GW-Version oben links UNTER FW_VERSION (6,15) auf dem
  Lok-Panel (COLOR_PANEL) — nie vom Debug-Bar betroffen.
- **Debug-Overlay flaggbar:** `T:N` (Touch-Leiste) + `L` (Loop-Counter) in
  `main.cpp` jetzt hinter `#ifdef DEBUG_OVERLAY`. Per Default AUS (sauberes
  Produktivbild). Einschalten via `platformio.ini`: `-D DEBUG_OVERLAY=1`.
  `DEBUG_OVERLAY` Default 0 in `config.h`.
- `config.h` FW_VERSION v0.2.7 -> v0.2.8 (Pflicht-Bump).
- Befund aus Hardware-Test v0.2.7 (USER): GW-Version nicht sichtbar -> durch
  T:N-Ueberlappung bestaetigt, hier behoben.
### Technik / Verifikation
- `pio run` SUCCESS.

## [v0.2.7] - 2026-07-13
### Feature (Gateway-Version im UI)
- **Gateway-Version unten rechts in der Statusleiste sichtbar:** `comm.cpp`
  parst `hello_ack.server_version` (vom Raspi-Gateway gesendet) und speichert
  es in `gwVersion`. `gui.cpp::drawStatusBar()` zeichnet die Version bei
  (235,312) rechtsbuendig in `TFT_LIGHTGREY` (grau auf Hintergrund).
- `comm.h`: `extern const char* gwVersion` deklariert.
- `gui.cpp`: `#include "comm.h"` ergaenzt.
### Technik / Verifikation
- `pio run` SUCCESS.
- Bump v0.2.6 -> v0.2.7 (Code-Change ggü. v0.2.6).

## [v0.2.6] - 2026-07-13
### Behoben (Display, Bugfix #2)
- **FW-Version oben links weiterhin unsichtbar (v0.2.5):** Das Lok-Panel `Layout::locomotive = {4,4,232,38}` uebermalt die Ecke (4,4) komplett — die weisse Version lag UNTER dem Panel.
  Fix: `guiDrawScreen()` zeichnet `FW_VERSION` JETZT NACH `drawLocomotive()` (on top), Position (6,6), Textfarbe `TFT_WHITE` auf `COLOR_PANEL` (Kontrast).
### Technik / Verifikation
- `pio run` SUCCESS.
- Bump v0.2.5 -> v0.2.6 (Code-Change ggü. v0.2.5).

## [v0.2.5] - 2026-07-13
### Behoben (Display, Bugfix)
- **FW-Version oben links sichtbar:** `gui.cpp` zeichnete `FW_VERSION` in `TFT_DARKGREY` auf `COLOR_BACKGROUND` (= ebenfalls `TFT_DARKGREY`) — identische Farbe, Text unsichtbar. Textfarbe auf `TFT_WHITE` geaendert (Kontrast).
### Technik / Verifikation
- `pio run` SUCCESS.
- Bump v0.2.4 -> v0.2.5 (Code-Change ggü. v0.2.4).

## [v0.2.4] - 2026-07-12
### Hinzugefügt (Button-Implementierung, Issue #8/#9/#12/#14/#16/#18/#20/#22)
- **Button 1 (Lok-Dropdown):** Adresse vom Raspi adoptieren (`logicSetState`), Lokname dynamisch (`"Lok <addr>"` statt Hartcode).
- **Button 2 (Licht F0):** F0 aus `functions["0"]` des Gateway-State übernehmen (Raspi→UI Sync).
- **Button 3 (Adress-Pfeile +/-):** Bereich 1..255 + Gateway-`select_loco` bei Adresswechsel.
- **Button 4 (Funktionsspalten F1..F16):** AUS-Toggle-Fix — alle 16 Funktionen explicit (true/false) an Raspi senden.
- **Button 5 (Throttle-Slider):** Absolut-Semantik — Slider-Position mappt direkt auf Ziel-Speed 0..99.
- **Button 6 (Gas +5):** Inkrement +5, Bereich 0..99.
- **Button 7/8 (Vor/Rück):** Direction FORWARD/REVERSE setzen + Raspi-Sync via `logicSetState`.
- **Button 9 (STOP):** `emergency_stop` NUR bei STOP-Taster (nicht bei jedem Speed=0).
- **WLAN-Kommunikation:** komplette Verdrahtung Touch→Logic→Comm→Raspi (WebSocket `drive`/`function`/`emergency_stop`/`select_loco`/`request_state` an Gateway Port 8080).
### Technik / Verifikation
- `pio run` SUCCESS (RAM 14.3 % / Flash 73.3 %).
- 9 PRs (#10/#11/#13/#15/#17/#19/#21/#23) als serielle Kette in `main` gemergt.
- Hinweis: physischer Hardware-Test am CYD (Issue #3) steht noch aus.

## [Unreleased]
### Infrastruktur (Pi-Gateway, Issue #4)
- **Verbindung ESP32↔Pi komplett**: Daemon (`rmx-sx-gateway`, Port 8080) + WLAN-AP
  (`Modellbahn-Fahrregler`, 192.168.50.1, DHCP .100-.200) auf dem Pi in Betrieb.
- Daemon-Start-Fix: `tmpfiles.d/rmx-sx-gateway.conf` (Reboot-festes `/run/rmx-sx-gateway`).
- AP-Fix: `rfkill unblock wifi` + dhcpcd-static-IP; neue `rfkill-unblock-wifi.service`
  (Before=hostapd) für Reboot-Festigkeit.
- **Verifiziert**: ESP32 erhält DHCP-Lease `192.168.50.199`, WS-`hello` beim Daemon
  registriert → Bidirektionale Verbindung steht.

## [v0.2.3] - 2026-07-12
### Geändert (Touch-Kalibrierung — Kernänderung)
- **Min/Max-Grenzen durch 2D-affine Abbildung ersetzt.** Die Kalibrierung
  lernt eine affine Matrix `px = a*rx + b*ry + c` / `py = d*rx + e*ry + f`
  per Least-Squares über 4 Eck-Messpunkte (Methode Adafruit/Bodmer).
- **Warum:** Das XPT2046-Panel ist zum Display um 90° verdreht. Min/Max-Grenzen
  können eine gedrehte/invertierte Achse nicht korrigieren → systematische
  Y-Verschiebung. Die affine Matrix bildet Roh→Pixel exakt (1–2 px Restfehler).
- Rotation in `touchUpdate()` entfernt: rohe XPT2046-Werte gehen **direkt** in
  die Matrix (die korrigiert Achslage/Rotation/Invertierung). Der alte Swap
  `_xraw = 4095 - ry` verfälschte die Rohwerte und war die Wurzel des
  `RX=4095`-Artefakts.
- NVS speichert jetzt 6 Matrix-Koeffizienten (`ma..mf`), nicht mehr
  `xmin/xmax/ymin/ymax`.
- Zielkreuze der Eck-Kalibrierung auf 15 % Inset verschoben
  `(36,48)(204,48)(204,272)(36,272)` — resistiver Panel misst exakt in den
  Ecken unzuverlässig (RX sprang auf 4095).
- `touchApplyMatrix()` bildet Roh→Pixel; `touchGetCalibrated()` nutzt sie.

### Technik / Verifikation
- Solver per Gauss-Elimination (3×3, mit Spaltenpivot), offline gegen
  `numpy.linalg.lstsq` verifiziert (2 px Genauigkeit an den Ecken).
- Alte NVS-Einträge (`xmin/..`) werden ignoriert (Valid-Check prüft die Matrix).
- Seriell-Log: `Probe: Ecke0 rx=665 ry=3266 -> px 36 py 50 (soll 36,48)` ✓
- Nach Reboot: `[CALIB] NVS geladen: a=0.0001 b=-0.0662 c=252.7 ...` ✓
- **FW_VERSION-String in `config.h` auf v0.2.3 nachgezogen** (Commit c3a75c7
  nannte v0.2.3, der String blieb aber fälschlich auf v0.2.2 — Boot-Log zeigte
  deshalb v0.2.2, obwohl bereits der affine Code lief).

## [v0.2.2] - 2026-07-11
### Geändert
- NVS-Kalib wurde in `touchBegin()` mit Defaults überschrieben (Wurzel der
  Y-Verschiebung) — behoben: gültige Matrix wird nicht mehr überschrieben.
- Auto-Kalib entfernt (Müll bei schwebendem Panel).
- Exakte Eck-Kalib (4 Ecken), 30 s Fenster, gelbe/grüne Ziel-Kreuze auf dem
  Display, Boot-Log der geladenen Werte.

## [v0.2.1] - 2026-07-10
### Geändert
- Echte Eck-Kalibrierung (NVS, seriell per `calib`), `map()`-Guard,
  Limits-Restore bei Abbruch. Behebt systematische Y-Verschiebung.

## [v0.2.0] - 2026-07-09
### Geändert
- Modulare C++-Architektur: `types/config/touch/logic/comm/gui/main`.
  Strenge Trennung GUI=nur Zeichnen, Logic=State, Comm=Netzwerk, Touch=Bit-Bang.
- Green-Bug (`fillScreen(GREEN)`) behoben: nur Debug-Leiste `T:Y` + Koordinaten.

## [v0.1.12] - (Baseline, Branch `master`)
### Initial
- Monolith `main.cpp`, Touch via Bit-Bang XPT2046 (working).

---

### Entwicklungs-Workflow (ab v0.2.3)
- `main` = stabile, geschützte Default-Branch. `master` bleibt als historische
  Baseline (v0.1.12 Monolith) erhalten.
- Jede Änderung läuft über: **GitHub-Issue** (Problem beschreiben) →
  **Branch** (`feature/...` oder `fix/...`, von `main` abgezweigt) →
  **Pull-Request** → Review/Merge nach `main`.
- FW_VERSION in `src/config.h` wird vor jedem funktionalen Test gebumppt
  (Vorgabe: Version sichtbar oben links am Display + im Boot-Log).
- Jede Version wird getaggt (`git tag vX.Y.Z`) und im CHANGELOG eingetragen.
