#ifndef SDCARD_H
#define SDCARD_H

#include <Arduino.h>

/** Initialisiert die FAT/FAT32-MicroSD-Karte auf dem separaten VSPI-Bus. */
bool sdCardBegin();

/** Liefert true, wenn die Karte erfolgreich eingebunden wurde. */
bool sdCardReady();

/** Gibt Kartentyp, Kapazitaet und Speicherbelegung auf Serial aus. */
void sdCardPrintInfo();

/** Listet Dateien und Verzeichnisse rekursiv auf Serial. */
void sdCardList(const char* path = "/", uint8_t levels = 2);

/** Schreibt oder ersetzt eine UTF-8-Textdatei. */
bool sdCardWriteText(const char* path, const char* text);

/** Haengt Text an eine bestehende Datei an oder legt sie neu an. */
bool sdCardAppendText(const char* path, const char* text);

#endif // SDCARD_H
