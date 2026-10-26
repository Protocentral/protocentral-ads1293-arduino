// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 06.InterruptDriven
//
// Runs the ADS1293 at 400 SPS with DRDY latched by an interrupt, so a sample
// that arrives while loop() is busy is counted rather than silently lost.
// Prints Lead II for the Serial Plotter plus, once per second, the number of
// samples received and missed. 115200 baud.
//
// DRDY must be on an interrupt-capable pin (D2 or D3 on an Uno).
//
// Wiring: see the README (Hardware Setup section).
// For research and education only - not a medical device.

#include <SPI.h>
#include "Protocentral_ADS1293.h"

#if defined(ARDUINO_ARCH_ESP32)
#if defined(CONFIG_IDF_TARGET_ESP32C3)
const int PIN_CS = SS, PIN_DRDY = 3;  // GPIO4 is the C3's default SCK
#else
const int PIN_CS = SS, PIN_DRDY = 4;
#endif
#else
const int PIN_CS = 4, PIN_DRDY = 2;
#endif

Protocentral_ADS1293 ads(PIN_CS, PIN_DRDY);

uint32_t received = 0;
unsigned long lastReport = 0;
int32_t base = 0;

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    ADS1293_Config cfg;
    cfg.dataRate = ADS1293_DataRate::SPS_400;

    if (!ads.begin(cfg)) {
        Serial.print(F("ADS1293 not found, error "));
        Serial.println((int)ads.lastError());
        while (1) delay(1000);
    }
    if (!ads.enableDataReadyInterrupt()) {
        Serial.println(F("DRDY pin is not interrupt-capable on this board"));
        while (1) delay(1000);
    }
}

void loop() {
    ADS1293_Sample s;
    if (ads.available() && ads.read(s) && (s.ecgUpdated & 0x02)) {
        received++;
        base += (s.ecg[1] - base) / 64;
        Serial.print(F("II:"));
        Serial.println((long)ads.countsToMicrovolts(s.ecg[1] - base, ADS1293_Channel::CH2));
    }

    if (millis() - lastReport >= 1000) {
        lastReport = millis();
        Serial.print(F("received:"));
        Serial.print(received);
        Serial.print(F(" missed:"));
        Serial.println(ads.missedSamples());
        received = 0;
    }
}
