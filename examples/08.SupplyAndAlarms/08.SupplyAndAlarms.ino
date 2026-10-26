// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 08.SupplyAndAlarms
//
// Every two seconds (115200 baud):
//   - measures the chip supply with the battery monitor (borrows the unused CH3)
//   - reads and decodes every error register: lead-off, common-mode and RLD
//     range, low battery, per-channel INA / modulator range, synchronization
//
// Disconnect an electrode or the RL lead to see the flags change.
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

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    if (!ads.begin()) {
        Serial.print(F("ADS1293 begin() failed, error "));
        Serial.println((int)ads.lastError());
        while (1) delay(1000);
    }

    // An alarm must persist for 8 clock counts before it is accepted
    ads.setAlarmFilter(8, 8);
}

static void printFlag(uint8_t value, uint8_t bit, const __FlashStringHelper* name) {
    if (value & bit) {
        Serial.print(' ');
        Serial.print(name);
    }
}

void loop() {
    float vdd = ads.readSupplyVoltage();
    Serial.print(F("Supply (VDD): "));
    if (isnan(vdd)) {
        Serial.println(F("measurement failed"));
    } else {
        Serial.print(vdd, 3);
        Serial.println(F(" V"));
    }

    uint8_t status = ads.readErrorStatus();  // also clears the latched flags
    Serial.print(F("ERROR_STATUS 0x"));
    Serial.print(status, HEX);
    printFlag(status, ADS1293_ERR_CMOR, F("common-mode-out-of-range"));
    printFlag(status, ADS1293_ERR_RLDRAIL, F("RLD-near-rail"));
    printFlag(status, ADS1293_ERR_BATLOW, F("supply-below-2.7V"));
    printFlag(status, ADS1293_ERR_LEADOFF, F("lead-off"));
    printFlag(status, ADS1293_ERR_CH1, F("CH1-range"));
    printFlag(status, ADS1293_ERR_CH2, F("CH2-range"));
    printFlag(status, ADS1293_ERR_CH3, F("CH3-range"));
    printFlag(status, ADS1293_ERR_SYNCEDGE, F("sync-edge"));
    Serial.println();

    uint8_t lod = ads.readLeadOffStatus();
    Serial.print(F("Lead-off:"));
    printFlag(lod, ADS1293_ELECTRODE_RA, F("RA"));
    printFlag(lod, ADS1293_ELECTRODE_LA, F("LA"));
    printFlag(lod, ADS1293_ELECTRODE_LL, F("LL"));
    if (!lod) Serial.print(F(" none"));
    Serial.println();

    for (uint8_t ch = 0; ch < 2; ch++) {
        uint8_t range = ads.readRangeErrors((ADS1293_Channel)ch);
        Serial.print(F("CH"));
        Serial.print(ch + 1);
        Serial.print(F(" range:"));
        printFlag(range, ADS1293_RANGE_DIF_HIGH, F("INA-differential-overrange"));
        printFlag(range, ADS1293_RANGE_OUTP_HIGH, F("OUTP-high"));
        printFlag(range, ADS1293_RANGE_OUTP_LOW, F("OUTP-low"));
        printFlag(range, ADS1293_RANGE_OUTN_HIGH, F("OUTN-high"));
        printFlag(range, ADS1293_RANGE_OUTN_LOW, F("OUTN-low"));
        printFlag(range, ADS1293_RANGE_SDM_OR, F("modulator-overrange"));
        if (!(range & ~ADS1293_RANGE_SIGN)) Serial.print(F(" ok"));
        Serial.println();
    }

    uint8_t misc = ads.readMiscErrors();
    Serial.print(F("Live status:"));
    printFlag(misc, ADS1293_MISC_CMOR, F("CMOR"));
    printFlag(misc, ADS1293_MISC_RLDRAIL, F("RLDRAIL"));
    printFlag(misc, ADS1293_MISC_BATLOW, F("BATLOW"));
    if (!misc) Serial.print(F(" ok"));
    Serial.print(F("   sync errors: 0x"));
    Serial.println(ads.readSyncErrors(), HEX);
    Serial.println();

    delay(2000);
}
