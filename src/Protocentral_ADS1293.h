// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2020-2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// Arduino driver for the TI ADS1293 three-channel 24-bit ECG analog front end,
// as used on the ProtoCentral ADS1293 Breakout.
//
// For research and education only - not a medical device.
//
// https://github.com/Protocentral/protocentral-ads1293-arduino

#ifndef PROTOCENTRAL_ADS1293_H
#define PROTOCENTRAL_ADS1293_H

#include <Arduino.h>
#include <SPI.h>

#define ADS1293_LIBRARY_VERSION "2.0.0"

// ============================================================================
// Compile-time configuration (override with -D or #define before #include)
// ============================================================================

#ifndef ADS1293_SPI_CLOCK
#define ADS1293_SPI_CLOCK 1000000UL  ///< SPI clock in Hz. 1 MHz is safe through the breakout's TXB0108 level shifter
#endif

#ifndef ADS1293_POWERUP_MS
#define ADS1293_POWERUP_MS 25        ///< Minimum ms after MCU boot before the first access (crystal start-up 15 ms + reference 5 ms)
#endif

#ifndef ADS1293_CLOCK_START_MS
#define ADS1293_CLOCK_START_MS 20    ///< Wait between selecting a clock source and setting STRTCLK (tSTART = 15 ms)
#endif

#define ADS1293_MAX_IRQ_INSTANCES 2  ///< Driver instances that may use DRDY interrupts at the same time (one ISR trampoline each)

// ============================================================================
// Register map (datasheet SNAS602C, section 8.6)
// ============================================================================

#define ADS1293_REG_CONFIG        0x00
#define ADS1293_REG_FLEX_CH1_CN   0x01
#define ADS1293_REG_FLEX_CH2_CN   0x02
#define ADS1293_REG_FLEX_CH3_CN   0x03
#define ADS1293_REG_FLEX_PACE_CN  0x04
#define ADS1293_REG_FLEX_VBAT_CN  0x05
#define ADS1293_REG_LOD_CN        0x06
#define ADS1293_REG_LOD_EN        0x07
#define ADS1293_REG_LOD_CURRENT   0x08
#define ADS1293_REG_LOD_AC_CN     0x09
#define ADS1293_REG_CMDET_EN      0x0A
#define ADS1293_REG_CMDET_CN      0x0B
#define ADS1293_REG_RLD_CN        0x0C
#define ADS1293_REG_WILSON_EN1    0x0D
#define ADS1293_REG_WILSON_EN2    0x0E
#define ADS1293_REG_WILSON_EN3    0x0F
#define ADS1293_REG_WILSON_CN     0x10
#define ADS1293_REG_REF_CN        0x11
#define ADS1293_REG_OSC_CN        0x12
#define ADS1293_REG_AFE_RES       0x13
#define ADS1293_REG_AFE_SHDN_CN   0x14
#define ADS1293_REG_AFE_FAULT_CN  0x15
#define ADS1293_REG_AFE_PACE_CN   0x17
#define ADS1293_REG_ERROR_LOD     0x18
#define ADS1293_REG_ERROR_STATUS  0x19
#define ADS1293_REG_ERROR_RANGE1  0x1A
#define ADS1293_REG_ERROR_RANGE2  0x1B
#define ADS1293_REG_ERROR_RANGE3  0x1C
#define ADS1293_REG_ERROR_SYNC    0x1D
#define ADS1293_REG_ERROR_MISC    0x1E
#define ADS1293_REG_DIGO_STRENGTH 0x1F
#define ADS1293_REG_R2_RATE       0x21
#define ADS1293_REG_R3_RATE_CH1   0x22
#define ADS1293_REG_R3_RATE_CH2   0x23
#define ADS1293_REG_R3_RATE_CH3   0x24
#define ADS1293_REG_R1_RATE       0x25
#define ADS1293_REG_DIS_EFILTER   0x26
#define ADS1293_REG_DRDYB_SRC     0x27
#define ADS1293_REG_SYNCB_CN      0x28
#define ADS1293_REG_MASK_DRDYB    0x29
#define ADS1293_REG_MASK_ERR      0x2A
#define ADS1293_REG_ALARM_FILTER  0x2E
#define ADS1293_REG_CH_CNFG       0x2F
#define ADS1293_REG_DATA_STATUS   0x30
#define ADS1293_REG_DATA_CH1_PACE 0x31  ///< 2 bytes, MSB first
#define ADS1293_REG_DATA_CH2_PACE 0x33
#define ADS1293_REG_DATA_CH3_PACE 0x35
#define ADS1293_REG_DATA_CH1_ECG  0x37  ///< 3 bytes, MSB first
#define ADS1293_REG_DATA_CH2_ECG  0x3A
#define ADS1293_REG_DATA_CH3_ECG  0x3D
#define ADS1293_REG_REVID         0x40
#define ADS1293_REG_DATA_LOOP     0x50  ///< Loop read-back of the registers enabled in CH_CNFG

