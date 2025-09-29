// MC6845 + RP2040 interface
// Cleaned up, more readable version
// Requires Pico SDK
// https://cpctech.cpcwiki.de/docs/mc6845/mc6845.htm
// https://minuszerodegrees.net/mda_cga_ega/mda_cga_ega.htm
// https://www.minuszerodegrees.net/oa/OA%20-%20IBM%20Color%20Graphics%20Monitor%20Adapter%20%28CGA%29.pdf

#include <stdio.h>
#include "pico/time.h"
#include "pico/stdio_usb.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include <hardware/structs/vreg_and_chip_reset.h>

#include "clock_pio.h"
#include "rom.h"

// ---------------- Pin assignments ----------------
#define PIN_MC6845_CS     26  // Chip Select (active low)
#define PIN_MC6845_RS     27  // Register Select (0=address, 1=data)
#define PIN_MC6845_E      28  // Enable (active edge high->low)
#define PIN_MC6845_RW     29  // Read/Write (0=write, 1=read)
#define PIN_MC6845_CLK    25  // Clock output pin

#define PIN_MA_BASE       0   // MA0..MA13 → GPIO0..13 (MC6845 address inputs - read only)
#define MA_WIDTH          14

#define PIN_RA_BASE       14  // RA0..RA2 → GPIO14..16 (MC6845 row address inputs - read only)
#define RA_WIDTH          3

#define PIN_DATA_BASE     17  // D0 = GPIO17 .. D7 = GPIO24 (MC6845 data bus)
#define DATA_WIDTH        8

// ---------------- System configuration ----------------
#define SYSTEM_CLOCK_HZ       (400 * MHZ)  // 400 MHz RP2040 core

// 80x25 (640x200) = 14.31818Mhz, 40x25 (320x200) = 7.15909Mhz
#define BASE_CLOCK_FREQ       (14.31818 * MHZ)
//#define BASE_CLOCK_FREQ       (7.15909 * MHZ)

// ---------------- Video modes ----------------
typedef enum {
    VIDEO_MODE_TEXT = 0,
    VIDEO_MODE_GRAPHICS = 1
} video_mode_t;

static video_mode_t current_video_mode = VIDEO_MODE_TEXT;

// ---------------- Video memory emulation ----------------
#define TEXT_BUFFER_SIZE (80 * 25)
#define GRAPHICS_BUFFER_SIZE (8000)  // 320x200/4 pixels per byte

// Simple test patterns for demonstration
static uint8_t text_buffer[TEXT_BUFFER_SIZE];
static uint8_t graphics_buffer[GRAPHICS_BUFFER_SIZE];
// Атрибуты задаются перемычками, не хранятся в RP2040

// Test pattern generation
static void init_test_patterns(void) {
    // Text mode: Fill with test characters
    for (int i = 0; i < TEXT_BUFFER_SIZE; i++) {
        text_buffer[i] = 0x20 + (i % 96); // ASCII printable chars
        // Атрибуты будут браться из перемычек
    }

    // Graphics mode: Fill with test pattern
    for (int i = 0; i < GRAPHICS_BUFFER_SIZE; i++) {
        graphics_buffer[i] = i & 0xFF; // Simple pattern
    }
}

// MC6845 register values for CGA 40x25 Text Mode
static const uint8_t mc6845_cga_40x25[16] = {
    0x38, // R0: Horizontal Total (56)
    0x28, // R1: Horizontal Displayed (40)
    0x2D, // R2: HSync Position (45)
    0x0A, // R3: HSync Width (10)
    0x1F, // R4: Vertical Total (31)
    0x06, // R5: VTotal Adjust (6)
    0x19, // R6: Vertical Displayed (25)
    0x1C, // R7: VSync Position (28)
    0x02, // R8: Interlace Mode (Non-interlaced)
    0x07, // R9: Max Scanline Address (7, for 8 lines per char)
    0x00, // R10: Cursor Start Line (6)
    0x07,  // R11: Cursor End Line (7)
    0,0,0,0
};

// MC6845 register values for CGA 80x25 Text Mode
static const uint8_t mc6845_cga_80x25[16] = {
    0x71, // R0: Horizontal Total (113)
    0x50, // R1: Horizontal Displayed (80)
    0x5A, // R2: HSync Position (90)
    0x0A, // R3: HSync Width (10)
    0x1F, // R4: Vertical Total (31)
    0x06, // R5: VTotal Adjust (6)
    0x19, // R6: Vertical Displayed (25)
    0x1C, // R7: VSync Position (28)
    0x02, // R8: Interlace Mode (Non-interlaced)
    0x07, // R9: Max Scanline Address (7, for 8 lines per char)
    0x06, // R10: Cursor Start Line (6)
    0x07,  // R11: Cursor End Line (7)
    0x00, // R12: Start Addr (H)
0x00, // R13: Start Addr (L)
0x00, // R14: Cursor Addr (H)
0x00 // R15: Cursor Addr (L)
};

