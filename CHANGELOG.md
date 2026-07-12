# Changelog

Alle nennenswerten Änderungen an der ESP32-Firmware (CYD 2432S028R Lok-Fahrregler)
werden in dieser Datei dokumentiert.

Format: [Keep a Changelog](https://keepachangelog.com/),
Versionierung: [SemVer](https://semver.org/).
Jede Version trägt ein Git-Tag (`vX.Y.Z`) und verweist auf Branch/Commit, damit
FW_VERSION, Tag und Log jederzeit nachvollziehbar sind.

---

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
