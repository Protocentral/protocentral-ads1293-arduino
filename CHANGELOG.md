# Changelog

All notable changes to this library are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/).

## [2.0.0] - 2026-10-26

Ground-up rewrite. **Breaking:** the 1.x API is removed. See "Migrating from 1.x" in the README.

### Added
- `Protocentral_ADS1293` driver class:
  - injected `SPIClass`, with an `SPISettings` transaction around every access
  - DRDY and RSTB pins optional (`-1`): without DRDY the driver polls DATA_STATUS
  - `bool begin()` with a REVID check and an `ADS1293_Status` from `lastError()`
  - `NoClock` status when the chip answers on SPI but makes no conversions
  - optional RST and SYNC pins, driven by the library before any SPI access. On the v2 breakout both reach the chip through the level shifter, and an undriven line can stop conversions
  - `ADS1293_SyncMode` (`Auto`, `Master`, `Slave`): without a SYNC pin the chip drives SYNCB itself, so an unconnected header pin cannot disturb it
  - `reset()` re-applies the last configuration after an RSTB pulse
- `ADS1293_Config` to set the complete device configuration in one call, with 3-lead, 5-lead and custom lead sets.
- Every register write is verified by read-back. Setters work while streaming: registers locked by `START_CON` are written with conversions briefly stopped.
- **Data path:**
  - one loop read (DATA_LOOP) per sample, with DATA_STATUS in every frame
  - ECG for all three channels and 16-bit digital pace data, with per-channel "updated" flags
  - lead-off and error flags attached to the sample whenever ALARMB is active
  - polled, DRDY-pin or interrupt-driven, with a missed-sample counter
  - `countsToMicrovolts()` / `paceCountsToMicrovolts()` using the correct ADCMAX for the current decimation
- **Every ADS1293 feature from the datasheet:**
  - 15 data-rate presets from 25 to 1600 SPS, plus raw R2 / per-channel R3, double pace rate (R1) and the 204.8 kHz modulator clock
  - ECG filter bypass, high-resolution INA mode, INA fault detection control
  - flexible input routing and positive / negative / zero test signals per channel
  - DC, analog AC and digital AC lead-off detection
  - common-mode detector and right-leg drive with bandwidth and capacitive-drive control
  - Wilson central terminal and Goldberger terminals
  - analog pace channel with WCT / RLDIN output routing
  - internal or external reference
  - crystal or external clock, clock output, SYNCB input / output
  - battery monitor (`readSupplyVoltage()`)
  - all error registers, alarm mask and alarm filter
  - DRDYB source selection (automatic by default) and masking, SDO drive strength
  - standby, power-down and wake-up
- **Examples 01-10:** 3-lead and 5-lead Serial Plotter, OpenView, lead-off, self-test, interrupt-driven, pace detection, supply and alarms, multi-board clock and SYNCB sync, diagnostics. All examples drive RST and SYNC.
- **Repo tooling:** `CHANGELOG.md`, Arduino Lint and an 11-board compile workflow (including ESP32-S3 and nRF52840), and `scripts/build.sh`.

### Changed
- Header renamed to `Protocentral_ADS1293.h` and the class to `Protocentral_ADS1293`. All public names carry an `ADS1293_` prefix.
- Constructor order is now `(cs, drdy, reset, sync, spi)` and takes the SPI bus by reference.
- ECG samples are signed counts around mid-scale (0 = 0 V differential) instead of a sign-extended raw code.
- The DRDY pin is configured as a plain input, without the internal pull-up, because the breakout drives it through a TXB0108 level shifter.
- The OpenView example now matches OpenView's `ads1293` descriptor baud rate (115200) and streams at 200 SPS.
- Default data rate is 200 SPS.

### Fixed
- `isDataReady()` tested the pace-ready and ALARMB bits instead of the ECG-ready bits, so examples printed repeated samples.
- Every sampling-rate preset was mislabelled by 2.5x (`SPS_128` ran at 320 SPS).
- ECG data were decoded as two's complement. The ADS1293 output is offset binary, so samples flipped sign above mid-scale + 0x200000 (about +37 mV at the input with R2 = 6).
- `rawToVoltage()` used the wrong formula and overflowed `int` on AVR (`1 << 23`).
- Conversions left running by a previous sketch locked the clock, rate and DRDY registers, so a reconfiguration after an MCU reset was silently ignored.
- The clock was started in one write. The driver now selects the source, waits for the oscillator, then sets STRTCLK, as the datasheet requires.
- The 3-lead profile left channel 3's modulator running.
- `DATA_CHx_PACE` register addresses were wrong.
- `disableChannel()` only handled channel 1, `attachTestSignal()` overwrote the input routing, and `disableFilter()` cleared the other channels' bits.
- ESP32 examples used SPI pins 18/19/23, which are the USB pins or do not exist on ESP32-S2/S3/C3.

### Removed
- `protocentral_ads1293.h`, the `ADS1293` / `ads1293` classes, the single-value option enums (`FlexCh1Mode`, `OscMode`, ...) and the global `SamplingRate` alias.
- `scripts/flash.sh` and `scripts/upload_uno_r4.sh` (both pointed at example paths that no longer existed).
- The vendor datasheet PDF from the repository; see the link in the README.
- `hardware/`: the board design files (v1 and v2) live in [protocentral_ads1293](https://github.com/Protocentral/protocentral_ads1293).

## [1.3.0] - 2025-12-19

- Last release of the original API.
