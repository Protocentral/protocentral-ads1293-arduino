// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 10.Diagnostics
//
// First thing to run when something does not work. 115200 baud.
// Prints the library version, REVID and a full register dump, then checks
// each stage separately:
//   - SPI: the chip answers and registers read back
//   - clock: conversions actually happen (DATA_STATUS)
//   - data path: samples per second, read by polling DATA_STATUS (no DRDY needed)
//   - DRDY: the DRDY pin toggles once per sample
//   - signal chain: internal test signal on CH1
//   - supply voltage and error registers
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

// No DRDY pin for the driver: the data path is tested by polling, and DRDY is
// watched separately so a DRDY fault cannot hide a working chip
Protocentral_ADS1293 ads(PIN_CS);

const __FlashStringHelper* statusText(ADS1293_Status st) {
    switch (st) {
        case ADS1293_Status::Ok:              return F("OK");
        case ADS1293_Status::NotInitialized:  return F("not initialised");
        case ADS1293_Status::SpiError:        return F("no SPI response - check CS/SCK/MOSI/MISO, VCC and GND");
        case ADS1293_Status::WrongChipId:     return F("unexpected REVID - not an ADS1293, or MISO is noisy");
        case ADS1293_Status::VerifyFailed:    return F("register read-back mismatch - check SPI wiring and signal integrity");
        case ADS1293_Status::NoClock:         return F("chip answers but makes no conversions - its clock is not running");
        case ADS1293_Status::InvalidArgument: return F("invalid argument");
        case ADS1293_Status::NotInterruptPin: return F("DRDY pin has no interrupt");
        case ADS1293_Status::Timeout:         return F("timeout waiting for data");
    }
    return F("unknown");
}

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    Serial.print(F("ProtoCentral ADS1293 library v"));
    Serial.println(Protocentral_ADS1293::getLibraryVersion());

    bool ok = ads.begin();
    Serial.print(F("begin(): "));
    Serial.println(statusText(ads.lastError()));

    Serial.print(F("REVID: 0x"));
    Serial.print(ads.readID(), HEX);
    Serial.println(F(" (expected 0x1)"));

    Serial.println(F("\nRegisters:"));
    ads.printRegisters(Serial);

    if (!ok) {
        if (ads.lastError() == ADS1293_Status::NoClock) {
            Serial.println(F("\nNo conversions. Check:"));
            Serial.println(F(" - power-cycle the board (not just the MCU) and run this again"));
            Serial.println(F(" - the 4.096 MHz crystal oscillates (probe XTAL1 with a x10 probe)"));
            Serial.println(F(" - nothing drives the CLK header pin"));
        }
        return;
    }

    // ---- Data path and DRDY, measured over one second ----
    pinMode(PIN_DRDY, INPUT);
    float expected = ads.getSampleRate(ADS1293_Channel::CH1);
    uint16_t samples = 0, drdyEdges = 0, drdyLowReads = 0, drdyReads = 0;
    int lastDrdy = digitalRead(PIN_DRDY);
    t0 = millis();
    while (millis() - t0 < 1000) {
        int drdy = digitalRead(PIN_DRDY);
        drdyReads++;
        if (drdy == LOW) drdyLowReads++;
        if (lastDrdy == HIGH && drdy == LOW) drdyEdges++;
        lastDrdy = drdy;

        ADS1293_Sample s;
        if (ads.available() && ads.read(s) && (s.ecgUpdated & 0x01)) samples++;
    }

    Serial.print(F("\nSample rate: "));
    Serial.print(expected, 1);
    Serial.println(F(" SPS configured"));
    Serial.print(F("Samples in 1 s (status polling): "));
    Serial.println(samples);
    Serial.println(samples >= expected * 0.9f ? F("  Data path OK") : F("  Data path FAILED"));

    Serial.print(F("DRDY falling edges on pin "));
    Serial.print(PIN_DRDY);
    Serial.print(F(": "));
    Serial.print(drdyEdges);
    Serial.print(F("  (pin low "));
    Serial.print(100UL * drdyLowReads / (drdyReads ? drdyReads : 1));
    Serial.println(F("% of the time)"));
    if (drdyEdges >= expected * 0.8f) {
        Serial.println(F("  DRDY OK"));
    } else if (drdyLowReads == drdyReads) {
        Serial.println(F("  DRDY stuck LOW - check the DRDY wire, remove any pull-up/pull-down on it."));
        Serial.println(F("  The data still works: pass -1 as the DRDY pin to poll DATA_STATUS instead."));
    } else if (drdyLowReads == 0) {
        Serial.println(F("  DRDY stuck HIGH - check the DRDY wire and pin number."));
        Serial.println(F("  The data still works: pass -1 as the DRDY pin to poll DATA_STATUS instead."));
    } else {
        Serial.println(F("  DRDY toggles irregularly - check the wiring for noise or a weak connection."));
    }

    // ---- Signal chain: positive test signal on CH1 should read +200 mV ----
    ads.setTestSignal(ADS1293_Channel::CH1, ADS1293_TestSignal::Positive);
    float sum = 0;
    uint8_t n = 0, skip = 0;
    t0 = millis();
    while (n < 16 && millis() - t0 < 2000) {
        ADS1293_Sample s;
        if (!ads.available() || !ads.read(s) || !(s.ecgUpdated & 0x01)) continue;
        if (skip++ < 10) continue;
        sum += ads.countsToMicrovolts(s.ecg[0], ADS1293_Channel::CH1);
        n++;
    }
    ads.setTestSignal(ADS1293_Channel::CH1, ADS1293_TestSignal::Off);
    float testMv = n ? sum / n / 1000.0f : 0;
    Serial.print(F("\nCH1 test signal: "));
    Serial.print(testMv, 2);
    Serial.println(F(" mV (expected 200 mV)"));
    Serial.println(fabs(testMv - 200.0f) < 10.0f ? F("  Signal chain OK") : F("  Signal chain FAILED"));

    // ---- Supply and errors ----
    Serial.print(F("\nSupply (VDD): "));
    Serial.print(ads.readSupplyVoltage(), 3);
    Serial.println(F(" V"));
    Serial.print(F("ERROR_STATUS: 0x"));
    Serial.println(ads.readErrorStatus(), HEX);
    Serial.print(F("ERROR_LOD:    0x"));
    Serial.print(ads.readLeadOffStatus(), HEX);
    Serial.println(F("  (0x7 with no electrodes = RA, LA, LL off)"));
}

void loop() {}