// MC6845 register values for CGA 320x200 4-Color Graphics Mode
// MC6845 register values for CGA 640x200 2-Color Graphics Mode
// Note: Timing parameters are identical to 320x200 mode.
// The higher resolution is handled by the CGA's internal logic,
// selected via the Mode Control Register.
static const uint8_t mc6845_cga_320x200[16] = {
    0x38, // R0: Horizontal Total (56)
    0x28, // R1: Horizontal Displayed (40)
    0x2D, // R2: HSync Position (45)
    0x0A, // R3: HSync Width (10)
    0x7F, // R4: Vertical Total (127)
    0x06, // R5: VTotal Adjust (6)
    0x64, // R6: Vertical Displayed (100)
    0x70, // R7: VSync Position (112)
    0x02, // R8: Interlace Mode (Non-interlaced)
    0x01, // R9: Max Scanline Address (1, for 2 lines per "char row")
    0x00, // R10: Cursor Start (Cursor typically disabled)
    0x00,  // R11: Cursor End (Cursor typically disabled)
    0,0,0,0
};

// ---------------- Default register values ----------------
static const uint8_t mc6845_defaults[16] = {
    113, // R0: Horizontal Total
    80, // R1: Horizontal Displayed
    90, // R2: HSync Position
    10, // R3: HSync Width

    31, // R4: Vertical Total
    6, // R5: VTotal Adjust
    25, // R6: Vertical Displayed
    28, // R7: VSync Position
    2, // R8: Interlace & Skew
    7, // R9: Max Scanline
    0x00, // R10: Cursor Start
    0x0B, // R11: Cursor End
    0x00, // R12: Start Addr (H)
    0x00, // R13: Start Addr (L)
    0x00, // R14: Cursor Addr (H)
    0x00 // R15: Cursor Addr (L)
};


// ==========================================================
// Low-level helpers
// ==========================================================

static void data_bus_set_output(void) {
    const uint32_t mask = 0xFF << PIN_DATA_BASE;
    gpio_set_dir_masked(mask, mask);
}

// Вывод данных на шину данных MC6845 (используется для регистров и видеоданных)
__always_inline static void data_bus_write(const uint8_t value) {
    const uint32_t mask = 0xFF << PIN_DATA_BASE;
    gpio_put_masked(mask, (uint32_t) value << PIN_DATA_BASE);
}

__always_inline static uint8_t data_bus_read() {
    const uint32_t mask = 0xFF << PIN_DATA_BASE;
    return (uint8_t) ((gpio_get_all() & mask) >> PIN_DATA_BASE);
}

// Active edge high→low on E
__always_inline static void pulse_enable(void) {
    gpio_put(PIN_MC6845_E, 1);
    sleep_us(1);
    gpio_put(PIN_MC6845_E, 0);
    sleep_us(1);
}

// ==========================================================
// Register access
// ==========================================================

static void mc6845_write_register(const uint8_t reg, const uint8_t value) {
    data_bus_set_output();
    gpio_put(PIN_MC6845_CS, 0);
    gpio_put(PIN_MC6845_RW, 0);

    gpio_put(PIN_MC6845_RS, 0);
    data_bus_write(reg & 0x1F);
    pulse_enable();

    gpio_put(PIN_MC6845_RS, 1);
    data_bus_write(value);
    pulse_enable();

    gpio_put(PIN_MC6845_CS, 1);
}

// ==========================================================
// Initialization
// ==========================================================

static void init_all_gpio(void) {
    // MC6845 control pins
    for (int i = 0; i < 4; i++) {
        const uint8_t mc6845_pins[] = {PIN_MC6845_CS, PIN_MC6845_RS, PIN_MC6845_E, PIN_MC6845_RW};
        gpio_init(mc6845_pins[i]);
        gpio_set_dir(mc6845_pins[i], GPIO_OUT);
    }
    gpio_put(PIN_MC6845_CS, 1);
    gpio_put(PIN_MC6845_E, 0);

    // Data bus + Address monitoring
    for (int i = 0; i < 25; i++) {
        gpio_init(i);
        gpio_set_dir(i, i < 17 ? GPIO_IN : GPIO_OUT);
    }

    init_clock_pio(pio0, SM_CLOCK, PIN_MC6845_CLK, BASE_CLOCK_FREQ);

    // Setup MC6845 registers
    for (int r = 0; r < 16; r++) {
        mc6845_write_register(r, mc6845_cga_80x25[r]);
    }
}


static void process_video_address(const uint16_t address, const uint8_t row) {
    if (current_video_mode == VIDEO_MODE_TEXT) {
        data_bus_write(cga_font_8x8[text_buffer[address] * 8 + row]);
    } else if (current_video_mode == VIDEO_MODE_GRAPHICS) {
        data_bus_write(graphics_buffer[address]);
    }
}


void main() {
    // Configure RP2040 system clock
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    set_sys_clock_hz(SYSTEM_CLOCK_HZ, true);
    busy_wait_ms(25);

    stdio_usb_init();
    busy_wait_ms(1000);

    printf("CGA Video Emulator\nCommands: t/g/r\n");

    init_all_gpio();
    init_test_patterns();

    uint32_t prev_addr = 0xFFFFFFFF;

    uint8_t i = 0;
    while (1) {
        const uint32_t addr = gpio_get_all() & 0x1FFFF;

        if (addr != prev_addr) {
            prev_addr = addr;
            process_video_address(addr & 0x3FFF, addr >> 14);
        }

        int c = getchar_timeout_us(0);
        if (c == 't') current_video_mode = VIDEO_MODE_TEXT;
        else if (c == 'g') current_video_mode = VIDEO_MODE_GRAPHICS;
        else if (c == 'r') init_test_patterns();
        mc6845_write_register(15, i++);
        busy_wait_ms(100);
    }
}
