// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2020-2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// Arduino driver for the TI ADS1293 three-channel 24-bit ECG analog front end.
//
// https://github.com/Protocentral/protocentral-ads1293-arduino

#include "Protocentral_ADS1293.h"

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
#define ADS1293_ISR_ATTR IRAM_ATTR
#else
#define ADS1293_ISR_ATTR
#endif

static const uint8_t READ_BIT          = 0x80;
static const uint8_t MAX_FRAME_LEN     = 16;     // status + 3 x pace (2 bytes) + 3 x ECG (3 bytes)
static const uint8_t SETTLE_SAMPLES    = 6;      // SINC filters settle within 6 output samples after a change
static const uint8_t AVERAGE_SAMPLES   = 4;      // samples averaged by readSupplyVoltage()
static const float   VREF_VOLTS        = 2.4f;   // internal reference
static const float   INA_GAIN          = 3.5f;   // fixed instrumentation-amplifier gain
static const float   VBAT_GAIN         = 3.246f; // battery-monitor gain factor (datasheet equation 1)

// Data-rate presets: R2_RATE and R3_RATE codes (ODR = 102400 / (4 x R2 x R3))
struct RatePreset {
    uint8_t r2;
    uint8_t r3;
};
static const RatePreset RATE_PRESETS[] = {
    {0x08, 0x80},  // SPS_25:   R2 = 8, R3 = 128
    {0x02, 0x80},  // SPS_40:   R2 = 5, R3 = 128
    {0x01, 0x80},  // SPS_50:   R2 = 4, R3 = 128
    {0x02, 0x40},  // SPS_80:   R2 = 5, R3 = 64
    {0x01, 0x40},  // SPS_100:  R2 = 4, R3 = 64
    {0x02, 0x20},  // SPS_160:  R2 = 5, R3 = 32
    {0x01, 0x20},  // SPS_200:  R2 = 4, R3 = 32
    {0x02, 0x10},  // SPS_320:  R2 = 5, R3 = 16
    {0x01, 0x10},  // SPS_400:  R2 = 4, R3 = 16
    {0x02, 0x04},  // SPS_640:  R2 = 5, R3 = 8
    {0x01, 0x04},  // SPS_800:  R2 = 4, R3 = 8
    {0x02, 0x02},  // SPS_853:  R2 = 5, R3 = 6
    {0x01, 0x02},  // SPS_1067: R2 = 4, R3 = 6
    {0x02, 0x01},  // SPS_1280: R2 = 5, R3 = 4
    {0x01, 0x01},  // SPS_1600: R2 = 4, R3 = 4
};
static const uint8_t NUM_RATE_PRESETS = sizeof(RATE_PRESETS) / sizeof(RATE_PRESETS[0]);

static bool isSingleBit(uint8_t v) {
    return v != 0 && (v & (v - 1)) == 0;
}

Protocentral_ADS1293* Protocentral_ADS1293::_irqInstances[ADS1293_MAX_IRQ_INSTANCES] = {nullptr, nullptr};

Protocentral_ADS1293::Protocentral_ADS1293(uint8_t csPin, int8_t drdyPin, int8_t resetPin, int8_t syncPin,
                                           SPIClass& spi)
    : _csPin(csPin),
      _drdyPin(drdyPin),
      _resetPin(resetPin),
      _syncPin(syncPin),
      _spi(&spi),
      _spiSettings(ADS1293_SPI_CLOCK, MSBFIRST, SPI_MODE0),
      _initialized(false),
      _autoDrdy(true),
      _autoSync(true),
      _syncMode(ADS1293_SyncMode::Auto),
      _lastError(ADS1293_Status::NotInitialized),
      _frameLen(1),
      _readyMask(ADS1293_STATUS_ECG_MASK | ADS1293_STATUS_PACE_MASK),
      _pending(false),
      _drdyFlag(false),
      _missed(0),
      _irqSlot(-1),
      _periodUs(5000),
      _nextPollUs(0),
      _lastPollUs(0),
      _pollLocked(false),
      _pollMissed(false),
      _anchorUs(0),
      _anchorSamples(0),
      _drdyFault(false),
      _drdyStrikes(0) {
    memset(_regs, 0, sizeof(_regs));
    memset(&_pendingSample, 0, sizeof(_pendingSample));
}

// ============================================================================
// Initialisation
// ============================================================================

bool Protocentral_ADS1293::begin(bool startSPI) {
    ADS1293_Config config;
    return begin(config, startSPI);
}

bool Protocentral_ADS1293::begin(const ADS1293_Config& config, bool startSPI) {
    _initialized = false;
    _pending     = false;
    _config      = config;
    _syncMode    = config.sync;
    _autoSync    = true;
    _drdyFault   = false;
    _drdyStrikes = 0;

    // RST and SYNC first: on the v2 breakout they reach the chip through a level
    // shifter, and an undriven line can hold the chip in reset or keep resetting its filters
    driveControlPins();

    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    if (_drdyPin >= 0) {
        // No pull-up: DRDY reaches the header through a TXB0108, which must not drive a resistive load
        pinMode(_drdyPin, INPUT);
    }

    if (startSPI) {
        _spi->begin();
    }

    uint32_t now = millis();
    if (now < ADS1293_POWERUP_MS) {
        delay(ADS1293_POWERUP_MS - now);
    }

    if (_resetPin >= 0) {
        hardwareReset();
    }

    // The chip survives an MCU reset. Stop any conversion left running by a previous
    // sketch: START_CON write-locks the clock, rate and DRDY registers
    rawWriteRegister(ADS1293_REG_CONFIG, 0x00);
    _regs[ADS1293_REG_CONFIG] = 0x00;

    uint8_t id = rawReadRegister(ADS1293_REG_REVID);
    if (id == 0x00 || id == 0xFF) {
        _lastError = ADS1293_Status::SpiError;
        return false;
    }
    if (id != ADS1293_REVID_VALUE) {
        _lastError = ADS1293_Status::WrongChipId;
        return false;
    }

    if (!applyConfig(config)) {
        return false;  // _lastError set by updateRegister()
    }

    _initialized = true;
    _lastError   = ADS1293_Status::Ok;

    if (config.autoStart) {
        if (!start()) return false;

        // Any enabled channel must produce data within a few periods, or the clock is not running
        float slowest = 0;
        for (uint8_t ch = 0; ch < 3; ch++) {
            ADS1293_Channel c = static_cast<ADS1293_Channel>(ch);
            if (!isChannelEnabled(c)) continue;
            float rate = getSampleRate(c);
            if (slowest == 0 || rate < slowest) slowest = rate;
        }
        if (slowest > 0) {
            uint32_t timeoutMs = static_cast<uint32_t>(10000.0f / slowest) + 50;
            if (!waitForConversion(timeoutMs)) {
                _lastError = ADS1293_Status::NoClock;
                return false;
            }
        }
    }
    return true;
}

#if defined(ARDUINO_ARCH_ESP32)
bool Protocentral_ADS1293::begin(int8_t sck, int8_t miso, int8_t mosi, const ADS1293_Config& config) {
    _spi->begin(sck, miso, mosi, -1);
    return begin(config, false);
}
#endif

void Protocentral_ADS1293::end() {
    disableDataReadyInterrupt();
    powerDown();
    _initialized = false;
    _lastError   = ADS1293_Status::NotInitialized;
}

bool Protocentral_ADS1293::reset() {
    // begin() pulses RSTB when it is wired, otherwise it stops the chip and rewrites every register
    ADS1293_Config config = _config;
    return begin(config, false);
}

void Protocentral_ADS1293::driveControlPins() {
    if (_resetPin >= 0) {
        digitalWrite(_resetPin, HIGH);  // set the level before enabling the driver: no glitch low
        pinMode(_resetPin, OUTPUT);
        digitalWrite(_resetPin, HIGH);
    }
    if (_syncPin >= 0) {
        if (_syncMode == ADS1293_SyncMode::Auto) {
            digitalWrite(_syncPin, HIGH);
            pinMode(_syncPin, OUTPUT);
            digitalWrite(_syncPin, HIGH);  // idle SYNCB, as the v1 board's pull-up did
        } else {
            pinMode(_syncPin, INPUT);      // master: the chip drives it; slave: the master board does
        }
    }
}

