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

## GAL Programming Guide

This section provides comprehensive guidance for programming GAL16V8 and GAL20V8 chips used in this project.

### GAL Source File Structure (.pld files)

Every GAL source file must follow this strict format:

```
GAL16V8         ; Chip type (line 1) - GAL16V8, GAL20V8, GAL22V10, or GAL20RA10
SIGNATURE       ; 8-character signature (line 2) - written into GAL for identification

; Pin declarations (2 lines total)
Pin1  Pin2  Pin3  Pin4  Pin5  Pin6  Pin7  Pin8  Pin9  GND     ; Pins 1-10
Pin11 Pin12 Pin13 Pin14 Pin15 Pin16 Pin17 Pin18 Pin19 VCC     ; Pins 11-20

; Boolean equations
OUTPUT1 = INPUT1 * INPUT2 + /INPUT3
OUTPUT2.T = INPUT4 * INPUT5      ; Tristate output
OUTPUT2.E = /INPUT6              ; Tristate enable
OUTPUT3.R = INPUT7 * /OUTPUT3    ; Registered output

DESCRIPTION
Optional description text explaining the GAL's function.
Can span multiple lines. Everything after DESCRIPTION is ignored by assembler.
```

### Pin Naming Conventions

- **Reserved pins**: `GND` (pin 10/12), `VCC` (pin 20/24)
- **Unused pins**: `NC` (Not Connected)
- **Clock pins**: Any descriptive name (e.g., `DOTCLK`, `CLK`, `SYSCLK`)
- **Active-low signals**: Prefix with `/` (e.g., `/RESET`, `/OE`, `/LOAD`)
- **Signal names**: Use descriptive names (e.g., `HSYNC`, `PIXELS`, `CHARCLK`)

### GAL16V8/GAL20V8 Architecture and Modes

#### Pin Configuration
- **GAL16V8**: 20-pin DIP (8 inputs + 8 configurable I/O)
- **GAL20V8**: 24-pin DIP (12 inputs + 8 configurable I/O)

#### Operating Modes
The GAL automatically selects operating mode based on output types used:

**Mode 1 (Combinational)**:
- No registered or tristate outputs used
- All outputs are combinational
- GAL16V8: 10 inputs max, 8 outputs
- GAL20V8: 14 inputs max, 8 outputs

**Mode 2 (Tristate)**:
- At least one tristate output (`.T`) used, no registered outputs
- Pin 1 becomes general input, pin 11 becomes general input (GAL16V8)
- Pin 1 becomes general input, pin 13 becomes general input (GAL20V8)

**Mode 3 (Registered)**:
- At least one registered output (`.R`) used
- Pin 1 becomes CLOCK input, pin 11 becomes /OE input (GAL16V8)
- Pin 1 becomes CLOCK input, pin 13 becomes /OE input (GAL20V8)

### Boolean Logic Syntax

#### Basic Operators
- `*` : AND operation
- `+` : OR operation  
- `/` : NOT operation (negation)
- `=` : Assignment

#### Output Types
- **Combinational**: `OUTPUT = logic_expression`
- **Tristate**: `OUTPUT.T = logic_expression` + `OUTPUT.E = enable_expression`
- **Registered**: `OUTPUT.R = logic_expression`

#### Operator Precedence
1. `/` (NOT) - highest precedence
2. `*` (AND) 
3. `+` (OR) - lowest precedence

Use parentheses to override: `OUTPUT = A * (B + C)`

### Product Term Limitations

GAL chips have limited product terms per output:

#### GAL16V8/GAL20V8
- **Combinational outputs**: 8 product terms max
- **Tristate outputs**: 7 product terms max (1 reserved for tristate control)
- **Registered outputs**: 8 product terms max
- **Tristate enable**: 1 product term only (no OR operations)

#### Optimization Strategies

**1. De Morgan's Laws**
Convert positive logic to negative to reduce product terms:
```
; Instead of this (may exceed product terms):
B = DE * PALETTE * (Q1*Q0*D7*D6 + /Q1*Q0*D5*D4 + ...)

; Use this (fewer product terms):
/B = /DE + /PALETTE + /Q1*/Q0*/D7*/D6 + /Q1*Q0*/D5*/D4 + ...
```

