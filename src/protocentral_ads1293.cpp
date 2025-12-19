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
//////////////////////////////////////////////////////////////////////////////////////////

#include "protocentral_ads1293.h"

ADS1293::ADS1293(uint8_t drdyPin, uint8_t csPin, SPIClass *spi) noexcept
	: drdyPin_(drdyPin), csPin_(csPin), spi_(spi) {}

void ADS1293::begin(bool startSPI)
{
	pinMode(drdyPin_, INPUT_PULLUP);
	pinMode(csPin_, OUTPUT);
	digitalWrite(csPin_, HIGH);
	if (startSPI && spi_)
	{
		spi_->begin();
	}
}

void ADS1293::begin(uint8_t sck, uint8_t miso, uint8_t mosi)
{
	pinMode(drdyPin_, INPUT_PULLUP);
	pinMode(csPin_, OUTPUT);
	digitalWrite(csPin_, HIGH);
	if (spi_)
	{
		// Some cores implement SPI.begin(SCK, MISO, MOSI). Only call that overload
		// on platforms that provide it (ESP32). Otherwise fall back to spi_->begin().
#if defined(ARDUINO_ARCH_ESP32)
		spi_->begin(sck, miso, mosi);
#else
		spi_->begin();
#endif
	}
}

bool ADS1293::writeRegister(Register reg, uint8_t value) noexcept
{
	if (!spi_)
		return false;
	uint8_t addr = static_cast<uint8_t>(reg) & WREG_MASK;
	spi_->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
	digitalWrite(csPin_, LOW);
	spi_->transfer(addr);
	spi_->transfer(value);
	digitalWrite(csPin_, HIGH);
	spi_->endTransaction();
	delayMicroseconds(10);
	return true;
}

bool ADS1293::readRegister(Register reg, uint8_t &value) noexcept
{
	if (!spi_)
		return false;
	uint8_t cmd = static_cast<uint8_t>(reg) | RREG_FLAG;
	spi_->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
	digitalWrite(csPin_, LOW);
	spi_->transfer(cmd);
	value = spi_->transfer(0x00);
	digitalWrite(csPin_, HIGH);
	spi_->endTransaction();
	return true;
}

int32_t ADS1293::signExtend24(uint32_t value) noexcept
{
	// value is expected to be 24-bit left-aligned in LSB positions
	value &= 0xFFFFFFu;
	if (value & 0x800000u)
	{ // negative
		return static_cast<int32_t>(value | 0xFF000000u);
	}
	return static_cast<int32_t>(value);
}

bool ADS1293::getECGData(int32_t &ch1, int32_t &ch2, int32_t &ch3)
{
	// Read DATA_CH1_ECG (0x37) through DATA_CH3_ECG (0x3F) in one auto-incrementing
	// SPI transaction. This returns 9 bytes: CH1[MSB,mid,LSB], CH2[MSB,mid,LSB], CH3[MSB,mid,LSB].
	if (!spi_)
		return false;

	const uint8_t startAddr = 0x37;
	uint8_t buf[9] = {0};

	spi_->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
	digitalWrite(csPin_, LOW);
	spi_->transfer(startAddr | RREG_FLAG);
	for (int i = 0; i < 9; ++i)
	{
		buf[i] = spi_->transfer(0x00);
	}
	digitalWrite(csPin_, HIGH);
	spi_->endTransaction();

	// assemble 24-bit raw samples and sign-extend to int32
	uint32_t raw1 = (static_cast<uint32_t>(buf[0]) << 16) |
					(static_cast<uint32_t>(buf[1]) << 8) |
					static_cast<uint32_t>(buf[2]);
	uint32_t raw2 = (static_cast<uint32_t>(buf[3]) << 16) |
					(static_cast<uint32_t>(buf[4]) << 8) |
					static_cast<uint32_t>(buf[5]);
	uint32_t raw3 = (static_cast<uint32_t>(buf[6]) << 16) |
					(static_cast<uint32_t>(buf[7]) << 8) |
					static_cast<uint32_t>(buf[8]);

	ch1 = signExtend24(raw1);
	ch2 = signExtend24(raw2);
	ch3 = signExtend24(raw3);
	return true;
}

bool ADS1293::getRaw24(uint8_t channel, uint32_t &raw24)
{
	if (!spi_)
		return false;
	if (channel < 1 || channel > 3)
		return false;

	// DATA_CHn_ECG registers start at 0x37 for channel 1 (MSB). Each channel uses 3 bytes.
	// Compute start address as 0x37 + (channel-1)*3 so channel=1 -> 0x37
	const uint8_t startAddr = static_cast<uint8_t>(0x37 + ((channel - 1) * 3));
	uint8_t buf3[3] = {0};

	spi_->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
	digitalWrite(csPin_, LOW);
	spi_->transfer(startAddr | RREG_FLAG);
	for (int i = 0; i < 3; ++i)
		buf3[i] = spi_->transfer(0x00);
	digitalWrite(csPin_, HIGH);
	spi_->endTransaction();

	raw24 = (static_cast<uint32_t>(buf3[0]) << 16) | (static_cast<uint32_t>(buf3[1]) << 8) | static_cast<uint32_t>(buf3[2]);
	return true;
}