bool Protocentral_ADS1293::isConnected() {
    return readID() == ADS1293_REVID_VALUE;
}

uint8_t Protocentral_ADS1293::readID() {
    return rawReadRegister(ADS1293_REG_REVID);
}

void Protocentral_ADS1293::hardwareReset() {
    digitalWrite(_resetPin, LOW);
    delay(1);
    digitalWrite(_resetPin, HIGH);
    delay(ADS1293_CLOCK_START_MS);  // the crystal oscillator restarts after reset
}

bool Protocentral_ADS1293::applyConfig(const ADS1293_Config& c) {
    // Build the complete register image, then write all of it so the result does
    // not depend on what an earlier sketch left in the chip
    uint8_t r[ADS1293_NUM_CONFIG_REGS];
    memset(r, 0, sizeof(r));

    uint8_t channels = 0;  // bit n = channel n+1 in use
    uint8_t lodPins  = 0;

    switch (c.leadSet) {
        case ADS1293_LeadSet::ThreeLead:
            r[ADS1293_REG_FLEX_CH1_CN] = (2 << 3) | 1;  // IN2 (LA) - IN1 (RA) = Lead I
            r[ADS1293_REG_FLEX_CH2_CN] = (3 << 3) | 1;  // IN3 (LL) - IN1 (RA) = Lead II
            channels = 0x03;
            lodPins  = ADS1293_ELECTRODE_RA | ADS1293_ELECTRODE_LA | ADS1293_ELECTRODE_LL;
            break;
        case ADS1293_LeadSet::FiveLead:
            r[ADS1293_REG_FLEX_CH1_CN] = (2 << 3) | 1;
            r[ADS1293_REG_FLEX_CH2_CN] = (3 << 3) | 1;
            r[ADS1293_REG_FLEX_CH3_CN] = (5 << 3) | 6;  // IN5 (V1) - IN6 (WCT)
            r[ADS1293_REG_WILSON_EN1]  = 1;             // WCT = average of RA, LA, LL
            r[ADS1293_REG_WILSON_EN2]  = 2;
            r[ADS1293_REG_WILSON_EN3]  = 3;
            r[ADS1293_REG_WILSON_CN]   = ADS1293_WILSON_WILSONINT;
            channels = 0x07;
            lodPins  = ADS1293_ELECTRODE_RA | ADS1293_ELECTRODE_LA | ADS1293_ELECTRODE_LL | ADS1293_ELECTRODE_V1;
            break;
        case ADS1293_LeadSet::Custom:
            break;
    }

    if (c.testSignal != ADS1293_TestSignal::Off) {
        for (uint8_t ch = 0; ch < 3; ch++) {
            if (channels & (1 << ch)) {
                r[ADS1293_REG_FLEX_CH1_CN + ch] |= static_cast<uint8_t>(c.testSignal) << 6;
            }
        }
    }

    if (c.leadOff && lodPins) {
        uint16_t nA = c.leadOffCurrent > 2040 ? 2040 : c.leadOffCurrent;
        r[ADS1293_REG_LOD_CN]      = 0x00;  // DC lead-off, active
        r[ADS1293_REG_LOD_EN]      = lodPins;
        r[ADS1293_REG_LOD_CURRENT] = static_cast<uint8_t>((nA + 4) / 8);
    } else {
        r[ADS1293_REG_LOD_CN] = ADS1293_LOD_SHDN;
    }

    if (channels) {
        r[ADS1293_REG_CMDET_EN] = ADS1293_ELECTRODE_RA | ADS1293_ELECTRODE_LA | ADS1293_ELECTRODE_LL;
    }
    r[ADS1293_REG_RLD_CN] = c.rld ? static_cast<uint8_t>(ADS1293_Input::IN4) : ADS1293_RLD_SHDN;

    r[ADS1293_REG_AFE_RES] = c.highResolution ? 0x07 : 0x00;
    uint8_t shdn = 0;
    for (uint8_t ch = 0; ch < 3; ch++) {
        if (!(channels & (1 << ch))) shdn |= 0x09 << ch;  // INA and SDM of unused channels off
    }
    r[ADS1293_REG_AFE_SHDN_CN]   = shdn;
    r[ADS1293_REG_AFE_PACE_CN]   = ADS1293_PACE_SHDN;
    r[ADS1293_REG_DIGO_STRENGTH] = static_cast<uint8_t>(ADS1293_DriveStrength::High);

    uint8_t preset = static_cast<uint8_t>(c.dataRate);
    if (preset >= NUM_RATE_PRESETS) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    r[ADS1293_REG_R2_RATE]     = RATE_PRESETS[preset].r2;
    r[ADS1293_REG_R3_RATE_CH1] = RATE_PRESETS[preset].r3;
    r[ADS1293_REG_R3_RATE_CH2] = RATE_PRESETS[preset].r3;
    r[ADS1293_REG_R3_RATE_CH3] = RATE_PRESETS[preset].r3;

    r[ADS1293_REG_SYNCB_CN]     = ADS1293_SYNCB_DIS_OUT;  // chip default: SYNCB is an input
    r[ADS1293_REG_ALARM_FILTER] = 0x33;                   // chip default: 4 counts
    r[ADS1293_REG_CH_CNFG]      = ADS1293_CH_CNFG_STS_EN | static_cast<uint8_t>(channels << 4);

    // Clock first, with the datasheet sequence, then everything else
    if (!startClock(c.clock, c.clockOutput)) return false;

    for (uint8_t reg = ADS1293_REG_FLEX_CH1_CN; reg < ADS1293_NUM_CONFIG_REGS; reg++) {
        if (reg == ADS1293_REG_OSC_CN || writableMask(reg) == 0) continue;
        if (!updateRegister(reg, r[reg])) return false;
    }

    _autoDrdy = true;
    return selectDataReadySource();
}

bool Protocentral_ADS1293::startClock(ADS1293_ClockSource source, bool clockOutput) {
    // STRTCLK must be low while the source changes, and may only be set once the
    // oscillator (or the external clock) is running
    if (!updateRegister(ADS1293_REG_OSC_CN, 0x00)) return false;

    uint8_t osc = 0;
    if (source == ADS1293_ClockSource::External) {
        osc = ADS1293_OSC_SHDN_OSC;
    } else if (clockOutput) {
        osc = ADS1293_OSC_EN_CLKOUT;
    }
    if (!updateRegister(ADS1293_REG_OSC_CN, osc)) return false;
    delay(ADS1293_CLOCK_START_MS);
    return updateRegister(ADS1293_REG_OSC_CN, osc | ADS1293_OSC_STRTCLK);
}

bool Protocentral_ADS1293::waitForConversion(uint32_t timeoutMs) {
    uint32_t t0 = millis();
    while (millis() - t0 < timeoutMs) {
        uint8_t status = rawReadRegister(ADS1293_REG_DATA_STATUS);
        if (!(status & 0x01) && (status & (ADS1293_STATUS_ECG_MASK | ADS1293_STATUS_PACE_MASK))) {
            return true;
        }
        delay(1);
    }
    return false;
}

// ============================================================================
// Conversion and power
// ============================================================================

bool Protocentral_ADS1293::start() {
    uint8_t config = _regs[ADS1293_REG_CONFIG];
    if (config & ADS1293_CONFIG_PWR_DOWN) {
        // Power-down stops the oscillator: leave it and restart the clock cleanly first
        if (!updateRegister(ADS1293_REG_CONFIG, 0x00)) return false;
        uint8_t osc = _regs[ADS1293_REG_OSC_CN];
        ADS1293_ClockSource src = (osc & ADS1293_OSC_SHDN_OSC) ? ADS1293_ClockSource::External
                                                                : ADS1293_ClockSource::Crystal;
        if (!startClock(src, osc & ADS1293_OSC_EN_CLKOUT)) return false;
    }
    _pending  = false;
    _drdyFlag = false;
    resetPolling();
    return updateRegister(ADS1293_REG_CONFIG, ADS1293_CONFIG_START_CON);
}