#define ADS1293_NUM_CONFIG_REGS   0x30  ///< Configuration registers 0x00-0x2F are shadowed by the driver
#define ADS1293_REVID_VALUE       0x01  ///< REVID register value

// CONFIG
#define ADS1293_CONFIG_START_CON  0x01
#define ADS1293_CONFIG_STANDBY    0x02
#define ADS1293_CONFIG_PWR_DOWN   0x04

// OSC_CN
#define ADS1293_OSC_EN_CLKOUT     0x01
#define ADS1293_OSC_SHDN_OSC      0x02
#define ADS1293_OSC_STRTCLK       0x04

// LOD_CN
#define ADS1293_LOD_ACAD          0x10
#define ADS1293_LOD_SHDN          0x08
#define ADS1293_LOD_SELAC         0x04

// RLD_CN
#define ADS1293_RLD_BW_HIGH       0x40
#define ADS1293_RLD_SHDN          0x08

// WILSON_CN
#define ADS1293_WILSON_GOLDINT    0x02
#define ADS1293_WILSON_WILSONINT  0x01

// REF_CN
#define ADS1293_REF_SHDN_CMREF    0x02
#define ADS1293_REF_SHDN_REF      0x01

// AFE_PACE_CN
#define ADS1293_PACE_TO_RLDIN     0x04
#define ADS1293_PACE_TO_WCT       0x02
#define ADS1293_PACE_SHDN         0x01

// SYNCB_CN
#define ADS1293_SYNCB_DIS_OUT     0x40

// CH_CNFG
#define ADS1293_CH_CNFG_STS_EN    0x01

// DATA_STATUS
#define ADS1293_STATUS_ALARMB     0x02
#define ADS1293_STATUS_PACE_MASK  0x1C  ///< P3..P1_DRDY
#define ADS1293_STATUS_ECG_MASK   0xE0  ///< E3..E1_DRDY

// Input-pin bit masks, shared by ERROR_LOD, LOD_EN (DC / analog AC) and CMDET_EN
#define ADS1293_IN1 0x01
#define ADS1293_IN2 0x02
#define ADS1293_IN3 0x04
#define ADS1293_IN4 0x08
#define ADS1293_IN5 0x10
#define ADS1293_IN6 0x20

// Electrode names for the ProtoCentral ADS1293 Breakout (from its netlist):
// 3.5 mm jack 1 carries RA/LA/RL, jack 2 carries LL/V1; IN6 goes to jumper JMP1.
#define ADS1293_ELECTRODE_RA ADS1293_IN1  ///< Right arm
#define ADS1293_ELECTRODE_LA ADS1293_IN2  ///< Left arm
#define ADS1293_ELECTRODE_LL ADS1293_IN3  ///< Left leg
#define ADS1293_ELECTRODE_RL ADS1293_IN4  ///< Right leg (driven by RLD)
#define ADS1293_ELECTRODE_V1 ADS1293_IN5  ///< Precordial V1 (5-lead)

