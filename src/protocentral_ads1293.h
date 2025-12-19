//////////////////////////////////////////////////////////////////////////////////////////
//
//  Protocentral ADS1293 Arduino Library
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
//  For more information, visit https://github.com/Protocentral/protocentral-ads1293-arduino
//
//////////////////////////////////////////////////////////////////////////////////////////


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
  // Wilson Central Terminal registers (0x0D-0x10)
  WILSON_EN1 = 0x0D,
  WILSON_EN2 = 0x0E,
  WILSON_EN3 = 0x0F,
  WILSON_CN = 0x10,
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
  DATA_STATUS = 0x30,
  DATA_CH1_PACE = 0x31,
  DATA_CH2_PACE = 0x32,
  DATA_CH3_PACE = 0x33,
  DATA_CH1_ECG = 0x37,
  DATA_CH2_ECG = 0x3A,
  DATA_CH3_ECG = 0x3D,
  REVID = 0x40
};

// Option enums for configuration helpers. Values map to register payloads used
// by the original implementation and TI reference code.
//
// FLEX_CHx_CN register format (per ADS1293 datasheet):
//   Bits [7:6]: Test signal selection (00=normal operation)
//   Bits [5:3]: NEG input selection (001=IN1, 010=IN2, 011=IN3, 100=IN4, 101=IN5, 110=IN6)
//   Bits [2:0]: POS input selection (001=IN1, 010=IN2, 011=IN3, 100=IN4, 101=IN5, 110=IN6)
//
// Values from TI reference (TI2093RegSetting.py):
//   0x11 = CH1: IN2-IN1 (bits: 00 010 001) - LA-RA = Lead I
//   0x19 = CH2: IN3-IN1 (bits: 00 011 001) - LL-RA = Lead II
//   0x2E = CH3: IN5-IN6 (bits: 00 101 110) - V1-WCT
enum class FlexCh1Mode : uint8_t { Default = 0x11 };  // Lead I: LA-RA
enum class FlexCh2Mode : uint8_t { Default = 0x19 };  // Lead II: LL-RA
// CMDET_EN: Enable common-mode detection on inputs
// Bits correspond to IN1-IN6, set bit to enable CM detection on that input
// 0x07 = IN1, IN2, IN3 enabled (RA, LA, LL)
enum class CMDetMode : uint8_t { Enabled = 0x07 };

// RLD_CN: Right Leg Drive configuration
// Bits [2:0] select which input pin to drive RLD signal to
// 0x04 = RLD output to IN4 (RL electrode)
enum class RLDMode : uint8_t { Default = 0x04 };
enum class OscMode : uint8_t { Default = 0x04 };
// AFE_SHDN_CN register: 0x00 = all AFE active (no shutdown)
// Bits set to 1 shut down corresponding blocks
enum class AFEShutdownMode : uint8_t {
  AllEnabled = 0x00,       // All AFE channels active (no shutdown)
  Ch1Ch2Only = 0x04,       // CH3 shutdown, CH1+CH2 active
  AllShutdown = 0x3F       // All AFE channels shutdown
};
enum class R2Rate : uint8_t { Rate_2 = 0x02 };
enum class R3Rate : uint8_t { Rate_2 = 0x02 };
enum class DRDYSource : uint8_t { Default = 0x08 };
enum class ChannelConfig : uint8_t { Default3Lead = 0x30, Default5Lead = 0x70 };
enum class GlobalConfig : uint8_t { Start = 0x01 };

// FLEX_CH3 register mode (used by 5-lead example)
// 0x2E = IN5-IN6 (bits: 00 101 110) - V1-WCT (from TI reference)
enum class FlexCh3Mode : uint8_t { Default = 0x2E };  // V1-WCT

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

  // Convert a 24-bit unsigned raw value to signed int32 using two's-complement
  // sign-extension. This library always interprets ADC output as two's-complement
  // 24-bit by default.
  static int32_t signExtend24(uint32_t raw24) noexcept;

  // Convert a signed code to a voltage (V). adcFullscale defaults to 2^23-1.
  static float rawToVoltage(int32_t signedCode, float vref = 2.4f, int32_t adcFullscale = ((1 << 23) - 1), float gain = 1.0f) noexcept;

  // Device information
  uint8_t readDeviceID();
  uint8_t readErrorStatus();

  // Check if new data is available by reading DATA_STATUS register
  // Returns true if any channel has new data ready (bits 0-2 indicate CH1-CH3)
  bool isDataReady();

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
  bool configureAFEShutdown(AFEShutdownMode m = AFEShutdownMode::AllEnabled);
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

  // Wilson Central Terminal configuration for 5-lead ECG
  // Configures the WCT buffers that connect input pins to the Wilson reference.
  // Must be called for 5-lead ECG configurations.
  bool configureWilsonCentralTerminal();

  // Sampling rate presets (output data rate, ODR) supported by setSamplingRate().
  // ODR = 102400 / (R1 * R2 * R3). With R1=4, R2=5, varying R3 gives these rates:
  enum class SamplingRate : uint8_t {
    SPS_853,  // R1=4, R2=6, R3=4  -> 102400/(4*6*4) = 1066
    SPS_512,  // R1=4, R2=6, R3=8  -> 102400/(4*6*8) = 533
    SPS_256,  // R1=4, R2=5, R3=8  -> 102400/(4*5*8) = 256
    SPS_128,  // R1=4, R2=5, R3=16 -> 102400/(4*5*16) = 128
    SPS_64,   // R1=4, R2=5, R3=32 -> 102400/(4*5*32) = 64
    SPS_32    // R1=4, R2=5, R3=64 -> 102400/(4*5*64) = 32
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

// SamplingRate is nested inside ADS1293; provide simple alias for convenience.
using SamplingRate = ADS1293::SamplingRate;