bool Protocentral_ADS1293::stop() {
    _pending = false;
    return updateRegister(ADS1293_REG_CONFIG, _regs[ADS1293_REG_CONFIG] & ~ADS1293_CONFIG_START_CON);
}

bool Protocentral_ADS1293::standby() {
    _pending = false;
    return updateRegister(ADS1293_REG_CONFIG, ADS1293_CONFIG_STANDBY);
}

bool Protocentral_ADS1293::powerDown() {
    _pending = false;
    return updateRegister(ADS1293_REG_CONFIG, ADS1293_CONFIG_PWR_DOWN);
}

bool Protocentral_ADS1293::wakeup() {
    return start();
}

// ============================================================================
// Data rate and digital filters
// ============================================================================

bool Protocentral_ADS1293::setDataRate(ADS1293_DataRate rate) {
    uint8_t preset = static_cast<uint8_t>(rate);
    if (preset >= NUM_RATE_PRESETS) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }

    // Several locked registers: stop once instead of once per register
    bool wasRunning = isRunning();
    if (wasRunning && !stop()) return false;

    bool ok = updateRegister(ADS1293_REG_R2_RATE, RATE_PRESETS[preset].r2) &&
              updateRegister(ADS1293_REG_R3_RATE_CH1, RATE_PRESETS[preset].r3) &&
              updateRegister(ADS1293_REG_R3_RATE_CH2, RATE_PRESETS[preset].r3) &&
              updateRegister(ADS1293_REG_R3_RATE_CH3, RATE_PRESETS[preset].r3) &&
              updateRegister(ADS1293_REG_R1_RATE, 0x00) &&
              updateRegister(ADS1293_REG_AFE_RES, _regs[ADS1293_REG_AFE_RES] & 0x07) &&  // FS_HIGH off
              selectDataReadySource();

    if (wasRunning) ok = start() && ok;
    return ok;
}

bool Protocentral_ADS1293::setR2(ADS1293_R2 r2) {
    uint8_t code = static_cast<uint8_t>(r2);
    if (!isSingleBit(code) || code > 0x08) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    return updateRegister(ADS1293_REG_R2_RATE, code) && selectDataReadySource();
}

bool Protocentral_ADS1293::setR3(ADS1293_Channel ch, ADS1293_R3 r3) {
    uint8_t code = static_cast<uint8_t>(r3);
    if (!validChannel(ch) || !isSingleBit(code)) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    return updateRegister(ADS1293_REG_R3_RATE_CH1 + static_cast<uint8_t>(ch), code) && selectDataReadySource();
}

bool Protocentral_ADS1293::setDoublePaceRate(ADS1293_Channel ch, bool enable) {
    if (!validChannel(ch)) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    uint8_t bit = 1 << static_cast<uint8_t>(ch);
    return updateBits(ADS1293_REG_R1_RATE, bit, enable ? bit : 0) && selectDataReadySource();
}

bool Protocentral_ADS1293::setHighSampleClock(ADS1293_Channel ch, bool enable) {
    if (!validChannel(ch)) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    uint8_t bit = 0x08 << static_cast<uint8_t>(ch);
    return updateBits(ADS1293_REG_AFE_RES, bit, enable ? bit : 0) && selectDataReadySource();
}

bool Protocentral_ADS1293::setEcgFilterBypass(ADS1293_Channel ch, bool bypass) {
    if (!validChannel(ch)) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    uint8_t c = static_cast<uint8_t>(ch);
    // The datasheet forbids bypassing the filter of the channel that drives DRDYB
    if (bypass && !_autoDrdy && _regs[ADS1293_REG_DRDYB_SRC] == (0x08 << c)) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    uint8_t bit = 1 << c;
    return updateBits(ADS1293_REG_DIS_EFILTER, bit, bypass ? bit : 0) && selectDataReadySource();
}

uint8_t Protocentral_ADS1293::channelR1(uint8_t ch) const {
    return (_regs[ADS1293_REG_R1_RATE] & (1 << ch)) ? 2 : 4;
}

uint8_t Protocentral_ADS1293::channelR2() const {
    switch (_regs[ADS1293_REG_R2_RATE]) {
        case 0x01: return 4;
        case 0x02: return 5;
        case 0x04: return 6;
        default:   return 8;  // 0x08, and the chip's fallback for invalid codes
    }
}

uint8_t Protocentral_ADS1293::channelR3(uint8_t ch) const {
    static const uint8_t values[8] = {4, 6, 8, 12, 16, 32, 64, 128};
    uint8_t code = _regs[ADS1293_REG_R3_RATE_CH1 + ch];
    for (uint8_t i = 0; i < 8; i++) {
        if (code == (1 << i)) return values[i];
    }
    return 128;  // chip fallback for invalid codes
}

float Protocentral_ADS1293::modulatorRate(uint8_t ch) const {
    return (_regs[ADS1293_REG_AFE_RES] & (0x08 << ch)) ? 204800.0f : 102400.0f;
}

float Protocentral_ADS1293::getSampleRate(ADS1293_Channel ch) const {
    if (!validChannel(ch)) return 0;
    uint8_t c = static_cast<uint8_t>(ch);
    return getPaceRate(ch) / channelR3(c);
}

float Protocentral_ADS1293::getPaceRate(ADS1293_Channel ch) const {
    if (!validChannel(ch)) return 0;
    uint8_t c = static_cast<uint8_t>(ch);
    return modulatorRate(c) / (channelR1(c) * channelR2());
}

// ============================================================================
// Reading data
// ============================================================================

void Protocentral_ADS1293::updateLayout() {
    uint8_t cfg = _regs[ADS1293_REG_CH_CNFG];
    uint8_t len = (cfg & ADS1293_CH_CNFG_STS_EN) ? 1 : 0;
    for (uint8_t ch = 0; ch < 3; ch++) {
        if (cfg & (0x02 << ch)) len += 2;
        if (cfg & (0x10 << ch)) len += 3;
    }
    _frameLen = len;

    // DATA_STATUS ready bit of the DRDYB source sits two bits above its DRDYB_SRC bit
    _readyMask = static_cast<uint8_t>(_regs[ADS1293_REG_DRDYB_SRC] << 2);
    if (_readyMask == 0) _readyMask = ADS1293_STATUS_ECG_MASK | ADS1293_STATUS_PACE_MASK;
}

bool Protocentral_ADS1293::selectDataReadySource() {
    if (!_autoDrdy) {
        updateLayout();
        return selectSyncSource();
    }

    // TI recommends the fastest read-back source, so no data is lost
    uint8_t cfg  = _regs[ADS1293_REG_CH_CNFG];
    uint8_t shdn = _regs[ADS1293_REG_AFE_SHDN_CN];
    uint8_t src  = 0;
    float   best = 0;
    for (uint8_t ch = 0; ch < 3; ch++) {
        if (shdn & (0x08 << ch)) continue;  // modulator off
        ADS1293_Channel c = static_cast<ADS1293_Channel>(ch);
        if ((cfg & (0x02 << ch)) && getPaceRate(c) > best) {
            best = getPaceRate(c);
            src  = 0x01 << ch;
        }
        if ((cfg & (0x10 << ch)) && !(_regs[ADS1293_REG_DIS_EFILTER] & (1 << ch)) && getSampleRate(c) > best) {
            best = getSampleRate(c);
            src  = 0x08 << ch;
        }
    }

    if (src != _regs[ADS1293_REG_DRDYB_SRC] && !updateRegister(ADS1293_REG_DRDYB_SRC, src)) return false;
    updateLayout();
    return selectSyncSource();
}