// ERROR_STATUS
#define ADS1293_ERR_CMOR          0x01  ///< Common-mode level out of range
#define ADS1293_ERR_RLDRAIL       0x02  ///< RLD amplifier near a supply rail
#define ADS1293_ERR_BATLOW        0x04  ///< Supply below 2.7 V
#define ADS1293_ERR_LEADOFF       0x08  ///< At least one ERROR_LOD bit set
#define ADS1293_ERR_CH1           0x10  ///< Channel 1 out of range (see readRangeErrors())
#define ADS1293_ERR_CH2           0x20
#define ADS1293_ERR_CH3           0x40
#define ADS1293_ERR_SYNCEDGE      0x80  ///< Digital synchronization error

// ERROR_RANGEx
#define ADS1293_RANGE_DIF_HIGH    0x01  ///< INA differential output beyond +/-1.4 V - SDM samples 0 V
#define ADS1293_RANGE_OUTP_HIGH   0x02
#define ADS1293_RANGE_OUTP_LOW    0x04
#define ADS1293_RANGE_OUTN_HIGH   0x08
#define ADS1293_RANGE_OUTN_LOW    0x10
#define ADS1293_RANGE_SIGN        0x20  ///< Sign of the INA output (1 = negative)
#define ADS1293_RANGE_SDM_OR      0x40  ///< Sigma-delta modulator over range

// ERROR_SYNC
#define ADS1293_SYNC_CH1          0x01
#define ADS1293_SYNC_CH2          0x02
#define ADS1293_SYNC_CH3          0x04
#define ADS1293_SYNC_PHASE        0x08

// ERROR_MISC (live status, not latched)
#define ADS1293_MISC_CMOR         0x01
#define ADS1293_MISC_RLDRAIL      0x02
#define ADS1293_MISC_BATLOW       0x04

// ============================================================================
// Enumerations (values are register bit patterns where that is meaningful)
// ============================================================================

/// Error codes reported by lastError().
enum class ADS1293_Status : uint8_t {
    Ok = 0,           ///< No error
    NotInitialized,   ///< begin() has not succeeded yet
    SpiError,         ///< No response from the chip (REVID read 0x00/0xFF, or a corrupt status byte)
    WrongChipId,      ///< A device answered, but it is not an ADS1293
    VerifyFailed,     ///< Register read-back did not match what was written
    NoClock,          ///< The chip answers, but produces no conversions: its clock is not running
    InvalidArgument,  ///< Out-of-range parameter, or a request the current configuration cannot satisfy
    NotInterruptPin,  ///< DRDY pin cannot be used with attachInterrupt() on this board
    Timeout           ///< No data arrived in time
};

/// Channel selector.
enum class ADS1293_Channel : uint8_t {
    CH1 = 0,
    CH2 = 1,
    CH3 = 2
};

/// Input pin for flex routing, RLD, Wilson and pace selections (register field value).
enum class ADS1293_Input : uint8_t {
    None = 0,
    IN1  = 1,
    IN2  = 2,
    IN3  = 3,
    IN4  = 4,
    IN5  = 5,
    IN6  = 6
};

/// Electrode set applied by begin().
enum class ADS1293_LeadSet : uint8_t {
    ThreeLead,  ///< CH1 = Lead I (LA-RA), CH2 = Lead II (LL-RA), CH3 off. RLD on RL
    FiveLead,   ///< ThreeLead plus CH3 = V1 - WCT, with the Wilson terminal routed to IN6
    Custom      ///< Nothing routed: set inputs with setChannelInputs() and enable channels yourself
};

