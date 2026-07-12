# SX-Referenzanalyse — github.com/michael71

**Zweck:** Hinweisgeber für das *serielle PC-Interface-Protokoll* von SX825/SX852.
Der Raspberry Pi erzeugt/decodiert den SX-Bus **nicht** direkt. Relevant ist nur
die Kommunikation **Computer → SX-Interface (USB/UART)**.

**Lizenz aller Fundstellen:** GPL-3.0 (siehe `sources-and-licenses.md`).
→ Quellcode wird **nur zum Verständnis** genutzt. Der Gateway-Code ist eigenständig
neu implementiert (MIT). Keine direkte Übernahme GPL-3.0-Quellcodes.

---

## 1. Geprüfte Repositories

| Repo | Beschreibung | Relevanz | URL |
|------|--------------|----------|-----|
| **SX3-PC** | PC-Control-Software für Selectrix (Java, RXTX). Enthält lesbaren Quellcode zu seriellen SX-Interfaces. | **Hoch** – dokumentiert SLX825/Rautenhaus-Format + FCC-Format | https://github.com/michael71/SX3-PC |
| **SX4** | Daemon zur Steuerung einer SX-Anlage (seriell) via TCP/IP. | Mittel – nur JAR+HTML-Docs auf GitHub, kein lesbarer Quellbaum | https://github.com/michael71/SX4 |
| LocoNetProjects | Arduino/LocoNet (opensx.net HW) | Niedrig – nicht SX-PC-Interface | — |
| Lanbahn-Spark | WLAN-Weichendecoder | Niedrig | — |

---

## 2. Relevante Dateien (SX3-PC, `src/de/blankedv/sx3pc/`)

| Datei | Inhalt |
|-------|--------|
| `SXInterface.java` | Generisches Interface: Trix-Format **und** Rautenhaus/SLX825-Format. Modus-Umschaltung, Power, Feedback-Parser. |
| `GenericSXInterface.java` | Abstrakt: `sendLoco()` (Fahrdaten-Bitlayout!), `sendAccessoryBit()`, `send2SX()` (SX0/SX1-Adressierung). |
| `SXFCCInterface.java` | FCC-Interface: **komplett anderes** Protokoll (230400 Baud, Polling). |
| `SXUtils.java` | Adress-/Bit-Validierung, SX-Bitlogik (1..8). |
| `SxAbit.java` | Adresse+Bit-Paar. |

---

## 3. Gefundene PC-Interface-Protokolle

### 3.1 SLX825 / Rautenhaus-Format (aus `SXInterface.java`, `GenericSXInterface.java`)

**Seriell:** 8N1, Baudrate variabel (im Constructor übergeben; typ. 57600 laut Projekt-Memory).
**Telegrammaufbau:** immer **2 Byte**.

| Richtung | Byte0 | Byte1 | Bedeutung |
|----------|-------|-------|-----------|
| Schreiben | `adr | 0x80` | `data` | Bit7 im Adressbyte = Schreiben |
| Lesen | `adr` | `0x00` | Bit7=0; Antwort (Trix-Format): nur Datenbyte |

**Interface-Modus (Rautenhaus/SLX825 einschalten)** — `setInterfaceMode()`:
```
Schreiben Adr 126 (0xFE): 0xFE, 0xA0
   Bit7 (128) = Überwachung Ein  (Rautenhaus-Befehlsformat)
   Bit5 (32)  = Feedback Ein
```
Danach sendet das Interface **automatisch** bei jeder Busänderung ein `[adr, data]`-Paar.
Adressbyte Bit7 kennzeichnet den Bus: `0 = SX0`, `1 = SX1` (SLX825 1-Bus: immer 0).

**Gleisspannung (Power)** — Adresse 127 (`0xFF`):
```
Power ON : 0xFF, 0x80
Power OFF: 0xFF, 0x00
Power lesen: 127, 0x00
```

**Lok-Schreiben** — `sendLoco(lok_adr, speed, licht, forward, horn)`:
```
Adressbyte = lok_adr | 0x80        (Loks immer auf SX0!)
Datenbyte:
   Bit0..4 = speed (0..31, auf 31 begrenzt)
   Bit5 (32)  = 1 → rückwärts (forward==false)
   Bit6 (64)  = 1 → Licht
   Bit7 (128) = 1 → Horn
```