bool Protocentral_ADS1293::selectSyncSource() {
    if (!_autoSync) return true;

    uint8_t value = ADS1293_SYNCB_DIS_OUT;  // input: follow the SYNC line
    bool chipDrives = _syncMode == ADS1293_SyncMode::Master ||
                      (_syncMode == ADS1293_SyncMode::Auto && _syncPin < 0);
    if (chipDrives) {
        // Drive SYNCB from the slowest read-back source (datasheet 8.3.21). The chip then
        // ignores whatever is on the header, so an unwired SYNC pin cannot disturb it
        uint8_t cfg  = _regs[ADS1293_REG_CH_CNFG];
        uint8_t shdn = _regs[ADS1293_REG_AFE_SHDN_CN];
        uint8_t src  = 0;
        float   slowest = 0;
        for (uint8_t ch = 0; ch < 3; ch++) {
            if (shdn & (0x08 << ch)) continue;
            ADS1293_Channel c = static_cast<ADS1293_Channel>(ch);
            if ((cfg & (0x10 << ch)) && (slowest == 0 || getSampleRate(c) < slowest)) {
                slowest = getSampleRate(c);
                src     = 0x08 << ch;
            }
        }
        if (src == 0) {
            for (uint8_t ch = 0; ch < 3; ch++) {
                ADS1293_Channel c = static_cast<ADS1293_Channel>(ch);
                if (!(shdn & (0x08 << ch)) && (cfg & (0x02 << ch)) && (slowest == 0 || getPaceRate(c) < slowest)) {
                    slowest = getPaceRate(c);
                    src     = 0x01 << ch;
                }
            }
        }
        if (src) value = src;  // output driver enabled
    }

    if (value == _regs[ADS1293_REG_SYNCB_CN]) return true;
    return updateRegister(ADS1293_REG_SYNCB_CN, value);
}

bool Protocentral_ADS1293::readFrame(ADS1293_Sample& sample) {
    uint8_t buf[MAX_FRAME_LEN];
    uint8_t len = _frameLen;

    _spi->beginTransaction(_spiSettings);
    digitalWrite(_csPin, LOW);
    _spi->transfer(ADS1293_REG_DATA_LOOP | READ_BIT);
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = _spi->transfer(0x00);
    }
    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();

    memset(&sample, 0, sizeof(sample));
    uint8_t status = buf[0];
    if (status & 0x01) {
        // Bit 0 of DATA_STATUS always reads 0: MISO is stuck high or the frame is corrupt
        _lastError = ADS1293_Status::SpiError;
        return false;
    }

    uint8_t cfg = _regs[ADS1293_REG_CH_CNFG];
    uint8_t idx = 1;
    for (uint8_t ch = 0; ch < 3; ch++) {
        if (!(cfg & (0x02 << ch))) continue;
        int32_t raw  = (static_cast<int32_t>(buf[idx]) << 8) | buf[idx + 1];
        int32_t half = 0;
        switch (channelR2()) {
            case 4:  half = 0x8000 / 2; break;
            case 5:  half = 0xC350 / 2; break;
            case 6:  half = 0xF300 / 2; break;
            default: half = 0x8000 / 2; break;
        }
        int32_t v = raw - half;
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        sample.pace[ch] = static_cast<int16_t>(v);
        idx += 2;
    }
    for (uint8_t ch = 0; ch < 3; ch++) {
        if (!(cfg & (0x10 << ch))) continue;
        uint32_t raw = (static_cast<uint32_t>(buf[idx]) << 16) | (static_cast<uint32_t>(buf[idx + 1]) << 8) |
                       buf[idx + 2];
        // The output is offset binary: ADCMAX/2 is 0 V differential
        sample.ecg[ch] = static_cast<int32_t>(raw) -
                         static_cast<int32_t>(getAdcMax(static_cast<ADS1293_Channel>(ch)) / 2);
        idx += 3;
    }
    sample.ecgUpdated  = status >> 5;
    sample.paceUpdated = (status >> 2) & 0x07;

    if (status & ADS1293_STATUS_ALARMB) {
        // Reading ERROR_STATUS clears the latched flags and releases ALARMB
        sample.leadOff = rawReadRegister(ADS1293_REG_ERROR_LOD) & 0x3F;
        sample.errors  = rawReadRegister(ADS1293_REG_ERROR_STATUS);
    }
    return (status & _readyMask) != 0;
}

bool Protocentral_ADS1293::available() {
    if (_pending) return true;
    if (!_initialized || !isRunning()) return false;

    if (_irqSlot >= 0) {
        noInterrupts();
        bool flag = _drdyFlag;
        _drdyFlag = false;
        interrupts();
        if (!flag) return false;
        _pending = readFrame(_pendingSample);
        return _pending;
    }
    if (_drdyPin >= 0 && !_drdyFault) return readOnDataReady();
    return pollFrame();
}

bool Protocentral_ADS1293::readOnDataReady() {
    uint32_t now = micros();
    if (digitalRead(_drdyPin) == HIGH) {
        // DRDY falls once per period. If it stays high, look at the chip directly now and then
        if (now - _lastPollUs < 4 * _periodUs) return false;
        _lastPollUs = now;
        _pending    = readFrame(_pendingSample);
        if (_pending && ++_drdyStrikes >= 4) {
            _drdyFault = true;  // data kept waiting behind a high DRDY
            resetPolling();
        }
        return _pending;
    }

    _lastPollUs = now;
    _pending    = readFrame(_pendingSample);
    // The read releases DRDY at once. A pin that stays low, or is low with no new data,
    // is not following the chip (seen on boards with a faulty DRDY path)
    if (!_pending || digitalRead(_drdyPin) == LOW) {
        if (++_drdyStrikes >= 8) _drdyFault = true;
    } else {
        _drdyStrikes = 0;
    }
    if (_drdyFault) resetPolling();
    return _pending;
}

bool Protocentral_ADS1293::pollFrame() {
    uint32_t now = micros();
    if (static_cast<int32_t>(now - _nextPollUs) < 0) return false;

    // A conversion that completes while a frame is being read can be lost, so polling
    // back to back drops most samples. Instead, find when the data updates, then read
    // once per period, half a period later
    uint32_t step = _periodUs / 8;
    _pending = readFrame(_pendingSample);
    if (!_pending) {
        _pollLocked = false;
        _pollMissed = true;
        _lastPollUs = now;
        _nextPollUs = now + step;
        return false;
    }
    _anchorSamples++;

    if (_pollLocked) {
        // Slightly short of a period, so any drift between the MCU clock and the chip's
        // clock ends in an empty read (and a re-lock) rather than a skipped sample
        uint32_t cadence = _periodUs - _periodUs / 256 - 1;
        do {
            _nextPollUs += cadence;
        } while (static_cast<int32_t>(now - _nextPollUs) >= 0);
    } else if (_pollMissed && now - _lastPollUs <= 2 * step) {
        // The update happened between the last empty read and now
        uint32_t updateUs = _lastPollUs + (now - _lastPollUs) / 2;
        if (_anchorUs != 0 && _anchorSamples >= 16) {
            // Measure the chip's period against the MCU clock, within 3 % of nominal
            uint32_t measured = (updateUs - _anchorUs) / _anchorSamples;
            uint32_t nominal  = dataReadyPeriodUs();
            if (measured > nominal - nominal / 32 && measured < nominal + nominal / 32) _periodUs = measured;
        }
        _anchorUs      = updateUs;
        _anchorSamples = 0;
        _nextPollUs    = updateUs + _periodUs + _periodUs / 2;
        _pollLocked    = true;
    } else {
        _nextPollUs = now + step;  // phase not known yet: keep searching
    }
    _pollMissed = false;
    _lastPollUs = now;
    return true;
}

void Protocentral_ADS1293::resetPolling() {
    _periodUs      = dataReadyPeriodUs();
    _pollLocked    = false;
    _pollMissed    = false;
    _anchorUs      = 0;
    _anchorSamples = 0;
    _nextPollUs    = micros();
}

