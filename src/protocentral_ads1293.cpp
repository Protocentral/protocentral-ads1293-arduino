//////////////////////////////////////////////////////////////////////////////////////////
// Protocentral ADS1293 - implementation
// https://github.com/Protocentral/protocentral-ads1293-arduino
// Copyright (c) 2020 ProtoCentral
// Licensed under the MIT License
//
// Implementation notes:
//  - Keep public API in the header. This TU focuses on small formatting
//    and style cleanups. No public API renames are performed here.
//  - Suggested non-breaking renames documented in TODO below.
//////////////////////////////////////////////////////////////////////////////////////////

#include "protocentral_ads1293.h"

// TODO (non-breaking): consider renaming internal helpers for clarity:
//  - signExtend24 -> raw24_to_signed32
//  - getRaw24 -> readRaw24 or readRawSample24
//  - setSamplingRate -> configureSamplingRate

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

bool ADS1293::readSampleBytes(uint8_t outBuf[9])
{
	if (!spi_)
		return false;
	const uint8_t startAddr = 0x37;

	spi_->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
	digitalWrite(csPin_, LOW);
	spi_->transfer(startAddr | RREG_FLAG);
	for (int i = 0; i < 9; ++i)
		outBuf[i] = spi_->transfer(0x00);
	digitalWrite(csPin_, HIGH);
	spi_->endTransaction();
	return true;
}

bool ADS1293::dumpDebug(Print &out)
{
	if (!spi_)
		return false;
	uint8_t rev = 0, err = 0;
	if (!readRegister(Register::REVID, rev))
		return false;
	if (!readRegister(Register::ERR_STATUS, err))
		return false;
	uint8_t buf9[9] = {0};
	if (!readSampleBytes(buf9))
		return false;

	out.print(F("REVID=0x"));
	if (rev < 16)
		out.print('0');
	out.println(rev, HEX);
	out.print(F("ERR=0x"));
	if (err < 16)
		out.print('0');
	out.println(err, HEX);
	out.print(F("SAMPLES:"));
	for (int i = 0; i < 9; ++i) {
		out.print(' ');
		uint8_t v = buf9[i];
		if (v < 16)
			out.print('0');
		out.print(v, HEX);
	}
	out.println();
	return true;
}

	// interpretRaw24 removed: library now always interprets ADC output as
	// two's-complement 24-bit. Use signExtend24() for conversion.

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
	if (!configureAFEShutdown(AFEShutdownMode::Default))
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

bool ADS1293::setChannelGainRaw(uint8_t channel, uint8_t regValue)
{
	if (channel < 1 || channel > 3) return false;
	// CH1SET @ 0x0A, CH2SET @ 0x0B, CH3SET @ 0x0C
	Register reg = static_cast<Register>(0x0A + (channel - 1));
	bool ok = writeRegister(reg, regValue);
	delay(1);
	return ok;
}

bool ADS1293::setChannelGain(uint8_t channel, ADS1293::PgaGain gain)
{
	return setChannelGainRaw(channel, static_cast<uint8_t>(gain));
}

