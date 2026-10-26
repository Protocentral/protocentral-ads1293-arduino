// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 09.MultiBoardClockSync
//
// Runs two ADS1293 boards from one clock, for six ECG channels. The master
// drives its 409.6 kHz clock out on CLK; the slave uses that as an external
// clock, so both sample at exactly the same rate. Prints Lead II of both
// boards for the Serial Plotter (115200 baud).
//
// Wiring: both boards share SCK, MOSI, MISO, VCC and GND. Each has its own CS.
// Connect CLK of the master to CLK of the slave. Only the master's DRDY is used.
// CLK is not level-shifted on the breakout: 3.3 V signals only.
// SYNCB is not on the breakout header, so the two boards' filters are not
// phase-aligned: samples can be offset by up to one sample period.
//
// For research and education only - not a medical device.

#include <SPI.h>
#include "Protocentral_ADS1293.h"

#if defined(ARDUINO_ARCH_ESP32)
#if defined(CONFIG_IDF_TARGET_ESP32C3)
const int PIN_CS_MASTER = SS, PIN_CS_SLAVE = 2, PIN_DRDY = 3;  // GPIO4 is the C3's default SCK
#else
const int PIN_CS_MASTER = SS, PIN_CS_SLAVE = 15, PIN_DRDY = 4;
#endif
#else
const int PIN_CS_MASTER = 4, PIN_CS_SLAVE = 5, PIN_DRDY = 2;
#endif

Protocentral_ADS1293 master(PIN_CS_MASTER, PIN_DRDY);
Protocentral_ADS1293 slave(PIN_CS_SLAVE);  // no DRDY: read whenever the master has data

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    // The master's clock output must be running before the slave starts
    ADS1293_Config mcfg;
    mcfg.clockOutput = true;
    if (!master.begin(mcfg)) {
        Serial.print(F("Master begin() failed, error "));
        Serial.println((int)master.lastError());
        while (1) delay(1000);
    }

    ADS1293_Config scfg;
    scfg.clock = ADS1293_ClockSource::External;
    if (!slave.begin(scfg, false)) {  // SPI is already running
        Serial.print(F("Slave begin() failed, error "));
        Serial.println((int)slave.lastError());
        Serial.println(F("NoClock (5) means the CLK link is missing"));
        while (1) delay(1000);
    }
}

int32_t baseM = 0, baseS = 0;

void loop() {
    ADS1293_Sample m, s;
    if (!master.available() || !master.read(m) || !(m.ecgUpdated & 0x02)) return;
    if (!slave.available() || !slave.read(s)) return;

    baseM += (m.ecg[1] - baseM) / 64;
    baseS += (s.ecg[1] - baseS) / 64;

    Serial.print(F("master_II:"));
    Serial.print((long)master.countsToMicrovolts(m.ecg[1] - baseM, ADS1293_Channel::CH2));
    Serial.print(F(" slave_II:"));
    Serial.println((long)slave.countsToMicrovolts(s.ecg[1] - baseS, ADS1293_Channel::CH2));
}