uint32_t Protocentral_ADS1293::dataReadyPeriodUs() const {
    uint8_t src  = _regs[ADS1293_REG_DRDYB_SRC];
    uint8_t cfg  = _regs[ADS1293_REG_CH_CNFG];
    float   rate = 0;
    for (uint8_t ch = 0; ch < 3; ch++) {
        ADS1293_Channel c = static_cast<ADS1293_Channel>(ch);
        // The DRDYB source sets the period; without one, the fastest read-back source
        bool pace = src ? (src & (0x01 << ch)) : (cfg & (0x02 << ch));
        bool ecg  = src ? (src & (0x08 << ch)) : (cfg & (0x10 << ch));
        if (pace && getPaceRate(c) > rate) rate = getPaceRate(c);
        if (ecg && getSampleRate(c) > rate) rate = getSampleRate(c);
    }
    return rate > 0 ? static_cast<uint32_t>(1000000.0f / rate) : 1000;
}

bool Protocentral_ADS1293::read(ADS1293_Sample& sample) {
    if (!_pending && !available()) return false;
    sample   = _pendingSample;
    _pending = false;
    return true;
}

void ADS1293_ISR_ATTR Protocentral_ADS1293::isr0() {
    if (_irqInstances[0]) _irqInstances[0]->handleDataReady();
}

void ADS1293_ISR_ATTR Protocentral_ADS1293::isr1() {
    if (_irqInstances[1]) _irqInstances[1]->handleDataReady();
}

void ADS1293_ISR_ATTR Protocentral_ADS1293::handleDataReady() {
    if (_drdyFlag) _missed = _missed + 1;
    _drdyFlag = true;
}

bool Protocentral_ADS1293::enableDataReadyInterrupt() {
    if (_irqSlot >= 0) return true;
    if (_drdyPin < 0) {
        _lastError = ADS1293_Status::NotInterruptPin;
        return false;
    }

    int irq = digitalPinToInterrupt(_drdyPin);
#ifdef NOT_AN_INTERRUPT
    if (irq == NOT_AN_INTERRUPT) {
        _lastError = ADS1293_Status::NotInterruptPin;
        return false;
    }
#endif
    if (irq < 0) {
        _lastError = ADS1293_Status::NotInterruptPin;
        return false;
    }

    int8_t slot = -1;
    for (uint8_t i = 0; i < ADS1293_MAX_IRQ_INSTANCES; i++) {
        if (_irqInstances[i] == nullptr) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }

    _irqInstances[slot] = this;
    _irqSlot  = slot;
    _drdyFlag = (digitalRead(_drdyPin) == LOW);
    _missed   = 0;
    attachInterrupt(irq, slot == 0 ? isr0 : isr1, FALLING);
    return true;
}

void Protocentral_ADS1293::disableDataReadyInterrupt() {
    if (_irqSlot < 0) return;
    detachInterrupt(digitalPinToInterrupt(_drdyPin));
    _irqInstances[_irqSlot] = nullptr;
    _irqSlot  = -1;
    _drdyFlag = false;
}

bool Protocentral_ADS1293::setEcgReadback(ADS1293_Channel ch, bool enable) {
    if (!validChannel(ch)) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    uint8_t bit = 0x10 << static_cast<uint8_t>(ch);
    return updateBits(ADS1293_REG_CH_CNFG, bit, enable ? bit : 0) && selectDataReadySource();
}

bool Protocentral_ADS1293::setPaceReadback(ADS1293_Channel ch, bool enable) {
    if (!validChannel(ch)) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    uint8_t bit = 0x02 << static_cast<uint8_t>(ch);
    return updateBits(ADS1293_REG_CH_CNFG, bit, enable ? bit : 0) && selectDataReadySource();
}

bool Protocentral_ADS1293::setDataReadySource(ADS1293_DataSource source) {
    uint8_t code = static_cast<uint8_t>(source);
    if (code == 0) {
        _autoDrdy = true;
        return selectDataReadySource();
    }
    if (!isSingleBit(code) || code > 0x20) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    _autoDrdy = false;
    if (!updateRegister(ADS1293_REG_DRDYB_SRC, code)) return false;
    updateLayout();
    return true;
}

bool Protocentral_ADS1293::setDataReadyMasking(bool maskAtStart, bool maskAfterSyncError) {
    uint8_t v = (maskAtStart ? 0 : 0x02) | (maskAfterSyncError ? 0 : 0x01);
    return updateRegister(ADS1293_REG_MASK_DRDYB, v);
}

// ============================================================================
// Channels and input routing
// ============================================================================

bool Protocentral_ADS1293::setChannelInputs(ADS1293_Channel ch, ADS1293_Input positive, ADS1293_Input negative) {
    uint8_t p = static_cast<uint8_t>(positive);
    uint8_t n = static_cast<uint8_t>(negative);
    if (!validChannel(ch) || p > 6 || n > 6) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    return updateBits(ADS1293_REG_FLEX_CH1_CN + static_cast<uint8_t>(ch), 0x3F, (p << 3) | n);
}

bool Protocentral_ADS1293::setChannelEnabled(ADS1293_Channel ch, bool enable) {
    if (!validChannel(ch)) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    uint8_t c    = static_cast<uint8_t>(ch);
    uint8_t shdn = 0x09 << c;  // INA and SDM shutdown bits
    uint8_t ecg  = 0x10 << c;
    if (enable) {
        return updateBits(ADS1293_REG_AFE_SHDN_CN, shdn, 0) && updateBits(ADS1293_REG_CH_CNFG, ecg, ecg) &&
               selectDataReadySource();
    }
    // Remove it from read-back and DRDY first, then power it down
    uint8_t pace = 0x02 << c;
    return updateBits(ADS1293_REG_CH_CNFG, ecg | pace, 0) && selectDataReadySource() &&
           updateBits(ADS1293_REG_AFE_SHDN_CN, shdn, shdn);
}

bool Protocentral_ADS1293::isChannelEnabled(ADS1293_Channel ch) const {
    if (!validChannel(ch)) return false;
    return !(_regs[ADS1293_REG_AFE_SHDN_CN] & (0x08 << static_cast<uint8_t>(ch)));
}

bool Protocentral_ADS1293::setTestSignal(ADS1293_Channel ch, ADS1293_TestSignal signal) {
    uint8_t s = static_cast<uint8_t>(signal);
    if (!validChannel(ch) || s > 3) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    return updateBits(ADS1293_REG_FLEX_CH1_CN + static_cast<uint8_t>(ch), 0xC0, s << 6);
}

bool Protocentral_ADS1293::setTestSignal(ADS1293_TestSignal signal) {
    for (uint8_t ch = 0; ch < 3; ch++) {
        ADS1293_Channel c = static_cast<ADS1293_Channel>(ch);
        if (isChannelEnabled(c) && !setTestSignal(c, signal)) return false;
    }
    return true;
}

bool Protocentral_ADS1293::setHighResolution(ADS1293_Channel ch, bool enable) {
    if (!validChannel(ch)) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    uint8_t bit = 1 << static_cast<uint8_t>(ch);
    return updateBits(ADS1293_REG_AFE_RES, bit, enable ? bit : 0);
}

bool Protocentral_ADS1293::setFaultDetection(ADS1293_Channel ch, bool enable) {
    if (!validChannel(ch)) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    uint8_t bit = 1 << static_cast<uint8_t>(ch);
    return updateBits(ADS1293_REG_AFE_FAULT_CN, bit, enable ? 0 : bit);
}

// ============================================================================
// Lead-off detection
// ============================================================================

uint8_t Protocentral_ADS1293::acDivider(uint16_t frequencyHz) {
    // Phi = 50 kHz / (4 x K x (ACDIV_LOD + 1)), K = 1 or 16
    if (frequencyHz == 0) frequencyHz = 1;
    uint32_t n = (12500UL + frequencyHz / 2) / frequencyHz;
    if (n >= 1 && n <= 128) return static_cast<uint8_t>(n - 1);
    n = (12500UL + 8UL * frequencyHz) / (16UL * frequencyHz);
    if (n < 1) n = 1;
    if (n > 128) n = 128;
    return static_cast<uint8_t>(0x80 | (n - 1));
}