bool ADS1293::setSamplingRate(ADS1293::SamplingRate s)
{
	// Implement ODR configuration by programming the decimation stages
	// R1 (0x25), R2 (0x21) and R3 (0x22/0x23/0x24) according to the datasheet.
	// Algorithm: search allowed R1/R2/R3 combinations and pick the combination
	// whose resulting ODR (fS/(R1*R2*R3)) is closest to the requested value.
	// We assume the SDM clock fS = 102400 Hz by default (AFE_RES FS_HIGH = 0).

	// Determine SDM clock (fS). Default is 102.4 kHz, but if the AFE_RES
	// register indicates high-rate mode (FS_HIGH) the SDM clock is doubled
	// to 204.8 kHz. Read the AFE_RES register to detect this.
	float fs = 102400.0f; // default SDM clock per-channel when FS_HIGH=0
	uint8_t afeRes = 0;
	if (readRegister(Register::AFE_RES, afeRes)) {
		// Datasheet labels a bit (FS_HIGH) in AFE_RES that enables higher SDM
		// clock. Use bit mask 0x01 here (common mapping). If your hardware
		// uses a different bit, we can adjust once you provide the register
		// value from `readRegister(Register::AFE_RES)`.
		if (afeRes & 0x01u) {
			fs = 204800.0f;
		}
	}

	// numeric target ODR for each supported enum (R1=4,R2=4; vary R3)
	float targetHz = 0.0f;
	switch (s)
	{
	case ADS1293::SamplingRate::SPS_1600: targetHz = 1600.0f; break;         // R3=4
	case ADS1293::SamplingRate::SPS_1067: targetHz = 1066.6667f; break;    // R3=6
	case ADS1293::SamplingRate::SPS_800:  targetHz = 800.0f; break;         // R3=8
	case ADS1293::SamplingRate::SPS_533:  targetHz = 533.3333f; break;      // R3=12
	case ADS1293::SamplingRate::SPS_400:  targetHz = 400.0f; break;         // R3=16
	case ADS1293::SamplingRate::SPS_200:  targetHz = 200.0f; break;         // R3=32
	case ADS1293::SamplingRate::SPS_100:  targetHz = 100.0f; break;         // R3=64
	case ADS1293::SamplingRate::SPS_50:   targetHz = 50.0f; break;          // R3=128
	default: return false;
	}

	// Fix R1=4 and R2=4 per requirement. Only vary R3 from allowed candidates.
	const uint8_t r1 = 4;
	const uint8_t r2 = 4;
	const uint8_t r3Candidates[] = {4, 6, 8, 12, 16, 32, 64, 128};

	auto r2Code = [](uint8_t v) -> uint8_t {
		switch (v) {
		case 4: return 0x01;
		case 5: return 0x02;
		case 6: return 0x04;
		case 8: return 0x08;
		default: return 0x00;
		}
	};
	auto r3Code = [](uint8_t v) -> uint8_t {
		switch (v) {
		case 4: return 0x01;
		case 6: return 0x02;
		case 8: return 0x04;
		case 12: return 0x08;
		case 16: return 0x10;
		case 32: return 0x20;
		case 64: return 0x40;
		case 128: return 0x80;
		default: return 0x00;
		}
	};

	// Choose the R3 candidate that best matches targetHz with R1=4,R2=4
	uint8_t chosenR3 = r3Candidates[0];
	float bestErr = 1e9f;
	for (uint8_t r3 : r3Candidates) {
		float odr = fs / (static_cast<float>(r1) * static_cast<float>(r2) * static_cast<float>(r3));
		float err = fabsf(odr - targetHz);
		if (err < bestErr) {
			bestErr = err;
			chosenR3 = r3;
		}
		if (err == 0.0f) break;
	}

	uint8_t r1Reg = 0x00; // R1=4 -> bits cleared
	uint8_t r2Reg = r2Code(r2); // r2=4 -> 0x01
	uint8_t r3Reg = r3Code(chosenR3);

	bool ok = true;
	ok &= writeRegister(Register::R1_RATE, r1Reg);
	ok &= writeRegister(Register::R2_RATE, r2Reg);
	ok &= writeRegister(Register::R3_RATE_CH1, r3Reg);
	ok &= writeRegister(Register::R3_RATE_CH2, r3Reg);
	ok &= writeRegister(Register::R3_RATE_CH3, r3Reg);
	delay(1);

	// Verify writes by reading back at least R2 and R3
	uint8_t verify = 0;
	if (!readRegister(Register::R2_RATE, verify)) return false;
	if (verify != r2Reg) return false;
	if (!readRegister(Register::R3_RATE_CH1, verify)) return false;
	if (verify != r3Reg) return false;

	return ok;
}