**2. Factor Common Terms**
```
; Instead of:
OUT = A*B*C + A*B*D + A*B*E

; Use:
OUT = A*B*(C + D + E)
```

**3. Use Intermediate Signals**
Break complex expressions across multiple outputs when possible.

### Common Patterns and Examples

#### 1. Binary Counter (Registered Outputs)
```
GAL16V8
COUNTER

CLOCK NC NC NC NC NC NC NC NC GND
/OE   NC NC NC NC Q2 Q1 Q0 NC VCC

Q0.R = /Q0                           ; Toggle every clock
Q1.R = Q1*/Q0 + /Q1*Q0               ; Toggle when Q0 goes high
Q2.R = Q2*/Q1 + Q2*/Q0 + /Q2*Q1*Q0   ; Toggle when Q1,Q0 both high

DESCRIPTION
3-bit binary counter
```

#### 2. Combinational Decoder
```
GAL16V8
DECODER

A0 A1 A2 A3 NC NC NC NC NC GND
NC NC NC NC Y3 Y2 Y1 Y0 NC VCC

Y0 = /A1 * /A0
Y1 = /A1 *  A0  
Y2 =  A1 * /A0
Y3 =  A1 *  A0

DESCRIPTION
2-to-4 decoder
```

#### 3. Tristate Buffer
```
GAL16V8
TRIBUFFER

D0 D1 D2 D3 /OE NC NC NC NC GND
NC NC NC NC Q3 Q2 Q1 Q0 NC VCC

Q0.T = D0
Q1.T = D1
Q2.T = D2
Q3.T = D3

Q0.E = /OE
Q1.E = /OE
Q2.E = /OE
Q3.E = /OE

DESCRIPTION
4-bit tristate buffer
```

### Debugging and Best Practices

#### 1. Systematic Design Process
1. Define all input/output signals clearly
2. Create truth tables for complex logic
3. Write equations incrementally
4. Test each output independently
5. Use the GAL optimizer for final simplification

#### 2. Common Mistakes to Avoid
- **Exceeded product terms**: Use De Morgan's laws or factor expressions
- **Incorrect pin assignments**: Double-check pin numbers against datasheet
- **Tristate enable complexity**: Keep tristate enable to single product term
- **Feedback timing**: Be careful with registered output feedback in same equation

#### 3. Verification Methods
- **Simulation**: Use logic simulator before programming
- **Documentation files**: Review .chp and .pin files for correct pin assignments  
- **Fuse map**: Check .fus file to verify logic implementation
- **Hardware testing**: Use logic analyzer or oscilloscope

#### 4. Design Guidelines
- **Use descriptive names**: Make pin names self-documenting
- **Add comprehensive comments**: Explain complex logic thoroughly  
- **Keep equations readable**: Use line breaks and indentation for clarity
- **Document operating modes**: Clearly state which GAL mode is used
- **Include pin descriptions**: Document each pin's function

### Project-Specific GAL Usage

#### character.pld (ATF16V8)
- **Mode**: Registered (clock on pin 1, /OE on pin 11)
- **Function**: 3-bit counter, sync inversion, cursor XOR, shift register control
- **Key techniques**: Registered counter with feedback, XOR implementation

#### attribute.pld (ATF16V8)  
- **Mode**: Combinational
- **Function**: 4-bit color multiplexer with display enable gating
- **Key techniques**: Conditional output selection, enable gating

#### graphics.pld (ATF20V8)
- **Mode**: Registered (clock on pin 1, /OE on pin 13)
- **Function**: CGA graphics mode processor with pixel counter
- **Key techniques**: De Morgan's optimization, complex pixel extraction logic

### GAL Assembler Usage (galette.exe)

The GAL assembler converts .pld source files to .jed programming files:

#### Command Line Usage
```bash
galette source.pld          # Creates source.jed
```

#### Generated Files
- `.jed` - JEDEC programming file (required for programming)
- `.chp` - Chip diagram (documentation)
- `.pin` - Pin assignment list (documentation)  
- `.fus` - Fuse map (documentation)

#### Assembly Process
1. Parse source file syntax
2. Optimize Boolean equations (Quine-McCluskey algorithm)
3. Map logic to GAL architecture
4. Generate JEDEC fuse programming data
5. Create documentation files

This comprehensive guide should provide all necessary knowledge for understanding, modifying, and creating GAL programs for this video adapter project.