float ADS1293::rawToVoltage(int32_t signedCode, float vref, int32_t adcFullscale, float gain) noexcept
{
	if (adcFullscale == 0) adcFullscale = (1 << 23) - 1;
	return (static_cast<float>(signedCode) / static_cast<float>(adcFullscale)) * vref * gain;
}

ADS1293::Samples ADS1293::getECGData()
{
	Samples s;
	int32_t a = 0, b = 0, c = 0;
	if (getECGData(a, b, c))
	{
		s.ch1 = a;
		s.ch2 = b;
		s.ch3 = c;
		s.ok = true;
	}
	return s;
}

uint8_t ADS1293::readDeviceID()
{
	uint8_t val = 0;
	readRegister(Register::REVID, val);
	return val;
}

uint8_t ADS1293::readErrorStatus()
{
	uint8_t val = 0;
	readRegister(Register::ERR_STATUS, val);
	return val;
}

bool ADS1293::isDataReady()
{
	uint8_t status = 0;
	if (!readRegister(Register::DATA_STATUS, status))
		return false;
	// Bits 0-2 indicate new data ready for CH1-CH3
	// Return true if any ECG channel has new data
	return (status & 0x07) != 0;
}

bool ADS1293::begin3LeadECG()
{
	// perform the configuration steps in a clear, datasheet-aligned order
	if (!configureChannel1(FlexCh1Mode::Default))
		return false;
	if (!configureChannel2(FlexCh2Mode::Default))
		return false;
	if (!enableCommonModeDetection(CMDetMode::Enabled))
		return false;
	if (!configureRLD(RLDMode::Default))
		return false;
	if (!configureOscillator(OscMode::Default))
		return false;
	if (!configureAFEShutdown(AFEShutdownMode::AllEnabled))
		return false;
	if (!configureSamplingRates(R2Rate::Rate_2, R3Rate::Rate_2, R3Rate::Rate_2))
		return false;
	if (!configureDRDYSource(DRDYSource::Default))
		return false;
	if (!configureChannelConfig(ChannelConfig::Default3Lead))
		return false;
	if (!applyGlobalConfig(GlobalConfig::Start))
		return false;

	return true;
}

// --- helper implementations follow ---
bool ADS1293::configureChannel1(FlexCh1Mode m)
{
	// FLEX_CH1_CN: enable input, set gain/placement per datasheet example
	return writeRegister(Register::FLEX_CH1_CN, static_cast<uint8_t>(m));
}

bool ADS1293::configureChannel2(FlexCh2Mode m)
{
	// FLEX_CH2_CN: enable input and set channel-specific config
	return writeRegister(Register::FLEX_CH2_CN, static_cast<uint8_t>(m));
}

bool ADS1293::enableCommonModeDetection(CMDetMode m)
{
	// CMDET_EN: enable common-mode detection
	return writeRegister(Register::CMDET_EN, static_cast<uint8_t>(m));
}

bool ADS1293::configureRLD(RLDMode m)
{
	// RLD_CN: configure right leg drive
	return writeRegister(Register::RLD_CN, static_cast<uint8_t>(m));
}

bool ADS1293::configureOscillator(OscMode m)
{
	// OSC_CN: oscillator configuration
	return writeRegister(Register::OSC_CN, static_cast<uint8_t>(m));
}

bool ADS1293::configureAFEShutdown(AFEShutdownMode m)
{
	// AFE_SHDN_CN: control AFE shutdown bits
	return writeRegister(Register::AFE_SHDN_CN, static_cast<uint8_t>(m));
}

bool ADS1293::configureChannel3(FlexCh3Mode m)
{
	return writeRegister(Register::FLEX_CH3_CN, static_cast<uint8_t>(m));
}

bool ADS1293::configureRef(RefMode m)
{
	return writeRegister(Register::REF_CN, static_cast<uint8_t>(m));
}

bool ADS1293::configureSamplingRates(R2Rate r2, R3Rate r3ch1, R3Rate r3ch2)
{
	// R2_RATE and R3_RATE_CHx: sampling rate related registers
	bool ok = true;
	ok &= writeRegister(Register::R2_RATE, static_cast<uint8_t>(r2));
	ok &= writeRegister(Register::R3_RATE_CH1, static_cast<uint8_t>(r3ch1));
	ok &= writeRegister(Register::R3_RATE_CH2, static_cast<uint8_t>(r3ch2));
	return ok;
}

	// namespace removed from this TU: implementation uses global symbols

bool ADS1293::configureDRDYSource(DRDYSource m)
{
	// DRDYB_SRC: data ready source selection
	return writeRegister(Register::DRDYB_SRC, static_cast<uint8_t>(m));
}

bool ADS1293::configureChannelConfig(ChannelConfig m)
{
	// CH_CNFG: global channel configuration
	return writeRegister(Register::CH_CNFG, static_cast<uint8_t>(m));
}

