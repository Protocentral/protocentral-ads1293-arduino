/////////////////////////////////////////////////////////////////////////////////////////

//  Demo code for the ADS1293 board
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


#pragma once

#include <Arduino.h>
#include <SPI.h>

// (no namespace) public API placed in the global namespace for compatibility

// Commands used to form read/write transfer bytes.
constexpr uint8_t WREG_MASK = 0x7F; // used to mask address for write
constexpr uint8_t RREG_FLAG = 0x80; // OR with address for read

enum class Register : uint8_t {
  CONFIG = 0x00,
  FLEX_CH1_CN = 0x01,
  FLEX_CH2_CN = 0x02,
  FLEX_CH3_CN = 0x03,
  FLEX_PACE_CN = 0x04,
  FLEX_VBAT_CN = 0x05,
  LOD_CN = 0x06,
  LOD_EN = 0x07,
  LOD_CURRENT = 0x08,
  LOD_AC_CN = 0x09,
  CMDET_EN = 0x0A,
  CMDET_CN = 0x0B,
  RLD_CN = 0x0C,
  REF_CN = 0x11,
  OSC_CN = 0x12,
  AFE_RES = 0x13,
  AFE_SHDN_CN = 0x14,
  AFE_FAULT_CN = 0x15,
  AFE_PACE_CN = 0x17,
  ERR_STATUS = 0x19,
  MASK_ERR = 0x2A,
  R2_RATE = 0x21,
  R3_RATE_CH1 = 0x22,
  R3_RATE_CH2 = 0x23,
  R3_RATE_CH3 = 0x24,
  R1_RATE = 0x25,
  DIS_EFILTER = 0x26,
  DRDYB_SRC = 0x27,
  SYNCB_CN = 0x28,
  CH_CNFG = 0x2F,
  REVID = 0x40
};

// Option enums for configuration helpers. Values map to register payloads used
// by the original implementation. We provide explicit names so callers can
// select options in a readable way.
enum class FlexCh1Mode : uint8_t { Default = 0x11 };
enum class FlexCh2Mode : uint8_t { Default = 0x19 };
enum class CMDetMode : uint8_t { Enabled = 0x07 };
enum class RLDMode : uint8_t { Default = 0x04 };
enum class OscMode : uint8_t { Default = 0x04 };
enum class AFEShutdownMode : uint8_t { Default = 0x24, AFE_On = 0x00 };
enum class R2Rate : uint8_t { Rate_2 = 0x02 };
enum class R3Rate : uint8_t { Rate_2 = 0x02 };
enum class DRDYSource : uint8_t { Default = 0x08 };
enum class ChannelConfig : uint8_t { Default3Lead = 0x30, Default5Lead = 0x70 };
enum class GlobalConfig : uint8_t { Start = 0x01 };

// FLEX_CH3 register mode (used by 5-lead example)
enum class FlexCh3Mode : uint8_t { Default = 0x2E };

// Reference configuration register helper
enum class RefMode : uint8_t { Default = 0x01 };

enum class TestSignal : uint8_t {
  Positive = 0x01,
  Negative = 0x02,
  Zero = 0x03
};

class ADS1293 {
public:
  // Construct with DRDY and CS pins. Use begin() to initialize hardware.
  explicit ADS1293(uint8_t drdyPin, uint8_t csPin, SPIClass *spi = &SPI) noexcept;

  // Initialize pins and optionally start SPI. Must be called in setup().
  // Pass startSPI=false on platforms that need custom SPI pin setup (e.g. some ESP32 configs).
  void begin(bool startSPI = true);

  // Initialize pins and start SPI using the provided SCK/MISO/MOSI pins
  // (convenience for platforms like ESP32 where SPI.begin(sck, miso, mosi) is common).
  void begin(uint8_t sck, uint8_t miso, uint8_t mosi);

  // Configuration helpers
  bool begin3LeadECG();

  // Read all three ECG channels. Returns true on success and writes signed
  // 24-bit samples (as int32_t) into the three output references.
  // This performs a single multi-byte SPI read from DATA_CH1_ECG..DATA_CH3_ECG.
  bool getECGData(int32_t &ch1, int32_t &ch2, int32_t &ch3);

  // Convenience POD returned by the no-argument getECGData() overload.
  // `ok` is true when the SPI read succeeded and channels contain valid data.
  struct Samples {
    int32_t ch1 = 0;
    int32_t ch2 = 0;
    int32_t ch3 = 0;
    bool ok = false;
  };

  // Convenience overload: returns a Samples struct containing the three
  // channel values and a boolean `ok` flag. This is useful for compact code
  // where reference parameters are inconvenient.
  Samples getECGData();

  // Raw access and conversion helpers
  // Read the raw 24-bit unsigned sample for channel (1..3). Returns true on success.
  bool getRaw24(uint8_t channel, uint32_t &raw24);

  // Read the raw sample bytes for all three channels (9 bytes: ch1[MSB..LSB], ch2[MSB..LSB], ch3[MSB..LSB]).
  // Useful for diagnostic/debug printing of the raw SPI payload.
  bool readSampleBytes(uint8_t buf[9]);