bool Protocentral_ADS1293::setLeadOff(uint8_t lodCn, uint8_t lodEn, uint16_t currentNanoAmps, uint8_t acCn) {
    if (currentNanoAmps > 2040) currentNanoAmps = 2040;
    return updateRegister(ADS1293_REG_LOD_AC_CN, acCn) &&
           updateRegister(ADS1293_REG_LOD_CURRENT, static_cast<uint8_t>((currentNanoAmps + 4) / 8)) &&
           updateRegister(ADS1293_REG_LOD_EN, lodEn) && updateRegister(ADS1293_REG_LOD_CN, lodCn);
}

bool Protocentral_ADS1293::configureLeadOffDC(uint8_t inputs, uint16_t currentNanoAmps) {
    return setLeadOff(0x00, inputs & 0x3F, currentNanoAmps, 0x00);
}

bool Protocentral_ADS1293::configureLeadOffAnalogAC(uint8_t inputs, uint16_t currentNanoAmps, uint16_t frequencyHz,
                                                    ADS1293_ACLeadOffLevel level) {
    uint8_t lodCn = ADS1293_LOD_ACAD | ADS1293_LOD_SELAC | (static_cast<uint8_t>(level) & 0x03);
    return setLeadOff(lodCn, inputs & 0x3F, currentNanoAmps, acDivider(frequencyHz));
}

bool Protocentral_ADS1293::configureLeadOffDigitalAC(uint8_t channels, uint16_t currentNanoAmps, uint16_t frequencyHz,
                                                     uint8_t antiPhase) {
    // In digital AC mode LOD_EN[2:0] selects channels and LOD_EN[5:3] their current phase
    uint8_t lodEn = (channels & 0x07) | ((antiPhase & 0x07) << 3);
    return setLeadOff(ADS1293_LOD_SELAC, lodEn, currentNanoAmps, acDivider(frequencyHz));
}

bool Protocentral_ADS1293::disableLeadOff() {
    return setLeadOff(ADS1293_LOD_SHDN, 0x00, 0, 0x00);
}

uint8_t Protocentral_ADS1293::readLeadOffStatus() {
    return rawReadRegister(ADS1293_REG_ERROR_LOD) & 0x3F;
}

// ============================================================================
// Common mode, right-leg drive, reference terminals
// ============================================================================

bool Protocentral_ADS1293::configureCommonModeDetect(uint8_t inputs, bool highBandwidth, ADS1293_CapDrive capDrive) {
    uint8_t cn = (highBandwidth ? 0x04 : 0x00) | (static_cast<uint8_t>(capDrive) & 0x03);
    return updateRegister(ADS1293_REG_CMDET_EN, inputs & 0x3F) && updateRegister(ADS1293_REG_CMDET_CN, cn);
}

bool Protocentral_ADS1293::configureRLD(bool enable, ADS1293_Input output, bool highBandwidth,
                                        ADS1293_CapDrive capDrive) {
    uint8_t out = static_cast<uint8_t>(output);
    if (out > 6) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    uint8_t v = (highBandwidth ? ADS1293_RLD_BW_HIGH : 0) | ((static_cast<uint8_t>(capDrive) & 0x03) << 4);
    v |= enable ? out : ADS1293_RLD_SHDN;
    return updateRegister(ADS1293_REG_RLD_CN, v);
}

bool Protocentral_ADS1293::configureWilson(ADS1293_Input a, ADS1293_Input b, ADS1293_Input c, bool connectToIN6) {
    uint8_t ia = static_cast<uint8_t>(a), ib = static_cast<uint8_t>(b), ic = static_cast<uint8_t>(c);
    if (ia > 6 || ib > 6 || ic > 6) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    bool any = ia || ib || ic;
    return updateRegister(ADS1293_REG_WILSON_CN, 0x00) &&  // never GOLDINT and WILSONINT together
           updateRegister(ADS1293_REG_WILSON_EN1, ia) && updateRegister(ADS1293_REG_WILSON_EN2, ib) &&
           updateRegister(ADS1293_REG_WILSON_EN3, ic) &&
           updateRegister(ADS1293_REG_WILSON_CN, (any && connectToIN6) ? ADS1293_WILSON_WILSONINT : 0x00);
}

bool Protocentral_ADS1293::configureGoldberger(ADS1293_Input a, ADS1293_Input b, ADS1293_Input c) {
    uint8_t ia = static_cast<uint8_t>(a), ib = static_cast<uint8_t>(b), ic = static_cast<uint8_t>(c);
    if (ia == 0 || ib == 0 || ic == 0 || ia > 6 || ib > 6 || ic > 6) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    return updateRegister(ADS1293_REG_WILSON_CN, 0x00) && updateRegister(ADS1293_REG_WILSON_EN1, ia) &&
           updateRegister(ADS1293_REG_WILSON_EN2, ib) && updateRegister(ADS1293_REG_WILSON_EN3, ic) &&
           updateRegister(ADS1293_REG_WILSON_CN, ADS1293_WILSON_GOLDINT);
}

bool Protocentral_ADS1293::setReference(bool internalReference, bool commonModeReference) {
    uint8_t v = (internalReference ? 0 : ADS1293_REF_SHDN_REF) | (commonModeReference ? 0 : ADS1293_REF_SHDN_CMREF);
    return updateRegister(ADS1293_REG_REF_CN, v);
}

// ============================================================================
// Pace
// ============================================================================

bool Protocentral_ADS1293::configureAnalogPace(bool enable, ADS1293_Input positive, ADS1293_Input negative,
                                               ADS1293_PaceOutput output) {
    uint8_t p = static_cast<uint8_t>(positive);
    uint8_t n = static_cast<uint8_t>(negative);
    if (p > 6 || n > 6) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    uint8_t cn = ADS1293_PACE_SHDN;
    if (enable) {
        cn = 0;
        if (output == ADS1293_PaceOutput::Wct) cn = ADS1293_PACE_TO_WCT;
        if (output == ADS1293_PaceOutput::RldIn) cn = ADS1293_PACE_TO_RLDIN;
    }
    return updateRegister(ADS1293_REG_FLEX_PACE_CN, enable ? ((p << 3) | n) : 0x00) &&
           updateRegister(ADS1293_REG_AFE_PACE_CN, cn);
}

// ============================================================================
// Clock and synchronization
// ============================================================================

bool Protocentral_ADS1293::setClockOutput(bool enable) {
    if (_regs[ADS1293_REG_OSC_CN] & ADS1293_OSC_SHDN_OSC) {
        _lastError = ADS1293_Status::InvalidArgument;  // only the crystal oscillator can drive CLK
        return false;
    }
    return updateBits(ADS1293_REG_OSC_CN, ADS1293_OSC_EN_CLKOUT, enable ? ADS1293_OSC_EN_CLKOUT : 0);
}

bool Protocentral_ADS1293::configureSync(bool output, ADS1293_DataSource source) {
    uint8_t src = static_cast<uint8_t>(source);
    if (src != 0 && (!isSingleBit(src) || src > 0x20)) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    _autoSync = false;
    return updateRegister(ADS1293_REG_SYNCB_CN, (output ? 0 : ADS1293_SYNCB_DIS_OUT) | src);
}

bool Protocentral_ADS1293::setSyncMode(ADS1293_SyncMode mode) {
    if (static_cast<uint8_t>(mode) > static_cast<uint8_t>(ADS1293_SyncMode::Slave)) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    _syncMode    = mode;
    _config.sync = mode;
    _autoSync    = true;
    // Release the line before the other side starts driving it
    if (mode == ADS1293_SyncMode::Auto && _syncPin >= 0) {
        bool ok = selectSyncSource();  // chip to input first
        driveControlPins();            // then the MCU drives it HIGH
        return ok;
    }
    driveControlPins();                // MCU to input first
    return selectSyncSource();         // then the chip may drive it
}

bool Protocentral_ADS1293::setDriveStrength(ADS1293_DriveStrength strength) {
    return updateRegister(ADS1293_REG_DIGO_STRENGTH, static_cast<uint8_t>(strength) & 0x03);
}

