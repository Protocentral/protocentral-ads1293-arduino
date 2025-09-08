//////////////////////////////////////////////////////////////////////////////////////////
//
//  Demo code for the ads1293 board
//
//  Author: Joice Tm
//  This example plots the ecg through arduino plotter.
//  Copyright (c) 2020 ProtoCentral
//
//  Arduino uno connections:
//
//  |MAX30003 pin label| Pin Function         |Arduino Connection|
//  |----------------- |:--------------------:|-----------------:|
//  | MISO             | Slave Out            |  12              |
//  | MOSI             | Slave In             |  11              |
//  | SCLK             | Serial Clock         |  13              |
//  | CS               | Chip Select          |  10              |
//  | VCC              | Digital VDD          |  +5V             |
//  | GND              | Digital Gnd          |  Gnd             |
//  | DRDY             | Data ready           |  02              |
//
//  This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
//  NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//  For information on how to use, visit https://github.com/Protocentral/protocentral-ads1293-arduino
//
/////////////////////////////////////////////////////////////////////////////////////////

#include "protocentral_ads1293.h"
#include <SPI.h>

#define DRDY_PIN 2
#define CS_PIN 10

// Uncomment to route the built-in positive test signal to all channels
// #define ENABLE_TEST_SIGNAL 0

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

using namespace protocentral;

ads1293 ADS1293(DRDY_PIN, CS_PIN);

// Uncomment to enable debug register/sample dumps on startup and before streaming
// #define ENABLE_DEBUG

void setup()
{
	Serial.begin(115200);
	// Start SPI appropriately for the platform. On ESP32 use the SCK/MISO/MOSI overload
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
	// Ensure the analog front-end is powered on so conversions produce valid data
	ADS1293.configureAFEShutdown(AFEShutdownMode::AFE_On);
	// Use the new high-level sampling-rate API and set default ODR to 100 SPS
	ADS1293.setSamplingRate(Ads1293::SamplingRate::SPS_100);
	ADS1293.configureDRDYSource(DRDYSource::Default);
	ADS1293.configureChannelConfig(ChannelConfig::Default3Lead);
	ADS1293.applyGlobalConfig(GlobalConfig::Start);

#if defined(ENABLE_DEBUG)
	// Dump ID, error status and a sample read for debugging wiring/SPI
	ADS1293.dumpDebug(Serial);
#endif

#if defined(ENABLE_TEST_SIGNAL)
	// enable positive test signal on all channels for validation
	ADS1293.enableTestSignalAll(TestSignal::Positive);
#endif

	delay(10);
}

void loop()
{
	// check DRDY pin and read channel 1
	if (digitalRead(DRDY_PIN) == LOW)
	{
		// Use the convenience overload that returns a Samples POD
		auto samples = ADS1293.getECGData();
		if (samples.ok)
		{
			// Print all three channels as CSV so the Arduino plotter or a host
			// application can parse them easily: ch1,ch2,ch3
			Serial.print(samples.ch1);
			ADS1293.configureAFEShutdown(AFEShutdownMode::AFE_On);
			// Set PGA gain=8 for all channels
			ADS1293.setChannelGain(1, Ads1293::PgaGain::G8);
			ADS1293.setChannelGain(2, Ads1293::PgaGain::G8);
			ADS1293.setChannelGain(3, Ads1293::PgaGain::G8);
			Serial.print(samples.ch2);
			Serial.print(',');
			Serial.println(samples.ch3);
		}
	}
}
