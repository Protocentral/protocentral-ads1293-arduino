// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 05.SelfTest
//
// Checks the whole signal chain of all three channels without electrodes
// (115200 baud):
//   - positive and negative internal test signals, +/-VREF/12 = +/-200 mV
//   - shorted inputs: offset and RMS noise floor
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

const uint8_t SETTLE  = 10;   // samples discarded after each switch
const uint8_t SAMPLES = 64;   // samples averaged per measurement

const float TEST_UV          = 200000.0f;  // VREF / 12
const float TEST_TOLERANCE   = 0.05f;      // 5 %
const float NOISE_LIMIT_UV   = 5.0f;       // datasheet: ~1.5 uV RMS at 200 SPS

Protocentral_ADS1293 ads(PIN_CS, PIN_DRDY);

// Average SAMPLES readings per channel; also returns the RMS deviation
static bool measure(float meanUv[3], float rmsUv[3]) {
    float sum[3] = {0, 0, 0}, sumSq[3] = {0, 0, 0};
    uint8_t n = 0, skipped = 0;
    unsigned long t0 = millis();
    while (n < SAMPLES) {
        if (millis() - t0 > 3000) return false;
        ADS1293_Sample s;
        if (!ads.available() || !ads.read(s) || !(s.ecgUpdated & 0x01)) continue;
        if (skipped < SETTLE) {
            skipped++;
            continue;
        }
        for (uint8_t ch = 0; ch < 3; ch++) {
            float uv = ads.countsToMicrovolts(s.ecg[ch], (ADS1293_Channel)ch);
            sum[ch] += uv;
            sumSq[ch] += uv * uv;
        }
        n++;
    }
    for (uint8_t ch = 0; ch < 3; ch++) {
        meanUv[ch]   = sum[ch] / SAMPLES;
        float var    = sumSq[ch] / SAMPLES - meanUv[ch] * meanUv[ch];
        rmsUv[ch]    = var > 0 ? sqrt(var) : 0;
    }
    return true;
}

static bool runTest(const __FlashStringHelper* name, ADS1293_TestSignal sig, float expectedUv) {
    ads.setTestSignal(sig);
    float mean[3], rms[3];
    if (!measure(mean, rms)) {
        Serial.println(F("  no data"));
        return false;
    }
    bool pass = true;
    for (uint8_t ch = 0; ch < 3; ch++) {
        bool ok;
        if (expectedUv == 0) {
            ok = rms[ch] < NOISE_LIMIT_UV;
        } else {
            ok = fabs(mean[ch] - expectedUv) < fabs(expectedUv) * TEST_TOLERANCE;
        }
        pass = pass && ok;
        Serial.print(name);
        Serial.print(F(" CH"));
        Serial.print(ch + 1);
        Serial.print(F(": mean "));
        Serial.print(mean[ch], 1);
        Serial.print(F(" uV, noise "));
        Serial.print(rms[ch], 2);
        Serial.print(F(" uV RMS  "));
        Serial.println(ok ? F("PASS") : F("FAIL"));
    }
    return pass;
}

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    // All three channels on; the test signal replaces the electrode inputs
    ADS1293_Config cfg;
    cfg.leadSet = ADS1293_LeadSet::FiveLead;
    cfg.leadOff = false;

    if (!ads.begin(cfg)) {
        Serial.print(F("ADS1293 begin() failed, error "));
        Serial.println((int)ads.lastError());
        while (1) delay(1000);
    }

    bool pass = runTest(F("Positive"), ADS1293_TestSignal::Positive, TEST_UV);
    pass = runTest(F("Negative"), ADS1293_TestSignal::Negative, -TEST_UV) && pass;
    pass = runTest(F("Shorted "), ADS1293_TestSignal::Zero, 0) && pass;
    ads.setTestSignal(ADS1293_TestSignal::Off);

    Serial.println(pass ? F("\nSelf-test PASSED") : F("\nSelf-test FAILED"));
}

void loop() {}
