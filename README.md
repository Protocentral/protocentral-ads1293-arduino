# ProtoCentral ADS1293 ECG Arduino Library

[![Arduino Lint](https://github.com/Protocentral/protocentral-ads1293-arduino/actions/workflows/arduino-lint.yml/badge.svg)](https://github.com/Protocentral/protocentral-ads1293-arduino/actions/workflows/arduino-lint.yml)
[![Compile Examples](https://github.com/Protocentral/protocentral-ads1293-arduino/actions/workflows/compile-examples.yml/badge.svg)](https://github.com/Protocentral/protocentral-ads1293-arduino/actions/workflows/compile-examples.yml)

Arduino library for the **ProtoCentral ADS1293 Breakout**: 3-lead and 5-lead ECG from the TI ADS1293 three-channel 24-bit analog front end. It exposes every feature of the chip, from lead-off detection and pace channels to the Wilson terminal and battery monitor, and runs on an Arduino Uno.

## Don't have one? [Buy it here](https://protocentral.com/product/protocentral-ads1293-breakout-board/)

![ProtoCentral ADS1293 Breakout v2](docs/assets/ads1293_breakout_v2.jpg)

## Links

| | |
|---|---|
| Product page | https://protocentral.com/product/protocentral-ads1293-breakout-board/ |
| Getting started guide (pinout, electrodes, OpenView) | https://protocentral.com/product/protocentral-ads1293-breakout-board/docs/getting-started/ |
| Hardware design files | https://github.com/Protocentral/protocentral_ads1293 |
| OpenView companion app | https://github.com/Protocentral/protocentral_openview |
| ADS1293 datasheet (TI SNAS602C) | https://www.ti.com/lit/ds/symlink/ads1293.pdf |

## How it works

The ADS1293 has three measurement channels, each with an instrumentation amplifier (fixed gain 3.5), a sigma-delta modulator and a digital filter. A switch matrix connects any of its six inputs to any channel. The library's lead sets use the breakout's electrode wiring:

| Channel | 3-lead | 5-lead |
|---|---|---|
| **CH1** | Lead I = LA − RA | Lead I = LA − RA |
| **CH2** | Lead II = LL − RA | Lead II = LL − RA |
| **CH3** | off | V1 − WCT (Wilson central terminal, built on-chip from RA, LA and LL) |

The right-leg drive holds the body at mid-supply through the RL electrode and cancels common-mode noise. Lead III = II − I.

## Features

- 3-lead and 5-lead ECG at 25–1600 SPS (up to 6.4 kSPS per channel), with any input-to-channel routing
- Lead-off detection: DC, analog AC and digital AC
- Digital pace data at up to 25.6 kSPS, and the analog pace channel
- Self-test signals, supply-voltage monitor and every error register
- Multi-board operation with a shared clock and SYNCB
- Polled, interrupt-driven, or no DRDY pin at all; readings converted to µV
- No dependencies. Runs on AVR, Uno R4, Nano 33 BLE, nRF52840, ESP32 (also S2/S3/C3) and RP2040

> **Not a medical device. For research and education only.** The breakout has no patient isolation: power the host from a battery when electrodes are on a person.

## Installation

- **Arduino Library Manager (recommended):** search for **ProtoCentral ADS1293** and install **ProtoCentral ADS1293 ECG Library**.
- **Manual:** download this repository as a ZIP, then use **Sketch → Include Library → Add .ZIP Library…**

## Wiring

The board pinout, electrode placement and power options are in the [getting started guide](https://protocentral.com/product/protocentral-ads1293-breakout-board/docs/getting-started/). The examples use each board's default SPI pins (SCK, MISO, MOSI) plus these:

| Breakout | Uno / Nano / R4 | ESP32 | ESP32-S2 / S3 | ESP32-C3 |
|---|---|---|---|---|
| CSB  | D4 | GPIO5 (SS) | SS (GPIO10 on S3) | GPIO7 (SS) |
| DRDY | D2 | GPIO4  | GPIO4 | GPIO3 |
| RST  | D5 | GPIO25 | GPIO5 | GPIO1 |
| SYNC | D6 | GPIO26 | GPIO6 | GPIO10 |

**Wire RST and SYNC to the MCU.** On the v2 board they reach the chip through a level shifter, with no pull-up on the header side. Connect both to MCU pins and pass those pins to the constructor; the library then holds them in a defined state. An RST or SYNC pin that is left undriven can stop conversions. (v1 boards have no SYNC pin: pass `-1` for it.)

Power the board from your MCU's I/O voltage: the header's logic level equals VCC.

## Quick Start

```cpp
#include <SPI.h>
#include "Protocentral_ADS1293.h"

Protocentral_ADS1293 ads(4 /*CS*/, 2 /*DRDY*/, 5 /*RST*/, 6 /*SYNC*/);

void setup() {
    Serial.begin(115200);
    if (!ads.begin()) {                        // 3-lead ECG at 200 SPS
        Serial.println((int)ads.lastError());  // see ADS1293_Status
        while (1);
    }
}

void loop() {
    ADS1293_Sample s;
    if (ads.available() && ads.read(s)) {
        Serial.print(ads.countsToMicrovolts(s.ecg[0]));  // Lead I, µV
        Serial.print(' ');
        Serial.println(ads.countsToMicrovolts(s.ecg[1], ADS1293_Channel::CH2));  // Lead II
    }
}
```

### Configuration

`begin()` with no arguments sets up 3-lead ECG at 200 SPS from the on-board crystal, with right-leg drive and DC lead-off detection. Pass an `ADS1293_Config` to change it:

```cpp
ADS1293_Config cfg;
cfg.leadSet  = ADS1293_LeadSet::FiveLead;   // ThreeLead, FiveLead or Custom
cfg.dataRate = ADS1293_DataRate::SPS_400;   // SPS_25 ... SPS_1600
cfg.leadOff  = false;
cfg.sync     = ADS1293_SyncMode::Auto;      // Master / Slave for multi-board sync
ads.begin(cfg);
```

The other fields (clock source, high-resolution mode, test signal, ...) are documented in `Protocentral_ADS1293.h`. Every setter also works later, while streaming.

Any `SPIClass` and any pin can be used: `Protocentral_ADS1293 ads(CS, DRDY, RST, SYNC, SPI1);`, with `-1` for pins that are not wired. On ESP32, `ads.begin(sck, miso, mosi)` selects custom SPI pins.

## Data format

`read()` fills an `ADS1293_Sample`:

| Field | Meaning |
|---|---|
| `ecg[3]` | ECG counts per channel, signed around mid-scale (0 = 0 V differential) |
| `pace[3]` | Pace counts (channels with pace read-back only) |
| `ecgUpdated` / `paceUpdated` | Bit *n* set: channel *n*+1 has a new conversion |
| `leadOff` | Lead-off flags (`ADS1293_ELECTRODE_RA` …), read while an alarm is active |
| `errors` | Error flags (`ADS1293_ERR_*`), read while an alarm is active |

`countsToMicrovolts(counts, channel)` converts to µV at the electrodes, using the full-scale code for the channel's current filter settings. The input range is ±400 mV differential.

## API Reference

| Method | Description |
|---|---|
| `Protocentral_ADS1293(cs, drdy = -1, reset = -1, sync = -1, SPIClass& spi = SPI)` | `-1` = pin not wired |
| `bool begin()` / `begin(const ADS1293_Config&)` | Reset, check the chip, configure, start and verify conversions |
| `ADS1293_Status lastError()` | `Ok`, `SpiError`, `WrongChipId`, `VerifyFailed`, `NoClock`, `Timeout`, ... |
| `bool available()` / `bool read(ADS1293_Sample&)` | New data is waiting / read it |
| `enableDataReadyInterrupt()` / `missedSamples()` | Latch DRDY with an interrupt / count overwritten samples |
| `dataReadyPinFault()` | True when DRDY did not follow the chip and the driver switched to polling |
| `reset()` / `end()` | Reset and re-apply the configuration / power down |
| `float countsToMicrovolts(counts, ch)` | Units |

The full feature API, grouped as in `Protocentral_ADS1293.h`, where each method is documented:

| Area | Methods |
|---|---|
| Conversion and power | `start`, `stop`, `standby`, `powerDown`, `wakeup` |
| Data rate and filters | `setDataRate`, `setR2`, `setR3`, `setDoublePaceRate`, `setHighSampleClock`, `setEcgFilterBypass`, `getSampleRate`, `getPaceRate` |
| Read-back and DRDY | `setEcgReadback`, `setPaceReadback`, `setDataReadySource`, `setDataReadyMasking` |
| Channels | `setChannelInputs`, `setChannelEnabled`, `setTestSignal`, `setHighResolution`, `setFaultDetection` |
| Lead-off | `configureLeadOffDC`, `configureLeadOffAnalogAC`, `configureLeadOffDigitalAC`, `disableLeadOff`, `readLeadOffStatus` |
| Patient reference | `configureCommonModeDetect`, `configureRLD`, `configureWilson`, `configureGoldberger`, `setReference` |
| Pace, clock, sync | `configureAnalogPace`, `setClockOutput`, `setSyncMode`, `configureSync`, `setDriveStrength` |
| Errors and monitoring | `readErrorStatus`, `readRangeErrors`, `readSyncErrors`, `readMiscErrors`, `setAlarmMask`, `setAlarmFilter`, `readSupplyVoltage` |
| Registers | `readRegister`, `writeRegister`, `printRegisters` |

## Examples

| # | Sketch | Description |
|---|---|---|
| 01 | `01.ThreeLeadSerialPlotter` | Leads I, II and III in µV in the Arduino Serial Plotter |
| 02 | `02.FiveLeadSerialPlotter` | Leads I, II and V1 (Wilson central terminal) |
| 03 | `03.OpenView` | Stream three leads to [OpenView](https://github.com/Protocentral/protocentral_openview) |
| 04 | `04.LeadOffDetection` | Report which electrode is off, DC or analog AC mode |
| 05 | `05.SelfTest` | Test signals and noise floor on all channels, no electrodes needed |
| 06 | `06.InterruptDriven` | 400 SPS with an interrupt on DRDY and a missed-sample counter |
| 07 | `07.PaceDetection` | 3.2 kSPS pace data and a simple pacemaker-pulse detector |
| 08 | `08.SupplyAndAlarms` | Supply voltage and every error register, decoded |
| 09 | `09.MultiBoardClockSync` | Two boards sharing clock and SYNCB: six ECG channels sampled in step |
| 10 | `10.Diagnostics` | Register dump plus clock, data-path, DRDY and signal-chain checks. **Run this first if something is wrong** |

All examples print at 115200 baud.

## Troubleshooting

Run `10.Diagnostics` first; it reports which stage fails.

- **`SpiError`:** check the CSB/SCK/MOSI/MISO wiring, VCC and GND. On ESP32-S2/S3/C3, do not use pins 18/19/23.
- **`VerifyFailed`:** registers do not read back correctly. Shorten the wires, or define `ADS1293_SPI_CLOCK 500000` before the include.
- **`NoClock`, or data that comes and goes between resets:** check that RST and SYNC are wired to the pins passed to the constructor (see Wiring), then power-cycle the board, not only the MCU.
- **DRDY stuck:** remove any pull-up or pull-down from DRDY. The library notices a DRDY pin that does not follow the chip and polls instead (`dataReadyPinFault()` returns true); `10.Diagnostics` reports it.

Electrode and signal-quality problems are covered in the [getting started guide](https://protocentral.com/product/protocentral-ads1293-breakout-board/docs/getting-started/).

## Migrating from 1.x

Version 2.0 is a ground-up rewrite; see [CHANGELOG.md](CHANGELOG.md).

| 1.x | 2.0 |
|---|---|
| `#include "protocentral_ads1293.h"` | `#include "Protocentral_ADS1293.h"` |
| `ads1293 ADS1293(DRDY, CS);` | `Protocentral_ADS1293 ads(CS, DRDY, RST, SYNC);` (note the order) |
| `begin()` + ten `configure…()` calls | `ads.begin()` or `ads.begin(cfg)`, which returns `bool` |
| `configureChannel3(...)` + `configureWilsonCentralTerminal()` | `cfg.leadSet = ADS1293_LeadSet::FiveLead` |
| `setSamplingRate(SamplingRate::SPS_128)` (actually 320 SPS) | `setDataRate(ADS1293_DataRate::SPS_320)` (names are the real rates) |
| `isDataReady()` + `getECGData()` | `if (ads.available() && ads.read(s))` |
| `samples.ch1` / `ch2` / `ch3` | `s.ecg[0]` / `s.ecg[1]` / `s.ecg[2]`, centred on 0 for 0 V input |
| `rawToVoltage(code)` | `ads.countsToMicrovolts(counts, channel)` |

## License

Software: MIT. See [LICENSE.md](LICENSE.md). Copyright (c) 2019-2026 ProtoCentral Electronics.

## Support

Open an issue on [GitHub](https://github.com/Protocentral/protocentral-ads1293-arduino/issues) or email support@protocentral.com.
