// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 07.PaceDetection
//
// Uses the ADS1293's digital pace path: a 16-bit, high-rate output taken
// before the final ECG filter, here at 3200 SPS on Lead II next to the normal
// 200 SPS ECG. Pacemaker pulses (0.1-2 ms wide, steep edges) show up as large
// sample-to-sample jumps in the pace data, which a slope threshold detects.
// Prints each detected pulse and the pace count every second (115200 baud).
//
// Test it with an ECG simulator that has a pacemaker output. The threshold is
// a starting point, not a validated detector.
//
// Wiring: see the Wiring section of the README.
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

const float         SLOPE_THRESHOLD_UV = 1000.0f;  // jump between consecutive pace samples
const unsigned long REFRACTORY_MS      = 200;

Protocentral_ADS1293 ads(PIN_CS, PIN_DRDY, PIN_RESET, PIN_SYNC);

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    if (!ads.begin()) {
        Serial.print(F("ADS1293 begin() failed, error "));
        Serial.println((int)ads.lastError());
        while (1) delay(1000);
    }

    // Pace rate = 102400 / (R1 x R2) = 102400 / (4 x 8) = 3200 SPS;
    // ECG rate = pace rate / R3 = 3200 / 16 = 200 SPS
    ads.setR2(ADS1293_R2::R2_8);
    for (uint8_t ch = 0; ch < 2; ch++) ads.setR3((ADS1293_Channel)ch, ADS1293_R3::R3_16);
    ads.setPaceReadback(ADS1293_Channel::CH2, true);  // DRDY now follows the faster pace data

    Serial.print(F("Pace rate: "));
    Serial.print(ads.getPaceRate(ADS1293_Channel::CH2), 0);
    Serial.print(F(" SPS, ECG rate: "));
    Serial.print(ads.getSampleRate(ADS1293_Channel::CH2), 0);
    Serial.println(F(" SPS"));
}

int16_t       lastPace    = 0;
bool          havePace    = false;
unsigned long lastPulse   = 0;
uint16_t      pulses      = 0;
unsigned long lastReport  = 0;

void loop() {
    ADS1293_Sample s;
    if (ads.available() && ads.read(s) && (s.paceUpdated & 0x02)) {
        int16_t pace = s.pace[1];
        if (havePace) {
            float slope = ads.paceCountsToMicrovolts(pace - lastPace);
            if (fabs(slope) > SLOPE_THRESHOLD_UV && millis() - lastPulse > REFRACTORY_MS) {
                lastPulse = millis();
                pulses++;
                Serial.print(F("Pace pulse at "));
                Serial.print(lastPulse);
                Serial.print(F(" ms, step "));
                Serial.print(slope, 0);
                Serial.println(F(" uV"));
            }
        }
        lastPace = pace;
        havePace = true;
    }

    if (millis() - lastReport >= 1000) {
        lastReport = millis();
        Serial.print(F("pulses/s: "));
        Serial.println(pulses);
        pulses = 0;
    }
}