/// ECG output data rate presets (all channels, SDM at 102.4 kHz, standard pace rate).
/// ODR = 102400 / (4 x R2 x R3). Names are the rate in samples per second.
enum class ADS1293_DataRate : uint8_t {
    SPS_25,    ///< R2 = 8, R3 = 128
    SPS_40,    ///< R2 = 5, R3 = 128
    SPS_50,    ///< R2 = 4, R3 = 128
    SPS_80,    ///< R2 = 5, R3 = 64
    SPS_100,   ///< R2 = 4, R3 = 64
    SPS_160,   ///< R2 = 5, R3 = 32
    SPS_200,   ///< R2 = 4, R3 = 32 (default)
    SPS_320,   ///< R2 = 5, R3 = 16
    SPS_400,   ///< R2 = 4, R3 = 16
    SPS_640,   ///< R2 = 5, R3 = 8
    SPS_800,   ///< R2 = 4, R3 = 8
    SPS_853,   ///< R2 = 5, R3 = 6 (TI reference configuration)
    SPS_1067,  ///< R2 = 4, R3 = 6
    SPS_1280,  ///< R2 = 5, R3 = 4
    SPS_1600   ///< R2 = 4, R3 = 4
};

/// Second-stage (R2) decimation, shared by all channels (R2_RATE codes).
enum class ADS1293_R2 : uint8_t {
    R2_4 = 0x01,
    R2_5 = 0x02,
    R2_6 = 0x04,
    R2_8 = 0x08
};

/// Third-stage (R3) decimation, per channel (R3_RATE_CHx codes).
enum class ADS1293_R3 : uint8_t {
    R3_4   = 0x01,
    R3_6   = 0x02,
    R3_8   = 0x04,
    R3_12  = 0x08,
    R3_16  = 0x10,
    R3_32  = 0x20,
    R3_64  = 0x40,
    R3_128 = 0x80
};

/// Internal test signal (FLEX_CHx_CN TSTx). Positive/Negative are +/-VREF/12 at the INA input.
enum class ADS1293_TestSignal : uint8_t {
    Off      = 0,
    Positive = 1,
    Negative = 2,
    Zero     = 3   ///< INA inputs shorted - offset and noise measurement
};

/// Clock source (OSC_CN SHDN_OSC).
enum class ADS1293_ClockSource : uint8_t {
    Crystal  = 0,  ///< On-board 4.096 MHz crystal on XTAL1/XTAL2
    External = 1   ///< 409.6 kHz on the CLK pin (multi-board slave)
};

/// Data source for the DRDYB and SYNCB pins (DRDYB_SRC / SYNCB_CN codes).
enum class ADS1293_DataSource : uint8_t {
    None  = 0x00,
    Pace1 = 0x01,
    Pace2 = 0x02,
    Pace3 = 0x04,
    Ecg1  = 0x08,
    Ecg2  = 0x10,
    Ecg3  = 0x20
};

/// Comparator threshold for analog AC lead-off (LOD_CN ACLVL_LOD). See datasheet Figure 24.
enum class ADS1293_ACLeadOffLevel : uint8_t {
    Level1 = 0,
    Level2 = 1,
    Level3 = 2,
    Level4 = 3
};

/// Capacitive-load drive of the RLD amplifier and common-mode detector.
enum class ADS1293_CapDrive : uint8_t {
    Low        = 0,
    MediumLow  = 1,
    MediumHigh = 2,
    High       = 3
};

/// Where the analog pace channel output goes (AFE_PACE_CN).
enum class ADS1293_PaceOutput : uint8_t {
    None  = 0,  ///< Powered up but not routed
    Wct   = 1,  ///< WCT pin (disconnects the Wilson output from WCT)
    RldIn = 2   ///< RLDIN pin (RLD amplifier then uses RLDREF internally)
};

/// SPI data-out drive strength (DIGO_STRENGTH). Use the lowest that works.
enum class ADS1293_DriveStrength : uint8_t {
    Low        = 0,
    MediumLow  = 1,
    MediumHigh = 2,
    High       = 3  ///< Chip default
};

// ============================================================================
// Data types
// ============================================================================