bool ADS1293::applyGlobalConfig(GlobalConfig m)
{
	// CONFIG: final device configuration to start conversions
	return writeRegister(Register::CONFIG, static_cast<uint8_t>(m));
}

// begin5LeadECG removed in favor of explicit helper calls in examples.

void ADS1293::disableChannel(uint8_t channel)
{
	if (channel == 1)
	{
		writeRegister(Register::FLEX_CH1_CN, 0x00);
		delay(1);
	}
}

void ADS1293::disableFilterAll()
{
	writeRegister(Register::DIS_EFILTER, 0x07);
	delay(1);
}

bool ADS1293::disableFilter(uint8_t channel)
{
	if (channel < 1 || channel > 3)
	{
		return false;
	}
	uint8_t mask = static_cast<uint8_t>(1u << (channel - 1));
	writeRegister(Register::DIS_EFILTER, mask);
	delay(1);
	return true;
}

bool ADS1293::attachTestSignal(uint8_t channel, TestSignal sig)
{
	if (channel < 1 || channel > 3)
	{
		return false;
	}
	uint8_t value = (static_cast<uint8_t>(sig) << 6);
	// write to channel register addresses (FLEX_CHn_CN)
	Register reg = static_cast<Register>(0x00 + channel);
	writeRegister(reg, value);
	delay(1);
	return true;
}

bool ADS1293::enableTestSignalAll(TestSignal sig)
{
	bool ok = true;
	for (uint8_t ch = 1; ch <= 3; ++ch)
	{
		ok &= attachTestSignal(ch, sig);
	}
	return ok;
}

bool ADS1293::configureWilsonCentralTerminal()
{
	// Configure WCT (Wilson Central Terminal) for 5-lead ECG.
	// These registers connect the internal Wilson reference buffers to input pins.
	// Values from the original working 5-lead implementation.
	bool ok = true;
	ok &= writeRegister(Register::WILSON_EN1, 0x01);
	delay(1);
	ok &= writeRegister(Register::WILSON_EN2, 0x02);
	delay(1);
	ok &= writeRegister(Register::WILSON_EN3, 0x03);
	delay(1);
	ok &= writeRegister(Register::WILSON_CN, 0x01);
	delay(1);
	return ok;
}

bool ADS1293::setSamplingRate(ADS1293::SamplingRate s)
{
	// Configure decimation registers for the requested sampling rate.
	// ODR = fMOD / (R1 * R2 * R3), where fMOD = 102.4 kHz (default).
	//
	// R2_RATE register codes: 4->0x01, 5->0x02, 6->0x04, 8->0x08
	// R3_RATE register codes: 4->0x01, 6->0x02, 8->0x04, 12->0x08, 16->0x10, 32->0x20, 64->0x40, 128->0x80

	uint8_t r2Reg = 0x02;  // R2=5 (default for most rates)
	uint8_t r3Reg = 0x10;  // R3=16 (default for ~128 SPS)

	switch (s)
	{
	case ADS1293::SamplingRate::SPS_853:
		// 102400/(4*6*4) = 1066 SPS (closest available to 853)
		r2Reg = 0x04;  // R2=6
		r3Reg = 0x01;  // R3=4
		break;
	case ADS1293::SamplingRate::SPS_512:
		// 102400/(4*5*4) = 1280 SPS, or 102400/(4*4*5)=1280
		// For ~512: 102400/(4*5*8) = 256, need R2=4,R3=5 but 5 not available
		// Use 102400/(4*4*8) = 800 or 102400/(4*5*4)=1280/2.5 not exact
		// Best: 102400/(4*6*8) = 533 SPS
		r2Reg = 0x04;  // R2=6
		r3Reg = 0x04;  // R3=8
		break;
	case ADS1293::SamplingRate::SPS_256:
		// 102400/(4*5*8) = 256 SPS
		r2Reg = 0x02;  // R2=5
		r3Reg = 0x04;  // R3=8
		break;
	case ADS1293::SamplingRate::SPS_128:
		// 102400/(4*5*16) = 128 SPS
		r2Reg = 0x02;  // R2=5
		r3Reg = 0x10;  // R3=16
		break;
	case ADS1293::SamplingRate::SPS_64:
		// 102400/(4*5*32) = 64 SPS
		r2Reg = 0x02;  // R2=5
		r3Reg = 0x20;  // R3=32
		break;
	case ADS1293::SamplingRate::SPS_32:
		// 102400/(4*5*64) = 32 SPS
		r2Reg = 0x02;  // R2=5
		r3Reg = 0x40;  // R3=64
		break;
	default:
		return false;
	}

	bool ok = true;
	ok &= writeRegister(Register::R2_RATE, r2Reg);
	delay(1);
	ok &= writeRegister(Register::R3_RATE_CH1, r3Reg);
	delay(1);
	ok &= writeRegister(Register::R3_RATE_CH2, r3Reg);
	delay(1);
	ok &= writeRegister(Register::R3_RATE_CH3, r3Reg);
	delay(1);

	return ok;
}
