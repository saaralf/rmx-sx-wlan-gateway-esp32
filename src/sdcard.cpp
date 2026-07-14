#include "sdcard.h"
#include "config.h"

#include <FS.h>
#include <SD.h>
#include <SPI.h>

namespace
{
    SPIClass sdSpi(VSPI);
    bool cardReady = false;

    void listDirectory(fs::FS& fs, const char* dirname, uint8_t levels)
    {
        File root = fs.open(dirname);
        if (!root)
        {
            Serial.printf("[SD] Verzeichnis kann nicht geoeffnet werden: %s\n", dirname);
            return;
        }
        if (!root.isDirectory())
        {
            Serial.printf("[SD] Kein Verzeichnis: %s\n", dirname);
            root.close();
            return;
        }

        File file = root.openNextFile();
        while (file)
        {
            if (file.isDirectory())
            {
                Serial.printf("[SD] DIR  %s\n", file.name());
                if (levels > 0)
                {
                    listDirectory(fs, file.path(), levels - 1);
                }
            }
            else
            {
                Serial.printf("[SD] FILE %-30s %lu Bytes\n",
                              file.name(),
                              static_cast<unsigned long>(file.size()));
            }
            file.close();
            file = root.openNextFile();
        }
        root.close();
    }
}

bool sdCardBegin()
{
    if (cardReady)
    {
        return true;
    }

    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    sdSpi.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS, sdSpi, SD_SPI_FREQUENCY))
    {
        Serial.println("[SD] Initialisierung fehlgeschlagen");
        Serial.println("[SD] Karte/FAT32 und Pins CS=5 SCK=18 MISO=19 MOSI=23 pruefen");
        cardReady = false;
        return false;
    }

    if (SD.cardType() == CARD_NONE)
    {
        Serial.println("[SD] Keine Karte erkannt");
        SD.end();
        cardReady = false;
        return false;
    }

    cardReady = true;
    sdCardPrintInfo();
    return true;
}

bool sdCardReady()
{
    return cardReady;
}

void sdCardPrintInfo()
{
    if (!cardReady)
    {
        Serial.println("[SD] Nicht initialisiert");
        return;
    }

    const uint8_t type = SD.cardType();
    const char* typeName = "UNKNOWN";
    if (type == CARD_MMC) typeName = "MMC";
    else if (type == CARD_SD) typeName = "SDSC";
    else if (type == CARD_SDHC) typeName = "SDHC/SDXC";

    const uint64_t cardMB = SD.cardSize() / (1024ULL * 1024ULL);
    const uint64_t totalMB = SD.totalBytes() / (1024ULL * 1024ULL);
    const uint64_t usedMB = SD.usedBytes() / (1024ULL * 1024ULL);

    Serial.printf("[SD] Bereit: Typ=%s Karte=%llu MB FAT=%llu MB Belegt=%llu MB\n",
                  typeName, cardMB, totalMB, usedMB);
}

void sdCardList(const char* path, uint8_t levels)
{
    if (!cardReady)
    {
        Serial.println("[SD] Dateiliste nicht moeglich: Karte nicht bereit");
        return;
    }
    listDirectory(SD, path, levels);
}

bool sdCardWriteText(const char* path, const char* text)
{
    if (!cardReady || path == nullptr || text == nullptr)
    {
        return false;
    }

    File file = SD.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.printf("[SD] Schreiben fehlgeschlagen: %s\n", path);
        return false;
    }

    const size_t expected = strlen(text);
    const size_t written = file.print(text);
    file.close();

    const bool ok = written == expected;
    Serial.printf("[SD] Schreiben %s: %s\n", path, ok ? "OK" : "UNVOLLSTAENDIG");
    return ok;
}

bool sdCardAppendText(const char* path, const char* text)
{
    if (!cardReady || path == nullptr || text == nullptr)
    {
        return false;
    }

    File file = SD.open(path, FILE_APPEND);
    if (!file)
    {
        Serial.printf("[SD] Anhaengen fehlgeschlagen: %s\n", path);
        return false;
    }

    const size_t expected = strlen(text);
    const size_t written = file.print(text);
    file.close();

    const bool ok = written == expected;
    Serial.printf("[SD] Anhaengen %s: %s\n", path, ok ? "OK" : "UNVOLLSTAENDIG");
    return ok;
}