// ============================================================================
// Errors and alarms
// ============================================================================

uint8_t Protocentral_ADS1293::readErrorStatus() {
    return rawReadRegister(ADS1293_REG_ERROR_STATUS);
}

uint8_t Protocentral_ADS1293::readRangeErrors(ADS1293_Channel ch) {
    if (!validChannel(ch)) return 0;
    return rawReadRegister(ADS1293_REG_ERROR_RANGE1 + static_cast<uint8_t>(ch)) & 0x7F;
}

uint8_t Protocentral_ADS1293::readSyncErrors() {
    return rawReadRegister(ADS1293_REG_ERROR_SYNC) & 0x0F;
}

uint8_t Protocentral_ADS1293::readMiscErrors() {
    return rawReadRegister(ADS1293_REG_ERROR_MISC) & 0x07;
}

bool Protocentral_ADS1293::setAlarmMask(uint8_t mask) {
    return updateRegister(ADS1293_REG_MASK_ERR, mask);
}

bool Protocentral_ADS1293::setAlarmFilter(uint8_t leadOffCounts, uint8_t otherCounts) {
    if (leadOffCounts < 1 || leadOffCounts > 16 || otherCounts < 1 || otherCounts > 16) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    return updateRegister(ADS1293_REG_ALARM_FILTER, ((otherCounts - 1) << 4) | (leadOffCounts - 1));
}

// ============================================================================
// Units and measurements
// ============================================================================

uint32_t Protocentral_ADS1293::getAdcMax(ADS1293_Channel ch) const {
    if (!validChannel(ch)) return 0x800000UL;
    uint8_t r3  = channelR3(static_cast<uint8_t>(ch));
    bool    odd = (r3 == 6 || r3 == 12);  // datasheet Tables 8-11: ADCMAX depends only on R2 and R3
    switch (channelR2()) {
        case 4:  return odd ? 0xF30000UL : 0x800000UL;
        case 5:  return odd ? 0xB964F0UL : 0xC35000UL;
        case 6:  return odd ? 0xE6A900UL : 0xF30000UL;
        default: return odd ? 0xF30000UL : 0x800000UL;
    }
}

float Protocentral_ADS1293::countsToMicrovolts(int32_t counts, ADS1293_Channel ch) const {
    // V_in = (ADCOUT / ADCMAX - 1/2) x 2 VREF / 3.5, and counts = ADCOUT - ADCMAX / 2
    return static_cast<float>(counts) * (2.0f * VREF_VOLTS * 1e6f / INA_GAIN) / static_cast<float>(getAdcMax(ch));
}

float Protocentral_ADS1293::paceCountsToMicrovolts(int16_t counts) const {
    float paceMax;
    switch (channelR2()) {
        case 5:  paceMax = 50000.0f; break;  // 0xC350
        case 6:  paceMax = 62208.0f; break;  // 0xF300
        default: paceMax = 32768.0f; break;  // 0x8000
    }
    return static_cast<float>(counts) * (2.0f * VREF_VOLTS * 1e6f / INA_GAIN) / paceMax;
}

float Protocentral_ADS1293::readSupplyVoltage(ADS1293_Channel ch) {
    if (!_initialized || !validChannel(ch)) {
        _lastError = _initialized ? ADS1293_Status::InvalidArgument : ADS1293_Status::NotInitialized;
        return NAN;
    }
    uint8_t c          = static_cast<uint8_t>(ch);
    uint8_t savedVbat  = _regs[ADS1293_REG_FLEX_VBAT_CN];
    uint8_t savedShdn  = _regs[ADS1293_REG_AFE_SHDN_CN];
    uint8_t savedCnfg  = _regs[ADS1293_REG_CH_CNFG];
    bool    wasRunning = isRunning();

    // Battery monitor: the INA must be off, the modulator on
    bool ok = updateRegister(ADS1293_REG_FLEX_VBAT_CN, savedVbat | (1 << c)) &&
              updateRegister(ADS1293_REG_AFE_SHDN_CN, (savedShdn | (0x01 << c)) & ~(0x08 << c)) &&
              updateRegister(ADS1293_REG_CH_CNFG, savedCnfg | (0x10 << c)) && selectDataReadySource() &&
              (wasRunning || start());

    float   result = NAN;
    int32_t sum    = 0;
    uint8_t seen = 0, used = 0;
    if (ok) {
        uint32_t timeoutMs = static_cast<uint32_t>(1000.0f * (SETTLE_SAMPLES + AVERAGE_SAMPLES + 4) / getSampleRate(ch)) + 50;
        uint32_t t0        = millis();
        while (used < AVERAGE_SAMPLES && millis() - t0 < timeoutMs) {
            ADS1293_Sample s;
            readFrame(s);
            if (!(s.ecgUpdated & (1 << c))) continue;
            if (seen++ < SETTLE_SAMPLES) continue;
            sum += s.ecg[c];
            used++;
        }
        if (used == AVERAGE_SAMPLES) {
            float ratio = static_cast<float>(sum / AVERAGE_SAMPLES) / static_cast<float>(getAdcMax(ch));
            result      = VREF_VOLTS * (1.0f + VBAT_GAIN * ratio);
        } else {
            _lastError = ADS1293_Status::Timeout;
        }
    }

    // Restore the channel
    _pending = false;
    bool restored = updateRegister(ADS1293_REG_CH_CNFG, savedCnfg) && selectDataReadySource() &&
                    updateRegister(ADS1293_REG_AFE_SHDN_CN, savedShdn) &&
                    updateRegister(ADS1293_REG_FLEX_VBAT_CN, savedVbat);
    if (!wasRunning) restored = stop() && restored;
    return restored ? result : NAN;
}

// ============================================================================
// Raw register access
// ============================================================================

uint8_t Protocentral_ADS1293::readRegister(uint8_t reg) {
    return rawReadRegister(reg);
}

bool Protocentral_ADS1293::writeRegister(uint8_t reg, uint8_t value) {
    if (!updateRegister(reg, value)) return false;
    if (reg == ADS1293_REG_DRDYB_SRC) _autoDrdy = false;
    return true;
}

