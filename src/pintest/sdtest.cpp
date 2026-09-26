// src/pintest/sdtest.cpp
// SD card bring-up — compiled with env:sdtest only.
// Lists all files on the card, then specifically checks for each WAV file
// the production firmware expects (0.WAV through 15.WAV).
// Reports file size and reads first 4 bytes to confirm the RIFF header.

#include <Arduino.h>
#include <SD.h>

static void list_dir(File dir, uint8_t depth) {
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;
        for (uint8_t i = 0; i < depth; i++) Serial.print("  ");
        Serial.print(entry.name());
        if (entry.isDirectory()) {
            Serial.println("/");
            list_dir(entry, depth + 1);
        } else {
            Serial.printf("  (%lu bytes)\n", entry.size());
        }
        entry.close();
    }
}

static void check_wav(uint8_t id) {
    char name[10];
    snprintf(name, sizeof(name), "%u.WAV", id);
    File f = SD.open(name);
    if (!f) {
        Serial.printf("  %5s  MISSING\n", name);
        return;
    }
    uint32_t size = f.size();
    uint8_t  hdr[4];
    f.read(hdr, 4);
    f.close();
    bool riff = (hdr[0]=='R' && hdr[1]=='I' && hdr[2]=='F' && hdr[3]=='F');
    Serial.printf("  %5s  %7lu bytes  header:%c%c%c%c  %s\n",
                  name, size,
                  hdr[0], hdr[1], hdr[2], hdr[3],
                  riff ? "OK" : "NOT A RIFF FILE");
}

void setup(void) {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {}

    Serial.println("\n=== SD card test ===");

    if (!SD.begin(BUILTIN_SDCARD)) {
        Serial.println("SD.begin() FAILED — card missing or not FAT32?");
        return;
    }
    Serial.println("SD init OK\n");

    Serial.println("--- Directory listing ---");
    File root = SD.open("/");
    list_dir(root, 0);
    root.close();

    Serial.println("\n--- WAV file check (0.WAV – 15.WAV) ---");
    for (uint8_t i = 0; i <= 15; i++) check_wav(i);

    Serial.println("\nDone.");
}

void loop(void) {}
