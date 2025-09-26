# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a hardware project implementing a **MC6845-based Color Text Video Adapter** using a Raspberry Pi Pico (RP2040). The system recreates classic video display functionality similar to MDA/CGA adapters but uses modern programmable logic (GAL/ATF chips) to minimize component count.

**Current Version**: v2.1 - Optimized and simplified codebase with ~50% reduction in code size and improved performance.

The project combines:
- **RP2040 microcontroller** serving multiple roles: MC6845 control, video memory emulation, character ROM, and address monitoring
- **Programmable logic** (GAL16V8/GAL20V8 chips) for timing, glue logic and color processing
- **Software-based video memory** with built-in character font data

## Architecture

### Core Components
- **MC6845 CRTC**: Master video timing controller (controlled via RP2040)
- **RP2040 (Raspberry Pi Pico)**: Multi-function controller:
  - MC6845 register programming and control
  - PIO-based precise clock generation (~1.79MHz)
  - Video memory emulation (Text + Attribute data)
  - Built-in character ROM (8x8 font from `rom.h`)
  - Real-time address monitoring (MA0-MA13, RA0-RA2)
- **GAL16V8/GAL20V8 chips**: Programmable logic for timing and color processing
- **74HC165**: Shift register for pixel serialization

### Data Flow
1. RP2040 programs MC6845 registers and generates clock via PIO
2. MC6845 generates memory addresses (MA0-MA13) and row addresses (RA0-RA2) 
3. RP2040 monitors address signals and provides:
   - ASCII codes and color attributes (software video memory)
   - 8-bit pixel patterns from built-in character ROM (`rom.h`)
4. 74HC165 shift register serializes pixel data from RP2040
5. GAL chips process pixels: timing/cursor effects → color multiplexing
6. Final output: 4-bit RGBI color signals + composite video

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

# This generates .jed files for programming GAL chips:
# character.jed - timing and cursor logic (ATF16V8)
# attribute.jed - color multiplexer logic (ATF16V8) 
# graphics.jed - CGA graphics mode processor (ATF20V8)
```

## Key Files

### Hardware Interface (`main.c`)
- MC6845 register initialization and control via GPIO
- PIO-based clock generation for precise MC6845 timing
- Video memory emulation (text and attribute data)
- Built-in character ROM lookup from `rom.h`
- Real-time monitoring of MA/RA address signals
- GPIO configuration for bidirectional data/address buses

### Programmable Logic (`pld/`)
- `character.pld`: 3-bit counter, clock division, cursor XOR, sync inversion (ATF16V8)
- `attribute.pld`: 4-bit color multiplexing with display enable gating + composite (ATF16V8)
- `graphics.pld`: CGA 320x200x4 graphics processor with De Morgan optimization (ATF20V8)
- `galette.exe`: GAL compiler for generating programming files

### Clock Generation (`clock_pio.h`)
- PIO-based precise clock generation at ~1.79MHz (NTSC-derived timing)
- Configurable frequency for MC6845 character clock
- State machine for reliable clock output

### Font Data (`rom.h`)
- 2KB character ROM data (8x8 pixel font)
- Standard ASCII character set with extended characters
- Embedded directly in RP2040 firmware for real-time access

## Development Workflow

1. **Firmware changes**: Edit `main.c`, rebuild with CMake, flash .uf2 to Pico
2. **Video memory changes**: Modify address monitoring and data generation logic in `main.c`
3. **Logic changes**: Edit `.pld` files, run `build.bat`, program new .jed files to GAL chips
4. **Timing adjustments**: Modify clock frequencies in `clock_pio.h` or MC6845 register defaults
5. **Font changes**: Update `rom.h` with new character pattern data (requires firmware rebuild)
6. **Mode switching**: Add logic in `main.c` to switch between text/graphics modes dynamically

## Code Organization (v2.1)

The codebase has been optimized for simplicity and performance:
- **Unified GPIO initialization**: Single `init_all_gpio()` function handles all pin setup
- **Streamlined main loop**: Direct address monitoring with minimal overhead
- **Simplified mode switching**: Direct variable assignment without wrapper functions
- **Minimal debug output**: Reduced printf overhead for better video timing
- **Compact functions**: Combined related operations to reduce call overhead

## Hardware Dependencies
- Requires PICO_SDK_PATH environment variable
- GAL programming requires physical chip programmer
- 16MB flash Pico variant assumed (`PICO_FLASH_SIZE_BYTES=16777216`)

## Pin Assignments
- **MC6845 Control**: GPIO 25-29 (CLK, CS, RS, E, RW)
- **MC6845 Data Bus**: GPIO 17-24 (D0-D7) - used for register programming, then repurposed
- **Address Monitoring**: GPIO 0-16 (MA0-MA13, RA0-RA2) - inputs from MC6845
- **Video Data Output**: GPIO 17-24 (data bus) - to 74HC165/graphics.pld
- **Attributes**: Set by hardware jumpers/switches (not controlled by RP2040)

## GPIO Usage Strategy
The design efficiently uses the MC6845 data bus (GPIO 17-24) for video output:
1. **MC6845 Register Programming**: Standard bidirectional interface for initial setup
2. **Video Data Output**: Same GPIO pins used for continuous video data stream
3. **Benefit**: MC6845 data bus is idle during normal operation (only used for register access)
4. **Result**: Maximum GPIO efficiency - dedicated video output with minimal pin count

## Hardware Configuration
- **Color Attributes**: Set via hardware jumpers or DIP switches
- **Palette Selection**: Hardware-controlled for graphics mode
- **RP2040 Focus**: Only video memory and character ROM emulation

## Current Implementation Status
The RP2040 firmware in `main.c` currently implements:
- ✅ MC6845 initialization and register programming
- ✅ PIO-based clock generation 
- ✅ Address monitoring and real-time display
- ✅ Built-in character ROM (`rom.h`)
- ✅ Active video data output (text and graphics modes)
- ✅ Efficient GPIO usage for video output
- ✅ Dynamic mode switching between text/graphics
- ✅ Test pattern generation