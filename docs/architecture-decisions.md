# Architekturentscheidungen

| Nr | Entscheidung | Begründung | Stand |
|----|--------------|-----------|-------|
| A1 | Backend-Sprache **Python 3** (asyncio, aiohttp, pydantic) statt C++ | Schnelle Entwicklung, gute Testbarkeit, seriell via pyserial-asyncio, kein harter Echtzeit-Bedarf im Gateway | final |
| A2 | Wire-Protokoll **JSON über WebSocket**, zentral in `protocol.py` | Versionierbar, menschenlesbar, einfach zu debuggen; ESP32-Client leicht umsetzbar | final |
| A3 | Gateway = **autoritative Zustandsquelle** (StateStore) | Mehrere Clients, Robustheit gegen veraltete Befehle, zentrale Validierung | final |
| A4 | Sequenznummern gegen **Doppel-/Replay-Schutz** | `apply_command` ignoriert `seq <= last_seq` (gleiche Quelle) | final |
| A5 | **Kein** automatischer RMX-Adress-Übersetzungs-Layer | Aufgabenstellung: Bus+Adresse immer getrennt; `interface_id+bus+address` als Schlüssel | final |
| A6 | Treiberabstraktion `InterfaceDriver` + `SimulatorDriver`, `RMX952Driver`, `SX825Driver`, `SX852Driver` | Saubere Trennung, isolierte Reconnect-Logik, testbar ohne HW | final |
| A7 | **Simulator zuerst** (Phase 1–3), RMX/SX-Treiber später | Kommunikation Handregler↔Gateway ist entkoppelt testbar; RMX-Doku steht aus | final |
| A8 | WLAN-AP via **hostapd + dnsmasq** auf Raspberry Pi OS | Robust, idempotent automatisierbar, kein NetworkManager-Zwang | final |
| A9 | SX-Loks immer **SX0** (wie SX3-PC `sendLoco`) | SX3-PC-Quelle bestätigt: Loksteuerung auf SX0; SX1 nur Schalten/Melden | offen-präzisiert |
| A10 | SX852-Treiber **nicht** aus SX3-PC abgeleitet | SX852-Protokoll in Referenzen OFFEN → eigener Treiber, nur Schlüsselgerüst, echte HW nötig | offen |

---

# Offene Fragen (open-questions.md)

1. **Raspi-Erreichbarkeit:** 192.168.0.1 ist die Fritzbox, nicht der Pi. Pi-IP für AP
   ist 192.168.50.1 (laut Aufgabe). SSH-Zugang zum Pi noch nicht geprüft.
2. **SX852-PC-Protokoll:** Keine belastbare Quelle. → Sniffer + echte HW nötig.
3. **RMX-Protokoll:** Dokumentation (RMX-Doku_V5.pdf) liegt in `docs/`, aber
   Implementierung bewusst zurückgestellt (Phase 4).
4. **F0..F16 → SX-Bit-Mapping:** SX-spezifisch je Decoder. Gateway braucht
   konfigurierbare Mapping-Tabelle pro Lok (noch nicht implementiert).
5. **Baudrate SLX825:** Default 57600 (Projekt-Memory), konfigurierbar.
6. **FCC-Code-Widerspruch:** in SX3-PC nicht übernommen (siehe sx-reference-analysis.md §3.2).

---

# Quellen und Lizenzen (sources-and-licenses.md)

| Quelle | Typ | Lizenz | Nutzung im Gateway |
|--------|-----|--------|--------------------|
| github.com/michael71/SX3-PC | Java/RXTX, SLX825+Rautenhaus-Format | **GPL-3.0** | Nur Verständnis. **Keine** Quellcode-Übernahme. |
| github.com/michael71/SX4 | Java JAR + HTML-Docs | **GPL-3.0** | Nur Verständnis (TCP-Protokoll-Docs). Kein Quellcode. |
| RMX-Doku_V5.pdf | Hardware-Doku | Proprietär (Rautenhaus) | Verb曲lich für RMX-Treiber (Phase 4). |
| paelzer/ESP32-Cheap-Yellow-Display-Documentation-DE | Hardware-Doku CYD | — | Pin/Display-Referenz für ESP32-Client. |
| Eigenentwicklung (dieses Repo) | Python/Arduino | **MIT** (Michael Keller) | Vollständiger Gateway-Code. |

**Lizenz-Strategie:** Das Gateway ist MIT-lizenziert. GPL-3.0-Quellen werden
ausdrücklich **nicht** kopiert; das SX825-Protokollverständnis ist eigenständig
neu implementiert (siehe `src/rmx_sx_gateway/drivers/sx825.py`).
