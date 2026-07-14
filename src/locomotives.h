#ifndef LOCOMOTIVES_H
#define LOCOMOTIVES_H

#include <Arduino.h>

// Maximale Anzahl der Lokomotiven in der CSV-Datei.
constexpr uint8_t MAX_LOCOMOTIVES = 32;

/**
 * Ein Eintrag aus der Lokdatenbank.
 */
struct LocomotiveInfo
{
    uint8_t address;
    char name[32];
    char imagePath[64];
};

/**
 * Lädt /loks/loks.csv von der SD-Karte.
 *
 * @return true, wenn mindestens eine Lok geladen wurde.
 */
bool locomotivesBegin();

/**
 * Anzahl der geladenen Lokomotiven.
 */
uint8_t locomotivesCount();

/**
 * Liefert einen Eintrag anhand des Listenindex.
 *
 * @return Zeiger auf die Lok oder nullptr.
 */
const LocomotiveInfo* locomotivesGet(uint8_t index);

/**
 * Sucht eine Lok anhand ihrer Digitaladresse.
 *
 * @return Zeiger auf die Lok oder nullptr.
 */
const LocomotiveInfo* locomotivesFindByAddress(uint8_t address);

/**
 * Gibt alle geladenen Lokomotiven auf Serial aus.
 */
void locomotivesPrintAll();

#endif