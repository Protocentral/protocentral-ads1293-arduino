// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 03.OpenView
//
// Streams three ECG leads to ProtoCentral OpenView
// (https://github.com/Protocentral/protocentral_openview).
// In OpenView, choose the "ADS1293 Breakout" board and this serial port.
//
// Wire format (must match OpenView's ads1293 descriptor - change both together):
//   115200 baud, 200 packets/s
//   0x0A 0xFA <len=12> 0x00 <type=0x02> | CH1 int32 | CH2 int32 | CH3 int32 | 0x00 0x0B
//   (all little-endian; signed counts around mid-scale)
//   CH1 = Lead I, CH2 = Lead II, CH3 = Lead III (3-lead) or V1 (5-lead, set FIVE_LEAD to 1)
//
// This sketch prints nothing but packets - any other text would corrupt the stream.
//
// Wiring: see the README (Hardware Setup section).
// For research and education only - not a medical device.

#include <SPI.h>
#include "Protocentral_ADS1293.h"

#define FIVE_LEAD 0

#if defined(ARDUINO_ARCH_ESP32)
#if defined(CONFIG_IDF_TARGET_ESP32C3)
const int PIN_CS = SS, PIN_DRDY = 3;  // GPIO4 is the C3's default SCK
#else
const int PIN_CS = SS, PIN_DRDY = 4;
#endif
#else
const int PIN_CS = 4, PIN_DRDY = 2;
#endif

const uint8_t PKT_START_1 = 0x0A;
const uint8_t PKT_START_2 = 0xFA;
const uint8_t PKT_TYPE    = 0x02;
const uint8_t PKT_STOP_1  = 0x00;
const uint8_t PKT_STOP_2  = 0x0B;
const uint8_t PAYLOAD_LEN = 12;

Protocentral_ADS1293 ads(PIN_CS, PIN_DRDY);

static void putInt32(uint8_t* p, int32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static void sendPacket(int32_t ch1, int32_t ch2, int32_t ch3) {
    uint8_t pkt[5 + PAYLOAD_LEN + 2] = {PKT_START_1, PKT_START_2, PAYLOAD_LEN, 0x00, PKT_TYPE};
    putInt32(&pkt[5], ch1);
    putInt32(&pkt[9], ch2);
    putInt32(&pkt[13], ch3);
    pkt[17] = PKT_STOP_1;
    pkt[18] = PKT_STOP_2;
    Serial.write(pkt, sizeof(pkt));
}

void setup() {
    Serial.begin(115200);

    ADS1293_Config cfg;
    cfg.dataRate = ADS1293_DataRate::SPS_200;
#if FIVE_LEAD
    cfg.leadSet = ADS1293_LeadSet::FiveLead;
#endif

    if (!ads.begin(cfg)) {
        // No text allowed on the stream: blink the LED instead
#ifdef LED_BUILTIN
        pinMode(LED_BUILTIN, OUTPUT);
        while (1) {
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            delay(200);
        }
#else
        while (1) delay(1000);
#endif
    }
}

void loop() {
    ADS1293_Sample s;
    if (!ads.available() || !ads.read(s)) return;
    if (!(s.ecgUpdated & 0x01)) return;

#if FIVE_LEAD
    sendPacket(s.ecg[0], s.ecg[1], s.ecg[2]);
#else
    sendPacket(s.ecg[0], s.ecg[1], s.ecg[1] - s.ecg[0]);
#endif
}