  // Dump a small set of diagnostic registers and the latest sample bytes to the provided Print
  // (e.g., `Serial`). This prints REVID, ERR_STATUS and the 9 sample bytes in hex.
  bool dumpDebug(Print &out);

  // Convert a 24-bit unsigned raw value to signed int32 using two's-complement
  // sign-extension. This library always interprets ADC output as two's-complement
  // 24-bit by default.
  static int32_t signExtend24(uint32_t raw24) noexcept;

  // Convert a signed code to a voltage (V). adcFullscale defaults to 2^23-1.
  static float rawToVoltage(int32_t signedCode, float vref = 2.4f, int32_t adcFullscale = ((1 << 23) - 1), float gain = 1.0f) noexcept;

  // Device information
  uint8_t readDeviceID();
  uint8_t readErrorStatus();

  // Channel and filter helpers
  void disableChannel(uint8_t channel);
  void disableFilterAll();
  bool disableFilter(uint8_t channel);

  // Helpers used by begin3LeadECG to configure device in readable steps
  // These are private implementation details; exposed here to keep the
  // header self-documenting. They perform single responsibilities that map
  // closely to the datasheet register names.
  bool configureChannel1(FlexCh1Mode m = FlexCh1Mode::Default);
  bool configureChannel2(FlexCh2Mode m = FlexCh2Mode::Default);
  bool configureChannel3(FlexCh3Mode m = FlexCh3Mode::Default);
  bool enableCommonModeDetection(CMDetMode m = CMDetMode::Enabled);
  bool configureRLD(RLDMode m = RLDMode::Default);
  bool configureOscillator(OscMode m = OscMode::Default);
  bool configureAFEShutdown(AFEShutdownMode m = AFEShutdownMode::Default);
  bool configureRef(RefMode m = RefMode::Default);
  bool configureSamplingRates(R2Rate r2 = R2Rate::Rate_2, R3Rate r3ch1 = R3Rate::Rate_2, R3Rate r3ch2 = R3Rate::Rate_2);
  bool configureDRDYSource(DRDYSource m = DRDYSource::Default);
  bool configureChannelConfig(ChannelConfig m = ChannelConfig::Default3Lead);
  bool applyGlobalConfig(GlobalConfig m = GlobalConfig::Start);

  // Attach built-in test signal to a channel (1..3)
  bool attachTestSignal(uint8_t channel, TestSignal sig);

  // Enable and route the built-in test signal to all channels (1..3).
  // Returns true if all channel routes were configured successfully.
  bool enableTestSignalAll(TestSignal sig);

  // PGA gain helpers
  // Raw write to CHnSET register (addresses 0x0A,0x0B,0x0C for channels 1..3).
  bool setChannelGainRaw(uint8_t channel, uint8_t regValue);

  // Convenience enum and wrapper for common gain presets.
  enum class PgaGain : uint8_t {
    G1 = 0x00,
    G4 = 0x08, // example mapping from datasheet/example
    G6 = 0x10,
  G12 = 0x18,
  G8 = 0x0C
  };
  bool setChannelGain(uint8_t channel, PgaGain gain);

  // Sampling rate presets (output data rate, ODR) supported by setSamplingRate().
  // These mappings assume the sigma-delta modulator clock fS = 102.4 kHz and
  // R1 = 4 (default). The function programs R2 and R3 registers for all three
  // ECG channels. If you change the AFE_RES (FS_HIGH) or R1_RATE, the resulting
  // ODR will change.
  enum class SamplingRate : uint8_t {
  // Added high-rate DRATE options (DR[3:0] mapping)
  SPS_25600, // DR = 0x00 -> 25.6 kSPS
  SPS_12800, // DR = 0x01 -> 12.8 kSPS
  SPS_6400,  // DR = 0x02 -> 6.4 kSPS
  SPS_3200,  // DR = 0x03 -> 3.2 kSPS
    SPS_1600,
    SPS_1280,
    SPS_1066,
    SPS_853,
    SPS_800,
    SPS_640,
    SPS_533,
    SPS_426,
    SPS_400,
    SPS_320,
    SPS_266,
    SPS_200,
    SPS_160,
    SPS_100,
    SPS_50,
    SPS_25
  };

  // Configure R2/R3 rate registers for the requested output data rate (ODR).
  // Returns true if all rate registers were written successfully.
  bool setSamplingRate(SamplingRate s);

private:
  uint8_t drdyPin_ = 255;
  uint8_t csPin_ = 255;
  SPIClass *spi_ = nullptr;

  // low-level register access
  bool writeRegister(Register reg, uint8_t value) noexcept;
  bool readRegister(Register reg, uint8_t &value) noexcept;

  // helper to convert a raw 24-bit unsigned value into signed int32_t
  // (declaration above is public; no duplicate private declaration needed)
};

// Backwards compatibility alias for existing sketches that used lowercase class name.
using ads1293 = ADS1293;

// PgaGain and SamplingRate are nested inside ADS1293; provide simple aliases
// for convenience (left in global scope).
using PgaGain = ADS1293::PgaGain;
using SamplingRate = ADS1293::SamplingRate;