static const __FlashStringHelper* registerName(uint8_t reg) {
    switch (reg) {
        case ADS1293_REG_CONFIG:        return F("CONFIG");
        case ADS1293_REG_FLEX_CH1_CN:   return F("FLEX_CH1_CN");
        case ADS1293_REG_FLEX_CH2_CN:   return F("FLEX_CH2_CN");
        case ADS1293_REG_FLEX_CH3_CN:   return F("FLEX_CH3_CN");
        case ADS1293_REG_FLEX_PACE_CN:  return F("FLEX_PACE_CN");
        case ADS1293_REG_FLEX_VBAT_CN:  return F("FLEX_VBAT_CN");
        case ADS1293_REG_LOD_CN:        return F("LOD_CN");
        case ADS1293_REG_LOD_EN:        return F("LOD_EN");
        case ADS1293_REG_LOD_CURRENT:   return F("LOD_CURRENT");
        case ADS1293_REG_LOD_AC_CN:     return F("LOD_AC_CN");
        case ADS1293_REG_CMDET_EN:      return F("CMDET_EN");
        case ADS1293_REG_CMDET_CN:      return F("CMDET_CN");
        case ADS1293_REG_RLD_CN:        return F("RLD_CN");
        case ADS1293_REG_WILSON_EN1:    return F("WILSON_EN1");
        case ADS1293_REG_WILSON_EN2:    return F("WILSON_EN2");
        case ADS1293_REG_WILSON_EN3:    return F("WILSON_EN3");
        case ADS1293_REG_WILSON_CN:     return F("WILSON_CN");
        case ADS1293_REG_REF_CN:        return F("REF_CN");
        case ADS1293_REG_OSC_CN:        return F("OSC_CN");
        case ADS1293_REG_AFE_RES:       return F("AFE_RES");
        case ADS1293_REG_AFE_SHDN_CN:   return F("AFE_SHDN_CN");
        case ADS1293_REG_AFE_FAULT_CN:  return F("AFE_FAULT_CN");
        case ADS1293_REG_AFE_PACE_CN:   return F("AFE_PACE_CN");
        case ADS1293_REG_ERROR_LOD:     return F("ERROR_LOD");
        case ADS1293_REG_ERROR_STATUS:  return F("ERROR_STATUS");
        case ADS1293_REG_ERROR_RANGE1:  return F("ERROR_RANGE1");
        case ADS1293_REG_ERROR_RANGE2:  return F("ERROR_RANGE2");
        case ADS1293_REG_ERROR_RANGE3:  return F("ERROR_RANGE3");
        case ADS1293_REG_ERROR_SYNC:    return F("ERROR_SYNC");
        case ADS1293_REG_ERROR_MISC:    return F("ERROR_MISC");
        case ADS1293_REG_DIGO_STRENGTH: return F("DIGO_STRENGTH");
        case ADS1293_REG_R2_RATE:       return F("R2_RATE");
        case ADS1293_REG_R3_RATE_CH1:   return F("R3_RATE_CH1");
        case ADS1293_REG_R3_RATE_CH2:   return F("R3_RATE_CH2");
        case ADS1293_REG_R3_RATE_CH3:   return F("R3_RATE_CH3");
        case ADS1293_REG_R1_RATE:       return F("R1_RATE");
        case ADS1293_REG_DIS_EFILTER:   return F("DIS_EFILTER");
        case ADS1293_REG_DRDYB_SRC:     return F("DRDYB_SRC");
        case ADS1293_REG_SYNCB_CN:      return F("SYNCB_CN");
        case ADS1293_REG_MASK_DRDYB:    return F("MASK_DRDYB");
        case ADS1293_REG_MASK_ERR:      return F("MASK_ERR");
        case ADS1293_REG_ALARM_FILTER:  return F("ALARM_FILTER");
        case ADS1293_REG_CH_CNFG:       return F("CH_CNFG");
        case ADS1293_REG_DATA_STATUS:   return F("DATA_STATUS");
        case ADS1293_REG_REVID:         return F("REVID");
        default:                        return nullptr;
    }
}

void Protocentral_ADS1293::printRegisters(Stream& out) {
    for (uint8_t reg = 0; reg <= ADS1293_REG_REVID; reg++) {
        const __FlashStringHelper* name = registerName(reg);
        if (!name) continue;
        uint8_t v = rawReadRegister(reg);
        out.print(F("0x"));
        if (reg < 0x10) out.print('0');
        out.print(reg, HEX);
        out.print(F("  "));
        out.print(name);
        for (uint8_t pad = strlen_P(reinterpret_cast<const char*>(name)); pad < 15; pad++) out.print(' ');
        out.print(F("0x"));
        if (v < 0x10) out.print('0');
        out.println(v, HEX);
    }
}

// ============================================================================
// Register write path
// ============================================================================

bool Protocentral_ADS1293::isLocked(uint8_t reg) {
    // START_CON = 1 locks write access to these registers (datasheet CONFIG note)
    return (reg >= ADS1293_REG_REF_CN && reg <= ADS1293_REG_AFE_RES) ||
           (reg >= ADS1293_REG_R2_RATE && reg <= ADS1293_REG_MASK_DRDYB);
}

uint8_t Protocentral_ADS1293::writableMask(uint8_t reg) {
    switch (reg) {
        case ADS1293_REG_CONFIG:        return 0x07;
        case ADS1293_REG_FLEX_CH1_CN:
        case ADS1293_REG_FLEX_CH2_CN:
        case ADS1293_REG_FLEX_CH3_CN:
        case ADS1293_REG_FLEX_PACE_CN:  return 0xFF;
        case ADS1293_REG_FLEX_VBAT_CN:  return 0x07;
        case ADS1293_REG_LOD_CN:        return 0x1F;
        case ADS1293_REG_LOD_EN:        return 0x3F;
        case ADS1293_REG_LOD_CURRENT:   return 0xFF;
        case ADS1293_REG_LOD_AC_CN:     return 0xFF;
        case ADS1293_REG_CMDET_EN:      return 0x3F;
        case ADS1293_REG_CMDET_CN:      return 0x07;
        case ADS1293_REG_RLD_CN:        return 0x7F;
        case ADS1293_REG_WILSON_EN1:
        case ADS1293_REG_WILSON_EN2:
        case ADS1293_REG_WILSON_EN3:    return 0x07;
        case ADS1293_REG_WILSON_CN:     return 0x03;
        case ADS1293_REG_REF_CN:        return 0x03;
        case ADS1293_REG_OSC_CN:        return 0x07;
        case ADS1293_REG_AFE_RES:       return 0x3F;
        case ADS1293_REG_AFE_SHDN_CN:   return 0x3F;
        case ADS1293_REG_AFE_FAULT_CN:  return 0x07;
        case ADS1293_REG_AFE_PACE_CN:   return 0x07;
        case ADS1293_REG_DIGO_STRENGTH: return 0x03;
        case ADS1293_REG_R2_RATE:       return 0x0F;
        case ADS1293_REG_R3_RATE_CH1:
        case ADS1293_REG_R3_RATE_CH2:
        case ADS1293_REG_R3_RATE_CH3:   return 0xFF;
        case ADS1293_REG_R1_RATE:       return 0x07;
        case ADS1293_REG_DIS_EFILTER:   return 0x07;
        case ADS1293_REG_DRDYB_SRC:     return 0x3F;
        case ADS1293_REG_SYNCB_CN:      return 0x7F;
        case ADS1293_REG_MASK_DRDYB:    return 0x03;
        case ADS1293_REG_MASK_ERR:      return 0xFF;
        case ADS1293_REG_ALARM_FILTER:  return 0xFF;
        case ADS1293_REG_CH_CNFG:       return 0x7F;
        default:                        return 0x00;  // read-only or reserved
    }
}

bool Protocentral_ADS1293::updateRegister(uint8_t reg, uint8_t value) {
    uint8_t mask = writableMask(reg);
    if (mask == 0) {
        _lastError = ADS1293_Status::InvalidArgument;
        return false;
    }
    value &= mask;
    if (reg == ADS1293_REG_CH_CNFG) value |= ADS1293_CH_CNFG_STS_EN;  // the driver needs DATA_STATUS in every frame

    // Locked registers are written with conversions briefly stopped
    bool pause = isRunning() && isLocked(reg);
    if (pause) {
        rawWriteRegister(ADS1293_REG_CONFIG, _regs[ADS1293_REG_CONFIG] & ~ADS1293_CONFIG_START_CON);
        _pending = false;
    }

    rawWriteRegister(reg, value);
    uint8_t readBack = rawReadRegister(reg) & mask;

    if (pause) {
        rawWriteRegister(ADS1293_REG_CONFIG, _regs[ADS1293_REG_CONFIG]);
    }
    if (readBack != value) {
        _lastError = ADS1293_Status::VerifyFailed;
        return false;
    }
    _regs[reg] = value;
    if (reg == ADS1293_REG_CH_CNFG || reg == ADS1293_REG_DRDYB_SRC) updateLayout();
    resetPolling();  // the output period or phase may have changed
    return true;
}

bool Protocentral_ADS1293::updateBits(uint8_t reg, uint8_t mask, uint8_t bits) {
    return updateRegister(reg, (_regs[reg] & ~mask) | (bits & mask));
}

// ============================================================================
// Low-level SPI
// ============================================================================

uint8_t Protocentral_ADS1293::rawReadRegister(uint8_t reg) {
    _spi->beginTransaction(_spiSettings);
    digitalWrite(_csPin, LOW);
    _spi->transfer(reg | READ_BIT);
    uint8_t v = _spi->transfer(0x00);
    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();
    return v;
}

void Protocentral_ADS1293::rawWriteRegister(uint8_t reg, uint8_t value) {
    _spi->beginTransaction(_spiSettings);
    digitalWrite(_csPin, LOW);
    _spi->transfer(reg & ~READ_BIT);
    _spi->transfer(value);
    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();
}
