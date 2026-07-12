# Changelog

Alle nennenswerten Änderungen an der ESP32-Firmware (CYD 2432S028R Lok-Fahrregler)
werden in dieser Datei dokumentiert.

Format: [Keep a Changelog](https://keepachangelog.com/),
Versionierung: [SemVer](https://semver.org/).
Jede Version trägt ein Git-Tag (`vX.Y.Z`) und verweist auf Branch/Commit, damit
FW_VERSION, Tag und Log jederzeit nachvollziehbar sind.

---

## [Unreleased]

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
