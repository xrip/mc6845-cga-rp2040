# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a hardware project implementing a **MC6845-based Color Text Video Adapter** using a Raspberry Pi Pico (RP2040). The system recreates classic video display functionality similar to MDA/CGA adapters but uses modern programmable logic (GAL/ATF chips) to minimize component count.

The project combines:
- **Hardware control code** (C/RP2040) for MC6845 interface
- **Programmable logic** (GAL16V8 chips) for glue logic and color processing
- **ROM data** for character fonts and video memory

## Architecture

### Core Components
- **MC6845 CRTC**: Master video timing controller (controlled via RP2040)
- **Text/Attribute ROMs**: Video memory storage (EPROM 27128)
- **Character ROM**: Font data (EPROM 2764) 
- **GAL16V8 chips**: Two programmable logic chips for timing and color processing
- **RP2040**: Interfaces with MC6845, generates precise clock signals

### Data Flow
1. MC6845 generates memory addresses (MA0-MA13) and row addresses (RA0-RA2)
2. Text ROM provides ASCII codes, Attribute ROM provides color data
3. Character ROM converts ASCII + row address to 8-bit pixel patterns
4. 74HC165 shift register serializes pixel data
5. Two GAL chips process pixels: timing/cursor effects → color multiplexing
6. Final output: 4-bit RGBI color signals

## Build Commands

### Raspberry Pi Pico (Main Firmware)
```bash
# Build the RP2040 firmware using CMake
mkdir -p cmake-build-minsizerel
cd cmake-build-minsizerel
cmake -DCMAKE_BUILD_TYPE=MinSizeRel ..
make -j4

# Output files will be in bin/ directory
# Flash the .uf2 file to Pico in bootloader mode
```

### GAL Programming Logic
```bash
# Compile the GAL logic files (requires galette.exe)
cd pld
./build.bat  # or on Windows: build.bat

# This generates .jed files for programming ATF16V8 chips
# character.jed - timing and cursor logic
# attribute.jed - color multiplexer logic
```

## Key Files

### Hardware Interface (`main.c`)
- MC6845 register initialization and control
- GPIO configuration for data/address buses  
- Clock generation using PIO (Programmable I/O)
- Real-time monitoring of MA/RA address signals

### Programmable Logic (`pld/`)
- `character.pld`: 3-bit counter, clock division, cursor XOR, sync inversion
- `attribute.pld`: 4-bit color multiplexing with display enable gating
- `galette.exe`: GAL compiler for generating programming files

### Clock Generation (`clock_pio.h`)
- PIO-based precise clock generation at ~1.79MHz (NTSC-derived timing)
- Configurable frequency for MC6845 character clock

### Font Data (`rom.h`)
- 2KB character ROM data (8x8 pixel font)
- Standard ASCII character set with extended characters

## Development Workflow

1. **Firmware changes**: Edit `main.c`, rebuild with CMake, flash .uf2 to Pico
2. **Logic changes**: Edit `.pld` files, run `build.bat`, program new .jed files to GAL chips
3. **Timing adjustments**: Modify clock frequencies in `clock_pio.h` or MC6845 register defaults
4. **Font changes**: Update `rom.h` with new character pattern data

## Hardware Dependencies
- Requires PICO_SDK_PATH environment variable
- GAL programming requires physical chip programmer
- 16MB flash Pico variant assumed (`PICO_FLASH_SIZE_BYTES=16777216`)

## Pin Assignments
- **MC6845 Control**: GPIO 25-29 (CLK, CS, RS, E, RW)
- **Data Bus**: GPIO 17-24 (D0-D7)  
- **Address Monitoring**: GPIO 0-16 (MA0-MA13, RA0-RA2)