/// One read from the loop-read registers. Values of channels that are not
/// enabled for read-back stay 0. Values whose ready bit is clear are the
/// previous conversion (the chip retains them until overwritten).
struct ADS1293_Sample {
    int32_t ecg[3];      ///< ECG, signed counts around mid-scale (0 = 0 V differential). See countsToMicrovolts()
    int16_t pace[3];     ///< Pace, signed counts around mid-scale (only for channels with pace read-back enabled)
    uint8_t ecgUpdated;  ///< Bit n set: ecg[n] is a new conversion
    uint8_t paceUpdated; ///< Bit n set: pace[n] is a new conversion
    uint8_t leadOff;     ///< ERROR_LOD flags (ADS1293_IN1..IN6 / ADS1293_ELECTRODE_*), read when an alarm is active
    uint8_t errors;      ///< ERROR_STATUS flags (ADS1293_ERR_*) latched since the previous alarm read
};

/// Full device configuration applied by begin(). Defaults: 3-lead ECG at
/// 200 SPS from the on-board crystal, right-leg drive on RL, common-mode
/// detection on RA/LA/LL and DC lead-off detection on the ECG electrodes.
struct ADS1293_Config {
    ADS1293_LeadSet     leadSet  = ADS1293_LeadSet::ThreeLead;
    ADS1293_DataRate    dataRate = ADS1293_DataRate::SPS_200;
    ADS1293_ClockSource clock    = ADS1293_ClockSource::Crystal;
    bool clockOutput    = false;  ///< EN_CLKOUT: drive the 409.6 kHz clock out on CLK (crystal only; multi-board master)

    bool highResolution = false;  ///< EN_HIRES on every channel: lower INA noise for more supply current

    bool rld = true;              ///< Right-leg drive into RL (IN4)

    bool     leadOff        = true;  ///< DC lead-off detection on the electrodes of the lead set
    uint16_t leadOffCurrent = 32;    ///< Lead-off current in nA (0-2040, 8 nA steps)

    ADS1293_TestSignal testSignal = ADS1293_TestSignal::Off;  ///< Replace the electrode inputs of every enabled channel

    bool autoStart = true;           ///< Start conversions and check the clock at the end of begin()
};

// ============================================================================
// Driver class
// ============================================================================

class Protocentral_ADS1293 {
public:
    /**
     * @param csPin     Chip select
     * @param drdyPin   Data ready (active low), or -1 to poll the DATA_STATUS register instead
     * @param resetPin  RSTB, or -1 if not wired (begin() then stops the chip and rewrites every register)
     * @param spi       SPI bus
     */
    Protocentral_ADS1293(uint8_t csPin, int8_t drdyPin = -1, int8_t resetPin = -1, SPIClass& spi = SPI);

    // ---- Initialisation ----------------------------------------------------

    /** @brief Reset the chip, check REVID and apply the default 3-lead profile. */
    bool begin(bool startSPI = true);

    /** @brief Reset the chip, check REVID and apply @p config. */
    bool begin(const ADS1293_Config& config, bool startSPI = true);

#if defined(ARDUINO_ARCH_ESP32)
    /** @brief ESP32 only: start SPI on custom pins, then begin(config). */
    bool begin(int8_t sck, int8_t miso, int8_t mosi, const ADS1293_Config& config = ADS1293_Config());
#endif

    /** @brief Stop conversions, power the chip down and release the DRDY interrupt. */
    void end();

    bool    isConnected();
    uint8_t readID();
    ADS1293_Status lastError() const { return _lastError; }
    static const char* getLibraryVersion() { return ADS1293_LIBRARY_VERSION; }

    // ---- Conversion and power ----------------------------------------------

    bool start();      ///< Start conversions (leaves standby / power-down)
    bool stop();       ///< Stop conversions
    bool standby();    ///< Stop and enter standby: most circuits off, fast return
    bool powerDown();  ///< Stop and enter power-down: minimum current
    bool wakeup();     ///< Leave standby / power-down and restart conversions
    bool isRunning() const { return _regs[ADS1293_REG_CONFIG] & ADS1293_CONFIG_START_CON; }

    // ---- Data rate and digital filters -------------------------------------

    /** @brief Set R2 and every channel's R3 from a preset. Clears FS_HIGH and double pace rate. */
    bool setDataRate(ADS1293_DataRate rate);

