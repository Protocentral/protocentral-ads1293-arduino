//////////////////////////////////////////////////////////////////////////////////////////
//
//  Protocentral ADS1293 Arduino example — 3-lead ECG (Arduino Plotter)
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

#define DRDY_PIN 2
#define CS_PIN 4

// Optional: override these before build to change SPI pins. Defaults follow
// common Uno pins, and typical VSPI pins for ESP32.
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

void setup()
{
	Serial.begin(115200);

#if defined(ARDUINO_ARCH_ESP32)
	ADS1293.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
#else
	ADS1293.begin();
#endif

	// Configure device for 3-lead ECG
	ADS1293.configureChannel1(FlexCh1Mode::Default);
	ADS1293.configureChannel2(FlexCh2Mode::Default);
	ADS1293.enableCommonModeDetection(CMDetMode::Enabled);
	ADS1293.configureRLD(RLDMode::Default);
	ADS1293.configureOscillator(OscMode::Default);
	ADS1293.configureAFEShutdown(AFEShutdownMode::AllEnabled);
	ADS1293.setSamplingRate(ADS1293::SamplingRate::SPS_128);
	ADS1293.configureDRDYSource(DRDYSource::Default);
	ADS1293.configureChannelConfig(ChannelConfig::Default3Lead);
	ADS1293.applyGlobalConfig(GlobalConfig::Start);

	delay(10);
}

void loop()
{
	// Poll DATA_STATUS register to check if new data is available
	if (ADS1293.isDataReady())
	{
		auto samples = ADS1293.getECGData();
		if (samples.ok)
		{
			Serial.print(samples.ch1);
			Serial.print(',');
			Serial.print(samples.ch2);
			Serial.print(',');
			Serial.println(samples.ch3);
		}
	}
}
