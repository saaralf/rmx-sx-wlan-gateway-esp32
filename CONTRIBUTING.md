# Entwicklungs-Workflow (CONTRIBUTING)

Gültig ab Firmware v0.2.3. Ziel: saubere, nachvollziehbare Entwicklerarbeit
mit Issue-Tracking, Feature-Branches und Code-Review über Pull-Requests.

## 1. Branch-Strategie
- `main` — **stabil, geschützt**. Nur über Pull-Request, nie direkt pushen.
- `master` — historische Baseline (v0.1.12 Monolith). Wird nicht mehr
  weiterentwickelt, nur als Referenz erhalten.
- Feature-/Fix-Branches werden von `main` abgezweigt:
  - `feature/<kuerzel>` — neue Funktion (z.B. `feature/oled-status`)
  - `fix/<kuerzel>` — Fehlerbehebung (z.B. `fix/touch-ghost-tap`)
  - `release/vX.Y.Z` — Version-Bump + Changelog (am Ende eines Zyklus)

## 2. Pro Änderung: Issue → Branch → PR
1. **Issue anlegen** (`gh issue create`), das Problem beschreiben:
   - Symptom / Beobachtung (z.B. Touch liefert Y-Verschiebung)
   - Betroffener Bereich (Modul, ggf. FW_VERSION)
   - Akzeptanzkriterium (wie verifiziere ich „gelöst"?)
2. **Branch erstellen** vom aktuellen `main`:
   `git checkout main && git pull && git checkout -b fix/<kuerzel>`
3. **Ändern + committen** (kleinschrittig, aussagekräftige Messages auf Deutsch).
4. **FW_VERSION bumpen** in `src/config.h`, wenn funktionaler Test ansteht
   (Vorgabe: Version oben links am Display + im Boot-Log sichtbar).
5. **PR öffnen** gegen `main` (`gh pr create`), Issue verlinken.
6. **Review/Merge**: nach Freigabe `gh pr merge --squash` (saubere History)
   oder `--merge` (erhält alle Commits). Danach Branch löschen.

## 3. Versionierung & Tags
- SemVer: `MAJOR.MINOR.PATCH`
  - PATCH = Bugfix ohne API-Änderung
  - MINOR = neue Funktion rückwärtskompatibel
  - MAJOR = Breaking Change
- Jede Version: `git tag vX.Y.Z`, Eintrag in `CHANGELOG.md` (Keep-a-Changelog).
- Tag und FW_VERSION-String MÜSSEN übereinstimmen.

## 4. Verifikation vor Merge
- `pio run` muss fehlerfrei bauen (RAM/Flash unkritisch).
- Touch-Änderungen: mind. ein `calib`-Lauf mit plausiblen Eck-Rohwerten +
  `Probe:`-Zeile im Seriell-Log (Restfehler ≤ 2 px).
- NVS-relevante Änderungen: Reboot-Test, dass Kalib aus NVS geladen wird.

## 5. Remote & Push
- Upstream = GitHub-Repo `saaralf/rmx-sx-wlan-gateway-esp32` (SSH).
- Nie direkt auf `main` pushen. Immer über PR.