**Funktionen / Zubehör-Bits** — `sendAccessoryBit(adr, bit, data)`:
```
Datenbyte = aktuelles Byte, Bit (1..8) setzen/löschen
Adressbyte = adr | 0x80
```
→ F0..F16 müssen auf die SX-Bits (1..8) und ggf. mehrere Kanäle abgebildet werden
(je nach Lok-Decoder-Protokoll). **Nicht** im SX3-PC-Code als F0..F16 definiert → eigene Abbildung im Gateway nötig.

**Adressbereich:** SX0 = 0..111, Power = 127. SX1 = 128..239 (Bit7 im Adressbyte).
(siehe `SXUtils.isValidSXAddress`: 0..111 oder 127; SXMAX=112)

### 3.2 FCC-Format (aus `SXFCCInterface.java`) — **NICHT** SX825/852

| Parameter | Wert |
|-----------|------|
| Baudrate | **230400 (fix)** |
| Telegramm | 3 Byte zum Schreiben: `[0x00, adr, data]` (+ 1 Byte Quittung = 0) |
| Poll | sende `[0x78, 0x03]`, erwarte **226 Byte** (SX0+SX1; Kanal 112 = Power: 0=aus) |
| Power ON | `[0x00, 0xFF, 0x01]` |
| Power OFF | `[0x00, 0xFF, 0x00]` |

⚠️ **Widerspruch im Quellcode:** Kommentar sagt „höchstes Bit auf 1 setzen",
der Code sendet aber `[0x00, adr, data]`. → **OFFEN / unzuverlässig**, nicht für Gateway übernehmen.

---

## 4. Bekannte Gerätezuordnung

| Gerät | Protokoll | Status |
|-------|-----------|--------|
| **SLX825 / SX825** | Rautenhaus-Format (§3.1) | ✅ dokumentiert (SX3-PC) |
| **FCC** (unbekanntes Interface) | FCC-Format (§3.2) | ⚠️ anderes Protokoll, nicht SX825/852 |
| **SX852** | — | ❌ **OFFEN** – in SX3-PC nicht abgedeckt |
| **SX4 (SLX825Interface)** | Rautenhaus-Format (vermutet, aus JAR) | ⚠️ nur als kompiliertes JAR vorhanden |

---

## 5. Offene Fragen / Unsicherheiten

1. **SX852-Protokoll:** Keine Quelle in github.com/michael71. Vermutlich eigener
   UART-Befehlssatz (Rautenhaus XP-Bus / „SX852AD"?). → **Muss an echter Hardware
   geklärt werden** (serieller Sniffer, nur dokumentierte/lesende Befehle).
2. **F0..F16-Abbildung:** SX3-PC nutzt nur SX-Bits 1..8. Die Funktionsgruppe-Fahrdaten
   (Märklin/MFX/etc.) sind SX-spezifisch je Decoder. Gateway braucht konfigurierbare
   Bit-Mapping-Tabelle pro Lok.
3. **Baudrate SLX825:** Im Projekt-Memory ist 57600 dokumentiert; SX3-PC lässt sie
   variabel. → Konfigurierbar im Gateway, Default 57600.
4. **FCC-Code-Widerspruch** (§3.2) → nicht übernehmen.
5. **SX4 SLX825Interface:** Quelle nur als JAR → für serielle Details ohne
   Dekompilierung nicht auswertbar.

---

## 6. Hinweise für Tests an echter Hardware

- **SLX825:** Zuerst nur Lesen/Polling (Rautenhaus-Modus `0xFE,0xA0` setzen, dann
  Feedback beobachten). Nicht sofort Schreiben auf unbekannte Adressen.
- **SX852:** Sniffer (`tools/serial_sniffer.py`) starten, nur **lesende**/sichere
  Befehle senden, Antworten hexdump-mäßig protokollieren. Anlagenzustand nicht
  unkontrolliert verändern.
- Power-Befehl (`0xFF,...`) nur testen, wenn Anlage spannungsfrei sicher ist.

---

## 7. Beispiel-Steuertelegramm (belegt, Quelle: SX3-PC)

**Lok Adr 42, SX0, vorwärts, speed 18, Licht an:**
```
Adressbyte = 42 | 0x80 = 0xAA
Datenbyte   = 18 (speed) + 64 (Licht) = 0x52
Telegramm:  AA 52
```
(Quelle: `GenericSXInterface.sendLoco`, `SXInterface`-Schreibpfad)

---

## 8. Lizenzhinweise

- SX3-PC: GPL-3.0
- SX4: GPL-3.0
- → Keine direkte Quellcode-Übernahme in das MIT-lizenzierte Gateway.
  Protokollverständnis fließt eigenständig neu implementiert ein.
