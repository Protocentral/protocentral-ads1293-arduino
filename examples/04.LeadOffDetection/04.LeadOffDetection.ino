// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 04.LeadOffDetection
//
// Reports which electrodes are off, once per second (115200 baud).
//
// DC mode (default) injects a small DC current into RA, LA and LL and flags
// an electrode whose input drifts towards VDD. Set USE_AC_LEADOFF to 1 for
// analog AC lead-off, which also works with dry (capacitive) electrodes.
//
// Both modes need the RL electrode: the current returns through the body to
// the right-leg drive. With RL off, every electrode reports off.
//
// Wiring: see the README (Hardware Setup section).
// For research and education only - not a medical device.

#include <SPI.h>
#include "Protocentral_ADS1293.h"

#define USE_AC_LEADOFF 0

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

const uint8_t ELECTRODES = ADS1293_ELECTRODE_RA | ADS1293_ELECTRODE_LA | ADS1293_ELECTRODE_LL;

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    // begin() already enables DC lead-off on RA, LA and LL at 32 nA
    if (!ads.begin()) {
        Serial.print(F("ADS1293 not found, error "));
        Serial.println((int)ads.lastError());
        while (1) delay(1000);
    }

#if USE_AC_LEADOFF
    // 100 nA at 2 kHz, lowest comparator threshold (datasheet Figure 24)
    ads.configureLeadOffAnalogAC(ELECTRODES, 100, 2000, ADS1293_ACLeadOffLevel::Level1);
    Serial.println(F("Analog AC lead-off"));
#else
    ads.configureLeadOffDC(ELECTRODES, 32);
    Serial.println(F("DC lead-off"));
#endif
}

unsigned long lastReport = 0;

void loop() {
    // Keep the data flowing; the lead-off flags also arrive in each sample
    ADS1293_Sample s;
    if (ads.available()) ads.read(s);

    if (millis() - lastReport < 1000) return;
    lastReport = millis();

    uint8_t off = ads.readLeadOffStatus();
    if ((off & ELECTRODES) == 0) {
        Serial.println(F("All electrodes connected"));
        return;
    }
    Serial.print(F("Off:"));
    if (off & ADS1293_ELECTRODE_RA) Serial.print(F(" RA"));
    if (off & ADS1293_ELECTRODE_LA) Serial.print(F(" LA"));
    if (off & ADS1293_ELECTRODE_LL) Serial.print(F(" LL"));
    if ((off & ELECTRODES) == ELECTRODES) Serial.print(F("  (all off - check the RL electrode)"));
    Serial.println();
}
