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
//  |pin label         | Pin Function         |Arduino Connection|
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

// Uncomment to print voltages instead of raw ADC codes
// #define PRINT_VOLTAGE

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

using namespace protocentral;

ads1293 ADS1293(DRDY_PIN, CS_PIN);

void setup() {
  Serial.begin(115200);
#if defined(ARDUINO_ARCH_ESP32)
  ADS1293.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
#else
  ADS1293.begin();
#endif
  // Configure device for 5-lead ECG using fine-grained helpers
  ADS1293.configureChannel1(FlexCh1Mode::Default);
  ADS1293.configureChannel2(FlexCh2Mode::Default);
  ADS1293.configureChannel3(FlexCh3Mode::Default);
  ADS1293.enableCommonModeDetection(CMDetMode::Enabled);
  ADS1293.configureRLD(RLDMode::Default);
  ADS1293.configureRef(RefMode::Default);
  ADS1293.configureAFEShutdown(AFEShutdownMode::AFE_On);
  // Set PGA gain=8 for all channels
  ADS1293.setChannelGain(1, Ads1293::PgaGain::G8);
  ADS1293.setChannelGain(2, Ads1293::PgaGain::G8);
  ADS1293.setChannelGain(3, Ads1293::PgaGain::G8);
  // Ensure example runs at 100 SPS using the DRATE-based API
  ADS1293.setSamplingRate(Ads1293::SamplingRate::SPS_100);
  ADS1293.configureDRDYSource(DRDYSource::Default);
  ADS1293.configureChannelConfig(ChannelConfig::Default5Lead);
  ADS1293.applyGlobalConfig(GlobalConfig::Start);
  delay(10);
}

void loop() {
  if (digitalRead(DRDY_PIN) == LOW) {
    // Use the convenience overload that returns Samples
    auto samples = ADS1293.getECGData();
    if (samples.ok) {
      #ifdef PRINT_VOLTAGE
      uint32_t raw1=0, raw2=0, raw3=0;
      if (ADS1293.getRaw24(1, raw1) && ADS1293.getRaw24(2, raw2) && ADS1293.getRaw24(3, raw3)) {
        int32_t s1 = Ads1293::interpretRaw24(raw1, /*offsetBinary=*/false);
        int32_t s2 = Ads1293::interpretRaw24(raw2, /*offsetBinary=*/false);
        int32_t s3 = Ads1293::interpretRaw24(raw3, /*offsetBinary=*/false);
        float v1 = Ads1293::rawToVoltage(s1);
        float v2 = Ads1293::rawToVoltage(s2);
        float v3 = Ads1293::rawToVoltage(s3);
        Serial.print(v1, 6); Serial.print(',');
        Serial.print(v2, 6); Serial.print(',');
        Serial.println(v3, 6);
      }
      #else
      // print CSV ch1,ch2,ch3 (raw signed codes)
      Serial.print(samples.ch1);
      Serial.print(',');
      Serial.print(samples.ch2);
      Serial.print(',');
      Serial.println(samples.ch3);
      #endif
    }
  }
}