    /** @brief Set the shared R2 decimation directly. */
    bool setR2(ADS1293_R2 r2);

    /** @brief Set one channel's R3 decimation directly (channels may run at different rates). */
    bool setR3(ADS1293_Channel ch, ADS1293_R3 r3);

    /** @brief Double pace data rate (R1 = 2 instead of 4); doubles the pace and ECG data rate of the channel. */
    bool setDoublePaceRate(ADS1293_Channel ch, bool enable);

    /** @brief Run the channel's sigma-delta modulator at 204.8 kHz instead of 102.4 kHz (doubles its data rates). */
    bool setHighSampleClock(ADS1293_Channel ch, bool enable);

    /** @brief Bypass the third (ECG) SINC filter of a channel. Not allowed on the DRDY source channel. */
    bool setEcgFilterBypass(ADS1293_Channel ch, bool bypass);

    float getSampleRate(ADS1293_Channel ch = ADS1293_Channel::CH1) const;  ///< ECG samples per second
    float getPaceRate(ADS1293_Channel ch = ADS1293_Channel::CH1) const;    ///< Pace samples per second

    // ---- Reading data ------------------------------------------------------

    /** @brief True when new data is waiting (DRDY low, the DRDY interrupt fired, or DATA_STATUS says so). */
    bool available();

    /** @brief Read the latest data. Call when available() is true. */
    bool read(ADS1293_Sample& sample);

    /** @brief Latch DRDY with an interrupt instead of polling. Needs a DRDY pin with interrupt support. */
    bool enableDataReadyInterrupt();
    void disableDataReadyInterrupt();

    /** @brief Data-ready events that arrived before the previous one was read (interrupt mode only). */
    uint32_t missedSamples() const { return _missed; }

    /** @brief Read ECG / pace data back in loop mode (CH_CNFG). The ECG of enabled channels is on by default. */
    bool setEcgReadback(ADS1293_Channel ch, bool enable);
    bool setPaceReadback(ADS1293_Channel ch, bool enable);

    /**
     * @brief Choose what drives DRDYB. By default the driver picks the fastest
     * read-back source automatically; call with ADS1293_DataSource::None to go back to that.
     */
    bool setDataReadySource(ADS1293_DataSource source);

    /** @brief MASK_DRDYB: keep DRDYB masked for the first samples after start / after a sync error (both default true). */
    bool setDataReadyMasking(bool maskAtStart, bool maskAfterSyncError);

    // ---- Channels and input routing ----------------------------------------

    /** @brief Route input pins to a channel's INA (FLEX_CHx_CN). Keeps the test-signal setting. */
    bool setChannelInputs(ADS1293_Channel ch, ADS1293_Input positive, ADS1293_Input negative);

    /** @brief Power a channel's INA and modulator up or down and add / remove it from read-back. */
    bool setChannelEnabled(ADS1293_Channel ch, bool enable);
    bool isChannelEnabled(ADS1293_Channel ch) const;

    /** @brief Internal test signal on one channel, or on every enabled channel. */
    bool setTestSignal(ADS1293_Channel ch, ADS1293_TestSignal signal);
    bool setTestSignal(ADS1293_TestSignal signal);

    /** @brief EN_HIRES: high-resolution INA mode for one channel. */
    bool setHighResolution(ADS1293_Channel ch, bool enable);

    /** @brief INA output fault detection (OUTP/OUTN near rail). DIF_HIGH and SIGN always work. */
    bool setFaultDetection(ADS1293_Channel ch, bool enable);

    // ---- Lead-off detection ------------------------------------------------

    /**
     * @brief DC lead-off: inject @p currentNanoAmps into each pin in @p inputs and
     * flag pins that rise towards VDD (no return path through the body).
     * Needs the RLD (or another low-impedance return) on the patient.
     */
    bool configureLeadOffDC(uint8_t inputs, uint16_t currentNanoAmps = 32);

