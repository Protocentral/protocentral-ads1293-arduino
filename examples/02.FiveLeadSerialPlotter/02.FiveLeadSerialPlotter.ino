// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 02.FiveLeadSerialPlotter
//
// 5-electrode ECG: Lead I, Lead II and the precordial lead V1, measured
// against the Wilson central terminal (the average of RA, LA and LL, built
// inside the ADS1293 and routed to IN6). Serial Plotter, 115200 baud,
// values in microvolts.
//
// Electrodes: RA, LA, RL on jack 1; LL and V1 on jack 2. Jumper JMP1 must not
// connect IN6 to an electrode.
//
// Wiring: see the README (Hardware Setup section).
// For research and education only - not a medical device.

#include <SPI.h>
#include "Protocentral_ADS1293.h"

// RST and SYNC must be driven: on the v2 breakout they reach the chip through a
// level shifter. Pass -1 for a pin you tie to VCC instead (or for SYNC on v1 boards)
#if defined(ARDUINO_ARCH_ESP32)
#if defined(CONFIG_IDF_TARGET_ESP32C3)
const int PIN_CS = SS, PIN_DRDY = 3, PIN_RESET = 1, PIN_SYNC = 10;  // GPIO4 is the C3's default SCK
#elif defined(CONFIG_IDF_TARGET_ESP32)
const int PIN_CS = SS, PIN_DRDY = 4, PIN_RESET = 25, PIN_SYNC = 26;
#else
const int PIN_CS = SS, PIN_DRDY = 4, PIN_RESET = 5, PIN_SYNC = 6;   // ESP32-S2 / S3
#endif
#else
const int PIN_CS = 4, PIN_DRDY = 2, PIN_RESET = 5, PIN_SYNC = 6;
#endif

Protocentral_ADS1293 ads(PIN_CS, PIN_DRDY, PIN_RESET, PIN_SYNC);

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    ADS1293_Config cfg;
    cfg.leadSet = ADS1293_LeadSet::FiveLead;

    if (!ads.begin(cfg)) {
        Serial.print(F("ADS1293 begin() failed, error "));
        Serial.println((int)ads.lastError());
        Serial.println(F("Run 10.Diagnostics for details"));
        while (1) delay(1000);
    }
}

int32_t base[3] = {0, 0, 0};

void loop() {
    ADS1293_Sample s;
    if (!ads.available() || !ads.read(s)) return;
    if (!(s.ecgUpdated & 0x01)) return;

    long uv[3];
    for (uint8_t ch = 0; ch < 3; ch++) {
        base[ch] += (s.ecg[ch] - base[ch]) / 64;
        uv[ch] = (long)ads.countsToMicrovolts(s.ecg[ch] - base[ch], (ADS1293_Channel)ch);
    }

    Serial.print(F("I:"));
    Serial.print(uv[0]);
    Serial.print(F(" II:"));
    Serial.print(uv[1]);
    Serial.print(F(" V1:"));
    Serial.println(uv[2]);
}
