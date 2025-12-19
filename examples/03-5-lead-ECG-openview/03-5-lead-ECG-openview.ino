//////////////////////////////////////////////////////////////////////////////////////////
//
//  Protocentral ADS1293 Arduino example — 5-lead ECG (OpenView packet format)
//
//  Author: Ashwin Whitchurch
//  Copyright (c) 2020-2025 Protocentral Electronics
//
//  SPDX-License-Identifier: MIT
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
//
//////////////////////////////////////////////////////////////////////////////////////////


#include "protocentral_ads1293.h"
#include <SPI.h>

#define CES_CMDIF_PKT_START_1 0x0A
#define CES_CMDIF_PKT_START_2 0xFA
#define CES_CMDIF_TYPE_DATA 0x02
#define CES_CMDIF_PKT_STOP 0x0B
#define DATA_LEN 12
#define ZERO 0

#define DRDY_PIN 2
#define CS_PIN 4

// Optional SPI pin overrides
#if !defined(SCK_PIN)
#if defined(ARDUINO_ARCH_ESP32)
#define SCK_PIN 18
#define MISO_PIN 19
#define MOSI_PIN 23
#else
#define SCK_PIN 13
#define MISO_PIN 12
#define MOSI_PIN 11
#endif
#endif

ads1293 ADS1293(DRDY_PIN, CS_PIN);

volatile uint8_t DataPacket[DATA_LEN];
const uint8_t DataPacketFooter[2] = {ZERO, CES_CMDIF_PKT_STOP};
const uint8_t DataPacketHeader[5] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, DATA_LEN, ZERO, CES_CMDIF_TYPE_DATA};

void sendDataThroughUart(int32_t ecgCh1, int32_t ecgCh2, int32_t ecgCh3) {
	// pack little-endian 32-bit values
	DataPacket[0] = static_cast<uint8_t>(ecgCh1 & 0xFF);
	DataPacket[1] = static_cast<uint8_t>((ecgCh1 >> 8) & 0xFF);
	DataPacket[2] = static_cast<uint8_t>((ecgCh1 >> 16) & 0xFF);
	DataPacket[3] = static_cast<uint8_t>((ecgCh1 >> 24) & 0xFF);

	DataPacket[4] = static_cast<uint8_t>(ecgCh2 & 0xFF);
	DataPacket[5] = static_cast<uint8_t>((ecgCh2 >> 8) & 0xFF);
	DataPacket[6] = static_cast<uint8_t>((ecgCh2 >> 16) & 0xFF);
	DataPacket[7] = static_cast<uint8_t>((ecgCh2 >> 24) & 0xFF);

	DataPacket[8] = static_cast<uint8_t>(ecgCh3 & 0xFF);
	DataPacket[9] = static_cast<uint8_t>((ecgCh3 >> 8) & 0xFF);
	DataPacket[10] = static_cast<uint8_t>((ecgCh3 >> 16) & 0xFF);
	DataPacket[11] = static_cast<uint8_t>((ecgCh3 >> 24) & 0xFF);

	// send packet header
	for (int i = 0; i < 5; ++i) {
		Serial.write(DataPacketHeader[i]);
	}

	// send actual data
	for (int i = 0; i < DATA_LEN; ++i) {
		Serial.write(DataPacket[i]);
	}

	// send packet footer
	for (int i = 0; i < 2; ++i) {
		Serial.write(DataPacketFooter[i]);
	}
}

void setup() {
	Serial.begin(57600);
#if defined(ARDUINO_ARCH_ESP32)
	ADS1293.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
#else
	ADS1293.begin();
#endif
	// Configure device for 5-lead ECG using explicit helpers
	ADS1293.configureChannel1(FlexCh1Mode::Default);
	ADS1293.configureChannel2(FlexCh2Mode::Default);
	ADS1293.configureChannel3(FlexCh3Mode::Default);
	ADS1293.enableCommonModeDetection(CMDetMode::Enabled);
	ADS1293.configureRLD(RLDMode::Default);
	ADS1293.configureWilsonCentralTerminal();  // Required for 5-lead ECG
	ADS1293.configureOscillator(OscMode::Default);
	ADS1293.configureAFEShutdown(AFEShutdownMode::AllEnabled);
	ADS1293.setSamplingRate(ADS1293::SamplingRate::SPS_128);
	ADS1293.configureDRDYSource(DRDYSource::Default);
	ADS1293.configureChannelConfig(ChannelConfig::Default5Lead);
	ADS1293.applyGlobalConfig(GlobalConfig::Start);

	delay(10);
}

void loop() {
	if (ADS1293.isDataReady()) {
		auto samples = ADS1293.getECGData();
		if (samples.ok) {
			sendDataThroughUart(samples.ch1, samples.ch2, samples.ch3);
		}
	}
}
