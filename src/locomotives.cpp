#include "locomotives.h"
#include "sdcard.h"

#include <FS.h>
#include <SD.h>
#include <cstring>
#include <cstdlib>

namespace
{
    LocomotiveInfo locomotiveList[MAX_LOCOMOTIVES];
    uint8_t locomotiveCount = 0;

    /**
     * Entfernt Leerzeichen, Tabs und Zeilenumbrüche
     * am Anfang und Ende eines Textes.
     */
    void trimText(char* text)
    {
        if (text == nullptr)
        {
            return;
        }

        char* start = text;

        while (
            *start == ' ' ||
            *start == '\t' ||
            *start == '\r' ||
            *start == '\n'
        )
        {
            start++;
        }

        if (start != text)
        {
            memmove(
                text,
                start,
                strlen(start) + 1
            );
        }

        int length =
            static_cast<int>(strlen(text));

        while (
            length > 0 &&
            (
                text[length - 1] == ' ' ||
                text[length - 1] == '\t' ||
                text[length - 1] == '\r' ||
                text[length - 1] == '\n'
            )
        )
        {
            text[length - 1] = '\0';
            length--;
        }
    }

    /**
     * Prüft, ob die Adresse bereits in der Liste existiert.
     */
    bool addressAlreadyExists(uint8_t address)
    {
        for (uint8_t index = 0;
             index < locomotiveCount;
             index++)
        {
            if (locomotiveList[index].address == address)
            {
                return true;
            }
        }

        return false;
    }

    /**
     * Zerlegt eine CSV-Zeile:
     *
     * Adresse,Name,Bildpfad
     */
    bool parseLocomotiveLine(
        char* line,
        LocomotiveInfo& destination
    )
    {
        if (
            line == nullptr ||
            line[0] == '\0'
        )
        {
            return false;
        }

        char* firstComma =
            strchr(line, ',');

        if (firstComma == nullptr)
        {
            return false;
        }

        *firstComma = '\0';

        char* nameText =
            firstComma + 1;

        char* secondComma =
            strchr(nameText, ',');

        if (secondComma == nullptr)
        {
            return false;
        }

        *secondComma = '\0';

        char* addressText = line;
        char* imageText = secondComma + 1;

        trimText(addressText);
        trimText(nameText);
        trimText(imageText);

        if (
            addressText[0] == '\0' ||
            nameText[0] == '\0' ||
            imageText[0] == '\0'
        )
        {
            return false;
        }

        char* parseEnd = nullptr;

        const long parsedAddress =
            strtol(
                addressText,
                &parseEnd,
                10
            );

        if (
            parseEnd == addressText ||
            *parseEnd != '\0' ||
            parsedAddress < 1 ||
            parsedAddress > 255
        )
        {
            return false;
        }

        destination.address =
            static_cast<uint8_t>(parsedAddress);

        strncpy(
            destination.name,
            nameText,
            sizeof(destination.name) - 1
        );

        destination.name[
            sizeof(destination.name) - 1
        ] = '\0';

        strncpy(
            destination.imagePath,
            imageText,
            sizeof(destination.imagePath) - 1
        );

        destination.imagePath[
            sizeof(destination.imagePath) - 1
        ] = '\0';

        return true;
    }
}

bool locomotivesBegin()
{
    locomotiveCount = 0;

    if (!sdCardReady())
    {
        Serial.println(
            "[LOKS] SD-Karte ist nicht bereit"
        );

        return false;
    }

    File csvFile = SD.open(
        "/loks/loks.csv",
        FILE_READ
    );

    if (!csvFile)
    {
        Serial.println(
            "[LOKS] Datei fehlt: /loks/loks.csv"
        );

        return false;
    }

    uint32_t lineNumber = 0;

    while (
        csvFile.available() &&
        locomotiveCount < MAX_LOCOMOTIVES
    )
    {
        String line =
            csvFile.readStringUntil('\n');

        lineNumber++;
        line.trim();

        // Leere Zeilen überspringen.
        if (line.length() == 0)
        {
            continue;
        }

        // Kommentarzeilen überspringen.
        if (line.startsWith("#"))
        {
            continue;
        }

        // Kopfzeile überspringen.
        if (
            lineNumber == 1 &&
            line.startsWith("address,")
        )
        {
            continue;
        }

        // Sehr lange Zeilen ablehnen.
        if (line.length() >= 128)
        {
            Serial.printf(
                "[LOKS] CSV-Zeile %lu ist zu lang\n",
                static_cast<unsigned long>(lineNumber)
            );

            continue;
        }

        char lineBuffer[128];

        line.toCharArray(
            lineBuffer,
            sizeof(lineBuffer)
        );

        LocomotiveInfo parsed{};

        if (!parseLocomotiveLine(
                lineBuffer,
                parsed
            ))
        {
            Serial.printf(
                "[LOKS] Ungueltige CSV-Zeile %lu: %s\n",
                static_cast<unsigned long>(lineNumber),
                line.c_str()
            );

            continue;
        }

        if (addressAlreadyExists(parsed.address))
        {
            Serial.printf(
                "[LOKS] Doppelte Adresse %u in Zeile %lu\n",
                parsed.address,
                static_cast<unsigned long>(lineNumber)
            );

            continue;
        }

        locomotiveList[locomotiveCount] =
            parsed;

        locomotiveCount++;
    }

    if (
        csvFile.available() &&
        locomotiveCount >= MAX_LOCOMOTIVES
    )
    {
        Serial.printf(
            "[LOKS] Maximale Anzahl erreicht: %u\n",
            MAX_LOCOMOTIVES
        );
    }

    csvFile.close();

    Serial.printf(
        "[LOKS] %u Lokomotiven geladen\n",
        locomotiveCount
    );

    return locomotiveCount > 0;
}

uint8_t locomotivesCount()
{
    return locomotiveCount;
}

const LocomotiveInfo* locomotivesGet(uint8_t index)
{
    if (index >= locomotiveCount)
    {
        return nullptr;
    }

    return &locomotiveList[index];
}

const LocomotiveInfo* locomotivesFindByAddress(
    uint8_t address
)
{
    for (uint8_t index = 0;
         index < locomotiveCount;
         index++)
    {
        if (
            locomotiveList[index].address ==
            address
        )
        {
            return &locomotiveList[index];
        }
    }

    return nullptr;
}

void locomotivesPrintAll()
{
    Serial.println(
        "[LOKS] Geladene Lokomotiven:"
    );

    for (uint8_t index = 0;
         index < locomotiveCount;
         index++)
    {
        const LocomotiveInfo& locomotive =
            locomotiveList[index];

        Serial.printf(
            "[LOKS] %u: Adresse=%u Name=\"%s\" Bild=\"%s\"\n",
            index,
            locomotive.address,
            locomotive.name,
            locomotive.imagePath
        );
    }
}