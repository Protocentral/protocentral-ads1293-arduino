//////////////////////////////////////////////////////////////////////////////////////////
//
//  Protocentral ADS1293 Arduino example — 3-lead ECG (Arduino Plotter)
//
//  Author: Ashwin Whitchurch, Protocentral Electronics
//  SPDX-FileCopyrightText: 2020 Protocentral Electronics
//  SPDX-License-Identifier: MIT
//
//  This example streams ECG samples to the Arduino IDE Plotter.
//
//  Hardware connections (Arduino UNO / ESP32 VSPI):
//
//  | Signal | Arduino UNO | ESP32 (VSPI default) |
//  |-------:|:------------:|:--------------------:|
//  | MISO   | 12          | 19                   |
//  | MOSI   | 11          | 23                   |
//  | SCLK   | 13          | 18                   |
//  | CS     | 4           | 4                    |
//  | VCC    | +5V         | +5V                  |
//  | GND    | GND         | GND                  |
//  | DRDY   | 2           | 2                    |
//
//  For full documentation and examples, see:
//    https://github.com/Protocentral/protocentral-ads1293-arduino
//
/////////////////////////////////////////////////////////////////////////////////////////

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

	ADS1293.configureAFEShutdown(AFEShutdownMode::AFE_On);
	ADS1293.setSamplingRate(ADS1293::SamplingRate::SPS_100);

	ADS1293.setChannelGain(1, ADS1293::PgaGain::G8);
	ADS1293.setChannelGain(2, ADS1293::PgaGain::G8);
	ADS1293.setChannelGain(3, ADS1293::PgaGain::G8);

	ADS1293.configureDRDYSource(DRDYSource::Default);
	ADS1293.configureChannelConfig(ChannelConfig::Default3Lead);
	ADS1293.applyGlobalConfig(GlobalConfig::Start);

	delay(10);
}

void loop()
{
	if (digitalRead(DRDY_PIN) == LOW)
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
	delay(10);
}
