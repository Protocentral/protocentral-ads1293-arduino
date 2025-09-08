//////////////////////////////////////////////////////////////////////////////////////////
//
//  Demo code for the ads1293 board
//
//  Author: Joice Tm
//  This example will plot the ECG through openview processing GUI.
//  GUI URL: https://github.com/Protocentral/protocentral_openview.git
//
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

#define CES_CMDIF_PKT_START_1 0x0A
#define CES_CMDIF_PKT_START_2 0xFA
#define CES_CMDIF_TYPE_DATA 0x02
#define CES_CMDIF_PKT_STOP 0x0B
#define DATA_LEN 12
#define ZERO 0

#define DRDY_PIN 2
#define CS_PIN 10

// Uncomment to send voltages instead of raw ADC codes
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

// Uncomment to enable debug register/sample dumps on startup
// #define ENABLE_DEBUG

void setup() {
  Serial.begin(115200);
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
  ADS1293.configureRef(RefMode::Default);
  ADS1293.configureAFEShutdown(AFEShutdownMode::AFE_On);
  // Ensure example runs at 100 SPS using the DRATE-based API
  ADS1293.setSamplingRate(Ads1293::SamplingRate::SPS_25);
  ADS1293.configureDRDYSource(DRDYSource::Default);
  ADS1293.configureChannelConfig(ChannelConfig::Default5Lead);
  ADS1293.applyGlobalConfig(GlobalConfig::Start);
  delay(10);

#if defined(ENABLE_DEBUG)
  ADS1293.dumpDebug(Serial);
#endif
}

void loop() {
  if (digitalRead(DRDY_PIN) == LOW) {
    // Use convenience overload to get all three channels in one call
    auto samples = ADS1293.getECGData();
    if (samples.ok) {
        // Read raw 24-bit values from the device, sign-extend to int32_t,
        // and send as 32-bit little-endian (LSB first) in the packet payload.
        uint32_t raw1 = 0, raw2 = 0, raw3 = 0;
        if (ADS1293.getRaw24(1, raw1) && ADS1293.getRaw24(2, raw2) && ADS1293.getRaw24(3, raw3)) {
          int32_t s1 = Ads1293::interpretRaw24(raw1, /*offsetBinary=*/false);
          int32_t s2 = Ads1293::interpretRaw24(raw2, /*offsetBinary=*/false);
          int32_t s3 = Ads1293::interpretRaw24(raw3, /*offsetBinary=*/false);
          sendDataThroughUart(s1, s2, s3);
        }
    }
  }
}