    /**
     * @brief Analog AC lead-off: inject an AC current and compare the detected amplitude.
     * Works with capacitive (dry) electrodes.
     * @param frequencyHz  Excitation, 12.5 kHz down to ~6 Hz (rounded to the nearest divider)
     */
    bool configureLeadOffAnalogAC(uint8_t inputs, uint16_t currentNanoAmps, uint16_t frequencyHz,
                                  ADS1293_ACLeadOffLevel level = ADS1293_ACLeadOffLevel::Level1);

    /**
     * @brief Digital AC lead-off: inject a balanced AC current into whole channels and
     * measure the tone in the ECG data (series impedance of the two electrodes).
     * @param channels   Bit n = channel n+1
     * @param antiPhase  Bit n = invert the current on channel n+1 (see datasheet 8.3.18)
     */
    bool configureLeadOffDigitalAC(uint8_t channels, uint16_t currentNanoAmps, uint16_t frequencyHz,
                                   uint8_t antiPhase = 0);

    /** @brief Turn lead-off detection off. */
    bool disableLeadOff();

    /** @brief ERROR_LOD: pins currently flagged as off (ADS1293_IN1..IN6). */
    uint8_t readLeadOffStatus();

    // ---- Common mode, right-leg drive, reference terminals -----------------

    /** @brief Common-mode detector: pins averaged (ADS1293_IN1..IN6), bandwidth and load drive. */
    bool configureCommonModeDetect(uint8_t inputs, bool highBandwidth = false,
                                   ADS1293_CapDrive capDrive = ADS1293_CapDrive::Low);

    /** @brief Right-leg drive amplifier and the pin it drives. */
    bool configureRLD(bool enable, ADS1293_Input output = ADS1293_Input::IN4, bool highBandwidth = false,
                      ADS1293_CapDrive capDrive = ADS1293_CapDrive::Low);

    /**
     * @brief Wilson central terminal from three pins; optionally connect it to IN6 internally.
     * Pass ADS1293_Input::None for all three to disable.
     */
    bool configureWilson(ADS1293_Input a, ADS1293_Input b, ADS1293_Input c, bool connectToIN6 = true);

    /**
     * @brief Goldberger terminals G1-G3 from the three Wilson buffers, connected internally
     * to IN4, IN5 and IN6. On the ProtoCentral breakout IN4 is the RL electrode: disable the
     * RLD first and see the README before using this.
     */
    bool configureGoldberger(ADS1293_Input a, ADS1293_Input b, ADS1293_Input c);

    /** @brief Internal 2.4 V reference and CM/RLD reference on or off (off = external CVREF). */
    bool setReference(bool internalReference, bool commonModeReference = true);

    // ---- Pace --------------------------------------------------------------

    /**
     * @brief Analog pace channel (fourth INA, gain 3.5): inputs and output routing.
     * Pass enable = false to shut it down (the default).
     */
    bool configureAnalogPace(bool enable, ADS1293_Input positive = ADS1293_Input::None,
                             ADS1293_Input negative = ADS1293_Input::None,
                             ADS1293_PaceOutput output = ADS1293_PaceOutput::None);

    // ---- Clock and synchronization -----------------------------------------

    /** @brief Drive the 409.6 kHz clock out on CLK (crystal source only). */
    bool setClockOutput(bool enable);

    /**
     * @brief SYNCB pin. As an output it is driven from @p source (use the slowest channel);
     * as an input (default) the filters follow an external SYNCB.
     */
    bool configureSync(bool output, ADS1293_DataSource source = ADS1293_DataSource::None);

    /** @brief SDO drive strength. */
    bool setDriveStrength(ADS1293_DriveStrength strength);

    // ---- Errors and alarms -------------------------------------------------

    /** @brief ERROR_STATUS (ADS1293_ERR_*). Reading clears the latched flags and releases ALARMB. */
    uint8_t readErrorStatus();

    /** @brief ERROR_RANGEx of one channel (ADS1293_RANGE_*). */
    uint8_t readRangeErrors(ADS1293_Channel ch);

    /** @brief ERROR_SYNC (ADS1293_SYNC_*). */
    uint8_t readSyncErrors();

