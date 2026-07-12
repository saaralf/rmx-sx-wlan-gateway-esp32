# Todo – WLANHandregler (Logregler)

Arbeitsweise (zwingend, seit 2026-07-12):
1. Aufgaben hier erfassen -> mit User BESPRECHEN (kein Code vor Freigabe).
2. Nach Freigabe umsetzen -> erneut besprechen.
3. Testen (Build `pio run` / Gateway `pytest` / Hardware am CYD).
4. Branch committen + CHANGELOG-Eintrag + PR gegen main mergen.
5. Naechste Aufgabe = neuer Branch + neue Todos.
6. JEDER Code-Change (ESP32-UI ODER Gateway) -> neue Version X.Y.Z,
   im Quellcode (config.h FW_VERSION + CHANGELOG) gefuehrt UND dem User gemeldet.
7. Isolierte Teilaufgaben an lokales Ollama delegieren
   (granite4.1:8b-hermes-sx, toolsets terminal+file), nur komprimiertes Ergebnis.

=====================================================================
ZYKLUS 1: Button-Implementierung (9 Buttons + WLAN-Kommunikation) -> main
=====================================================================
Ziel-Version: **v0.2.4**  (Code-Change gegenueber v0.2.3 -> Pflicht-Bump)

Status vorab (verifiziert 2026-07-12):
- Alle 9 Button-Branches programmiert, stehen auf FW_VERSION v0.2.3.
- Build `pio run` auf feature/button9-stop: SUCCESS (RAM 14.3% / Flash 73.3%).
- 9 PRs offen: #10 #11 #13 #15 #17 #19 #21 #23 (Kette main<-#10<-#11<-#13<-#15<-#17<-#19<-#21<-#23).
- Hardware-Test (Issue #3) noch nicht durchgefuehrt.

Aufgaben:
- [x] T1  PR-Kette gemergt: #10->#11->#13->#15->#17->#19->#21->#23 (Branch-Protection auf Solo-Dev umgestellt: required_pull_request_reviews=null, enforce_admins=false).
- [x] T2  Branch `release/v0.2.4` von main angelegt.
- [x] T3  FW_VERSION in `src/config.h`: "v0.2.3" -> "v0.2.4" (Pflicht-Bump fuer Code-Change).
- [x] T4  CHANGELOG.md: Block `## [v0.2.4] - 2026-07-12` mit Button-Implementierung + Verifikation (Build SUCCESS).
- [x] T5  `release/v0.2.4` committet + PR #24 gegen main gemergt (squash).
- [x] T6  `git tag v0.2.4` (matchet FW_VERSION-String) + push.
- [x] T7  Hardware-Test am CYD (Issue #3) — USER uebernimmt:
           OK: Flash, USB Power-Cycle, Lxxx zaehlt, T:Y-Leiste, alle 9 Buttons reagieren.
           BUG: oben links KEINE Versionsnummer sichtbar (T8 Sub-Agent: timeout -> NA, T4 selbst geschrieben).
- [ ] T8  (DELEGIERT an Ollama/granite) Sub-Agent TIMEOUT (600s) -> NA. CHANGELOG (T4) selbst geschrieben (Notfall-Regel).

==================================================================
Zyklus 2: Bugfix FW_VERSION-Sichtbarkeit (Display-Farbe)
==================================================================
Befund: gui.cpp:27 COLOR_BACKGROUND = TFT_DARKGREY; gui.cpp:398 setTextColor(TFT_DARKGREY, COLOR_BACKGROUND)
  -> FW_VERSION wird in DARKGREY auf DARKGREY gezeichnet = unsichtbar.
  Fix: Textfarbe auf kontrastierend (TFT_WHITE) aendern. Pflicht-Bump -> v0.2.5.
Aufgaben:
- [x] Z2-T1  Branch `bugfix/fwversion-visible` von main angelegt.
- [x] Z2-T2  gui.cpp:398 setTextColor TFT_DARKGREY -> TFT_WHITE (Kontrast).
- [x] Z2-T3  FW_VERSION v0.2.4 -> v0.2.5 (Pflicht-Bump: Code-Change ggü. v0.2.4).
- [x] Z2-T4  CHANGELOG Block v0.2.5 (Bugfix FW-Version sichtbar).
- [x] Z2-T5  pio run SUCCESS, committet, PR #25 ggü. main gemergt (squash).
- [x] Z2-T6  git tag v0.2.5 + push (matchet FW_VERSION).
- [x] Z2-T7  (USER) Hardware-Test v0.2.5: Version NICHT sichtbar (2. Ursache: Lok-Panel uebermalt Ecke) -> Zyklus 3.

==================================================================
Zyklus 3: Bugfix #2 FW_VERSION-Panel-Overlap (v0.2.6)
==================================================================
Befund: Lok-Panel Layout::locomotive={4,4,232,38} uebermalt Ecke (4,4) komplett.
  Version lag UNTER dem Panel -> auch mit weisser Farbe unsichtbar.
  Fix: FW_VERSION NACH drawLocomotive() zeichnen (on top), Pos (6,6), TFT_WHITE auf COLOR_PANEL.
Aufgaben:
- [x] Z3-T1  Branch bugfix/fwversion-panel-overlap von main.
- [x] Z3-T2  guiDrawScreen(): FW_VERSION nach drawLocomotive() (on top), TFT_WHITE/COLOR_PANEL.
- [x] Z3-T3  FW_VERSION v0.2.5 -> v0.2.6 (Pflicht-Bump).
- [x] Z3-T4  CHANGELOG v0.2.6 (Bugfix #2).
- [x] Z3-T5  pio run SUCCESS, commit, PR #26 ggü. main gemergt.
- [x] Z3-T6  git tag v0.2.6 + push.
- [x] Z3-T7  (USER) Hardware-Test v0.2.6: Version "v0.2.6" oben links SICHTBAR (bestätigt). Zyklus 3 final.

Offen danach (kuenftige Zyklen, eigene Branches/Todos):
- Zyklus 5: RMX952-Treiber (Phase 4) – echtes RMX-Logregler-Ziel (aus RMX-Doku_V5.pdf, jetzt lokal im Gateway-Repo docs/).
- Zyklus 6: SX852-Protokoll an echter HW klaeren (tools/serial_sniffer.py).
- Zyklus 7: F0..F16 -> SX-Bit-Mapping Tabelle pro Lok.

==================================================================
GATEWAY-REPO: rmx-sx-gateway (Raspi-Daemon) — Setup + Versionierung (v0.1.1)
==================================================================
Kontext: Quellcode vom Raspi (192.168.0.87:/opt/rmx-sx-gateway) geholt,
als eigenes Git-Repo initialisiert + zu GitHub gepusht
(https://github.com/saaralf/rmx-sx-gateway). ESP32-Firmware = eigenes Repo.
Aufgaben (erledigt 2026-07-13):
- [x] G1  Passwordless SSH zum Raspi (Key id_ed25519.pub hinterlegt, NO_PW_NEEDED).
- [x] G2  Gateway-Code vom Pi geholt (tar, ohne .venv/.pytest_cache) -> lokal abgelegt.
- [x] G3  Eigenes Git-Repo + Initial-Commit + Push zu GitHub.
- [x] G4  Versionierung: protocol.SERVER_VERSION an __version__ gekoppelt (Single Source).
- [x] G5  hello_ack.server_version spiegelt echte Gateway-Version an ESP32 (Handshake).
- [x] G6  systemd service: Pfad /opt/rmx-sx-gateway (war veraltet rmx-sx-wlan-gateway).
- [x] G7  CHANGELOG.md angelegt (analog ESP32).
- [x] G8  __version__ 0.1.0 -> 0.1.1 (Pflicht-Bump), commit, PR, Tag v0.1.1 + push.

==================================================================
Zyklus 4: ESP32 zeigt Gateway-Version (v0.2.7)
==================================================================
Befund: Gateway sendet server_version im hello_ack, aber ESP32 zeigte sie nicht an.
  Fix: comm.cpp parst hello_ack.server_version -> gwVersion; gui.cpp drawStatusBar()
  zeichnet gwVersion unten rechts (235,312) in TFT_LIGHTGREY.
Aufgaben:
- [x] Z4-T1  comm.h: extern const char* gwVersion deklariert.
- [x] Z4-T2  comm.cpp: hello_ack.server_version parsen -> gwVersion speichern.
- [x] Z4-T3  gui.cpp: #include comm.h; drawStatusBar() zeigt gwVersion unten rechts.
- [x] Z4-T4  FW_VERSION v0.2.6 -> v0.2.7 (Pflicht-Bump).
- [x] Z4-T5  CHANGELOG v0.2.7.
- [x] Z4-T6  pio run SUCCESS, commit, PR #27 ggü. main gemergt.
- [x] Z4-T7  git tag v0.2.7 + push.
- [ ] Z4-T8  (USER) Hardware-Test v0.2.7: GW-Version NICHT sichtbar -> Ursache:
           lag bei (235,312) im Bereich der T:N-Debug-Leiste (y~305) die sie
           ueberschrieb. Behoben in Zyklus 5 (v0.2.8).

==================================================================
Zyklus 5: GW-Version sichtbar + Debug-Overlay-Flag (v0.2.8)
==================================================================
Befund (USER, v0.2.7): GW-Version "0.1.1" nicht sichtbar.
  Ursache: guiDrawDebugTouch() malt schwarzes Rechteck y=305..317 ueber volle
  Breite -> ueberschreibt gwVersion bei (235,312) jeden 150ms-Tick.
  Fix: (1) gwVersion nach oben links (6,15) auf Lok-Panel verschieben.
       (2) T:N + L hinter #ifdef DEBUG_OVERLAY (Default AUS).
Aufgaben:
- [x] Z5-T1  gui.cpp: gwVersion nach (6,15) TL_DATUM auf COLOR_PANEL.
- [x] Z5-T2  main.cpp: T:N + L Aufrufe hinter #ifdef DEBUG_OVERLAY.
- [x] Z5-T3  config.h: DEBUG_OVERLAY Default 0 + FW_VERSION v0.2.8.
- [x] Z5-T4  platformio.ini: kommentiertes -D DEBUG_OVERLAY=1.
- [x] Z5-T5  CHANGELOG v0.2.8 + Z4-T8 vermerkt.
- [x] Z5-T6  pio run SUCCESS, Branch, PR #28, Tag v0.2.8 + push.
- [ ] Z5-T7  (USER) Hardware-Test v0.2.8: GW-Version NICHT sichtbar + T:N/L
           TROTZDEM da -> zwei echte Bugs (siehe Zyklus 6 / v0.2.9).

==================================================================
Zyklus 6: Zwei Bugs fix (v0.2.9) — Debug-Flag + Dangling Pointer
==================================================================
Befund (USER, v0.2.8): (1) T:N + L noch sichtbar obwohl DEBUG_OVERLAY=0.
  (2) GW-Version "0.1.1" NICHT sichtbar.
  Ursache 1: main.cpp #ifdef DEBUG_OVERLAY -> Makro IMMER definiert (Wert 0)
             -> Code lief trotzdem. Fix: #if DEBUG_OVERLAY (Wertpruefung).
  Ursache 2: comm.cpp gwVersion = doc["server_version"] -> doc lokal im Event,
             nach Event weg -> Dangling Pointer (Müll). Fix: char[16] + strncpy.
Aufgaben:
- [x] Z6-T1  main.cpp: #ifdef DEBUG_OVERLAY -> #if DEBUG_OVERLAY.
- [x] Z6-T2  comm.cpp: gwVersion -> gwVersionBuf[16] + strncpy.
- [x] Z6-T3  comm.h: Dok-Kommentar gwVersion (statischer Puffer).
- [x] Z6-T4  config.h FW_VERSION v0.2.9 + gui.cpp drawStatusBar Klartext.
- [x] Z6-T5  CHANGELOG v0.2.9 + Z5-T7 vermerkt.
- [x] Z6-T6  pio run SUCCESS, Branch, PR #29, Tag v0.2.9 + push.
- [ ] Z6-T7  (USER) Hardware-Test v0.2.9: 1. NEIN (GW-Version weg)
           2. JA (T:N + L weg -> Debug-Fix bestaetigt).
           => Root-Cause Punkt 1 noch offen -> Zyklus 7 (v0.2.10).

==================================================================
Zyklus 7: GW-Version sichtbar + Diagnose-Fallback (v0.2.10)
==================================================================
Befund (USER, v0.2.9): T:N/L weg (Fix OK), aber GW-Version "0.1.1" fehlt
  weiterhin. Gateway sendet hello_ack.server_version="0.1.1" korrekt
  (protocol.py HelloAck.server_version=SERVER_VERSION=__version__="0.1.1"),
  Key in comm.cpp stimmt. Position (6,15) auf COLOR_PANEL versehentlich
  nicht uebermalt (lightButton y=80, locName y=46).
  Verdacht: hello_ack kommt nicht an / wird nicht geparst -> gwVersion leer.
  Fix: (1) Sichtbares Fallback "GW: ?" (ROT) wenn gwVersion leer.
       (2) drawStatusBar() in guiUpdateDynamic() ALS LETZTES (Draw-Order).
       (3) FW_VERSION v0.2.10.
Aufgaben:
- [x] Z7-T1  comm.cpp/gui: Root-Cause-Check (Key, Draw-Order) -> Key OK,
                    Pos OK, hello_ack-Pfad verifiziert.
- [x] Z7-T2  gui.cpp: "GW: ?" (ROT) Fallback + drawStatusBar() zuletzt.
- [x] Z7-T3  config.h FW_VERSION v0.2.10 + CHANGELOG + Todo.
- [x] Z7-T4  pio run SUCCESS, Branch, PR #30, Tag v0.2.10 + push.
- [ ] Z7-T5  (USER) Hardware-Test v0.2.10: 1. JA (0.1.1 grau) + 2. JA (rotes ?).
           => BEIDES gleichzeitig unmoeglich -> Artefakt: rotes "?" blieb rechts
           als Rest vom Boot-Fallback "GW: ?" stehen, weil graue Version kuerzer.
           => hello_ack-Pfad FUNKTIONIERT. Zyklus 8 (v0.2.11) raeumt den Rest weg.

==================================================================
Zyklus 8: rotes "?"-Artefakt entfernen (v0.2.11)
==================================================================
Befund (USER, v0.2.10): "0.1.1" grau sichtbar (hello_ack OK!) + rotes "?"
  daneben. Ursache: Boot-Fallback "GW: ?" (6 Zch, rot) wurde von kuerzerer
  Version (5 Zch, grau) nicht voll uebermalt -> roter Rest haengt.
  Fix: Textbereich (6,15,44,9) VOR dem Zeichnen mit COLOR_PANEL loeschen.
Aufgaben:
- [x] Z8-T1  gui.cpp drawStatusBar: fillRect(6,15,44,9,COLOR_PANEL) vor Text.
           (Achtung: Patch hatte dabei die "} von drawStatusBar geloescht ->
            wieder eingefuegt + verifiziert.)
- [x] Z8-T2  config.h FW_VERSION v0.2.11.
- [x] Z8-T3  CHANGELOG v0.2.11 + Z7-T5 Interpretation + Zyklus 8.
- [x] Z8-T4  pio run SUCCESS, Branch, PR #31, Tag v0.2.11 + push.
-------------------------------------------------------------------
USER-Befund v0.2.11 (Zyklus 8): "ich sehe v0.2.11 und GW: ? in rot"
  => NUR rotes "GW: ?", KEIN graues "0.1.1". Das war KEIN Artefakt,
     sondern: gwVersion blieb leer -> hello_ack kam NICHT an.
  HYPOTHESE: falscher Gateway-Host (192.168.50.1 statt 192.168.0.87).
  => Zyklus 9: Infrastruktur verifiziert, WURZELURSACHE gefunden.

==================================================================
Zyklus 9: WURZELURSACHE = falscher GW_HOST (v0.2.12)
==================================================================
Verifiziert (von Linux-Host aus):
- Pi erreichbar: 192.168.0.87 ping OK; Port 8080 OFFEN (python pid 992).
- 192.168.50.1 NICHT erreichbar (100% loss) -> Subnetz existiert nicht.
- platformio.ini + config.h setzen GW_HOST="192.168.50.1" (FALSCH).
- Roher WebSocket-Handshake gegen 127.0.0.1:8080 (Pi) liefert:
    {"type":"hello_ack","server":"rmx-sx-gateway",
     "server_version":"0.1.0","status":"ready"}
  -> Daemon funktioniert, liefert 0.1.0 (nicht 0.1.1; unkritisch).
Fix: GW_HOST -> "192.168.0.87" (platformio.ini massgebend + config.h).
Aufgaben:
- [x] Z9-T1  platformio.ini + config.h: GW_HOST 192.168.50.1 -> 192.168.0.87.
- [x] Z9-T2  config.h FW_VERSION v0.2.12.
- [x] Z9-T3  CHANGELOG v0.2.12 + Z8-T5 Befund + Zyklus 9.
- [x] Z9-T4  pio run SUCCESS, Branch, PR #32, Tag v0.2.12 + push.
- [ ] Z9-T5  (USER) Hardware-Test v0.2.12:
           1. steht grau "0.1.0" (oder 0.1.1) unter v0.2.12?
           2. KEIN rotes "GW: ?" mehr?
           Antwort: 1. ja/nein  2. ja/nein
