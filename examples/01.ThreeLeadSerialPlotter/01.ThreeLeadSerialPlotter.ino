// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 01.ThreeLeadSerialPlotter
//
// Streams the three limb leads of a 3-electrode ECG (plus the RL drive
// electrode) to the Arduino Serial Plotter (Tools > Serial Plotter, 115200 baud).
// CH1 measures Lead I and CH2 Lead II; Lead III = II - I is computed.
// Values are in microvolts at the electrodes.
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

    if (!ads.begin()) {
        Serial.print(F("ADS1293 begin() failed, error "));
        Serial.println((int)ads.lastError());
        Serial.println(F("Run 10.Diagnostics for details"));
        while (1) delay(1000);
    }
}

// The electrode offset is much larger than the ECG; subtracting a slow running
// average keeps the traces centred in the plotter
int32_t base1 = 0, base2 = 0;

void loop() {
    ADS1293_Sample s;
    if (!ads.available() || !ads.read(s)) return;
    if (!(s.ecgUpdated & 0x01)) return;

    base1 += (s.ecg[0] - base1) / 64;
    base2 += (s.ecg[1] - base2) / 64;

    long leadI   = (long)ads.countsToMicrovolts(s.ecg[0] - base1, ADS1293_Channel::CH1);
    long leadII  = (long)ads.countsToMicrovolts(s.ecg[1] - base2, ADS1293_Channel::CH2);
    long leadIII = leadII - leadI;

    Serial.print(F("I:"));
    Serial.print(leadI);
    Serial.print(F(" II:"));
    Serial.print(leadII);
    Serial.print(F(" III:"));
    Serial.println(leadIII);
}