    /** @brief ERROR_MISC: live CMOR / RLDRAIL / BATLOW status (ADS1293_MISC_*). */
    uint8_t readMiscErrors();

    /** @brief MASK_ERR: ADS1293_ERR_* flags that should not pull ALARMB low. */
    bool setAlarmMask(uint8_t mask);

    /** @brief ALARM_FILTER: consecutive counts (1-16) an alarm must persist before it is accepted. */
    bool setAlarmFilter(uint8_t leadOffCounts, uint8_t otherCounts);

    // ---- Units and measurements --------------------------------------------

    /** @brief Convert ECG counts to microvolts at the input, for the channel's current filter settings. */
    float countsToMicrovolts(int32_t counts, ADS1293_Channel ch = ADS1293_Channel::CH1) const;

    /** @brief Convert pace counts to microvolts at the input. */
    float paceCountsToMicrovolts(int16_t counts) const;

    /** @brief Full-scale code (ADCMAX) of a channel's ECG output. */
    uint32_t getAdcMax(ADS1293_Channel ch = ADS1293_Channel::CH1) const;

    /**
     * @brief Measure the chip supply (VDD) with the battery monitor. Briefly borrows
     * @p ch (CH3 by default, unused in 3-lead mode) and blocks for a few sample periods.
     * Not valid when VDD is a regulated 5 V. Returns NAN on error.
     */
    float readSupplyVoltage(ADS1293_Channel ch = ADS1293_Channel::CH3);

    // ---- Raw register access -----------------------------------------------

    uint8_t readRegister(uint8_t reg);
    bool    writeRegister(uint8_t reg, uint8_t value);  ///< Configuration registers only; verified
    void    printRegisters(Stream& out = Serial);

private:
    uint8_t     _csPin;
    int8_t      _drdyPin;
    int8_t      _resetPin;
    SPIClass*   _spi;
    SPISettings _spiSettings;

    uint8_t _regs[ADS1293_NUM_CONFIG_REGS];  // shadow copy of the configuration registers
    bool    _initialized;
    bool    _autoDrdy;                       // driver picks the DRDYB source
    ADS1293_Status _lastError;

    // Loop read-back layout, derived from CH_CNFG and DRDYB_SRC
    uint8_t _frameLen;
    uint8_t _readyMask;

    ADS1293_Sample _pendingSample;
    bool           _pending;

    volatile bool     _drdyFlag;
    volatile uint32_t _missed;
    int8_t            _irqSlot;

    uint8_t rawReadRegister(uint8_t reg);
    void    rawWriteRegister(uint8_t reg, uint8_t value);
    bool    updateRegister(uint8_t reg, uint8_t value);
    bool    updateBits(uint8_t reg, uint8_t mask, uint8_t bits);
    bool    applyConfig(const ADS1293_Config& config);
    bool    startClock(ADS1293_ClockSource source, bool clockOutput);
    bool    waitForConversion(uint32_t timeoutMs);
    bool    readFrame(ADS1293_Sample& sample);
    bool    selectDataReadySource();
    void    updateLayout();
    void    hardwareReset();
    bool    setLeadOff(uint8_t lodCn, uint8_t lodEn, uint16_t currentNanoAmps, uint8_t acCn);

    uint8_t channelR1(uint8_t ch) const;
    uint8_t channelR2() const;
    uint8_t channelR3(uint8_t ch) const;
    float   modulatorRate(uint8_t ch) const;

    static bool    isLocked(uint8_t reg);
    static uint8_t writableMask(uint8_t reg);
    static uint8_t acDivider(uint16_t frequencyHz);
    static bool    validChannel(ADS1293_Channel ch) { return static_cast<uint8_t>(ch) < 3; }

    static Protocentral_ADS1293* _irqInstances[ADS1293_MAX_IRQ_INSTANCES];
    static void isr0();
    static void isr1();
    void handleDataReady();
};

#endif  // PROTOCENTRAL_ADS1293_H
