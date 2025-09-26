// MC6845 + RP2040 interface
// Cleaned up, more readable version
// Requires Pico SDK

#include <stdio.h>
#include "pico/time.h"
#include "pico/stdio_usb.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include <hardware/structs/vreg_and_chip_reset.h>

#include "clock_pio.h"

// ---------------- Pin assignments ----------------
#define PIN_MC6845_CS     26  // Chip Select (active low)
#define PIN_MC6845_RS     27  // Register Select (0=address, 1=data)
#define PIN_MC6845_E      28  // Enable (active edge high->low)
#define PIN_MC6845_RW     29  // Read/Write (0=write, 1=read)
#define PIN_MC6845_CLK    25  // Clock output pin

#define PIN_DATA_BASE     17  // D0 = GPIO17 .. D7 = GPIO24
#define DATA_WIDTH        8

#define PIN_MA_BASE       0   // MA0..MA13 → GPIO0..13
#define MA_WIDTH          14

#define PIN_RA_BASE       14  // RA0..RA2 → GPIO14..16
#define RA_WIDTH          3

// ---------------- System configuration ----------------
#define SYSTEM_CLOCK_HZ       (400 * MHZ)  // 400 MHz RP2040 core
//#define MC6845_CLOCK_FREQ     (1789772.2f)      // ~NTSC master clock / 10
#define BASE_CLOCK_FREQ (14.31818 * MHZ)
#define MC6845_CLOCK_FREQ     ((BASE_CLOCK_FREQ) / 8)      /

// ---------------- Default register values ----------------
static const uint8_t mc6845_defaults[16] = {
    0x64, // R0: Horizontal Total
    0x50, // R1: Horizontal Displayed
    0x54, // R2: HSync Position
    0x07, // R3: HSync Width
    0x1B, // R4: Vertical Total
    0x02, // R5: VTotal Adjust
    0x18, // R6: Vertical Displayed
    0x19, // R7: VSync Position
    0x00, // R8: Interlace & Skew
    0x0A, // R9: Max Scanline
    0x00, // R10: Cursor Start
    0x0B, // R11: Cursor End
    0x00, // R12: Start Addr (H)
    0x80, // R13: Start Addr (L)
    0x00, // R14: Cursor Addr (H)
    0x80  // R15: Cursor Addr (L)
};

// ==========================================================
// Low-level helpers
// ==========================================================

static void data_bus_dir_out(void) {
    for (int i = 0; i < DATA_WIDTH; i++) {
        gpio_set_dir(PIN_DATA_BASE + i, GPIO_OUT);
        gpio_set_pulls(PIN_DATA_BASE + i, false, false);
    }
}

static void data_bus_dir_in(void) {
    for (int i = 0; i < DATA_WIDTH; i++) {
        gpio_set_dir(PIN_DATA_BASE + i, GPIO_IN);
        gpio_set_pulls(PIN_DATA_BASE + i, false, false);
    }
}

__always_inline static void data_bus_write(const uint8_t value) {
    const uint32_t mask = ((1u << DATA_WIDTH) - 1u) << PIN_DATA_BASE;
    gpio_put_masked(mask, (uint32_t)value << PIN_DATA_BASE);
}

__always_inline static uint8_t data_bus_read() {
    const uint32_t mask = ((1u << DATA_WIDTH) - 1u) << PIN_DATA_BASE;
    return (uint8_t)((gpio_get_all() & mask) >> PIN_DATA_BASE);
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

// Write a value into one MC6845 register
static void mc6845_write_register(const uint8_t reg, const uint8_t value) {
    // Write register index
    data_bus_dir_out();
    gpio_put(PIN_MC6845_CS, 0);
    gpio_put(PIN_MC6845_RW, 0); // write
    gpio_put(PIN_MC6845_RS, 0); // address register
    data_bus_write(reg & 0x1F);
    pulse_enable();

    // Write register value
    gpio_put(PIN_MC6845_RS, 1); // data register
    data_bus_write(value);
    pulse_enable();

    gpio_put(PIN_MC6845_CS, 1);
}

// Read value from register (only valid for R14,R15,R16,R17 on original MC6845)
static uint8_t mc6845_read_register(const uint8_t reg) {
    // Write register index first
    data_bus_dir_out();
    gpio_put(PIN_MC6845_CS, 0);
    gpio_put(PIN_MC6845_RW, 0);
    gpio_put(PIN_MC6845_RS, 0);
    data_bus_write(reg & 0x1F);
    pulse_enable();

    // Now read data
    data_bus_dir_in();
    gpio_put(PIN_MC6845_RS, 1);
    gpio_put(PIN_MC6845_RW, 1);

    gpio_put(PIN_MC6845_E, 1);
    sleep_us(1);
    gpio_put(PIN_MC6845_E, 0);
    sleep_us(1);

    const uint8_t value = data_bus_read();

    gpio_put(PIN_MC6845_CS, 1);
    return value;
}

// ==========================================================
// Initialization
// ==========================================================

static void mc6845_init_gpio(void) {
    // Control pins
    gpio_init(PIN_MC6845_CS);
    gpio_init(PIN_MC6845_RS);
    gpio_init(PIN_MC6845_E);
    gpio_init(PIN_MC6845_RW);

    gpio_set_dir(PIN_MC6845_CS, GPIO_OUT);
    gpio_set_dir(PIN_MC6845_RS, GPIO_OUT);
    gpio_set_dir(PIN_MC6845_E, GPIO_OUT);
    gpio_set_dir(PIN_MC6845_RW, GPIO_OUT);

    gpio_put(PIN_MC6845_CS, 1); // deselect
    gpio_put(PIN_MC6845_E, 0);

    // Data bus (default input)
    for (int i = 0; i < DATA_WIDTH; i++) {
        gpio_init(PIN_DATA_BASE + i);
        gpio_set_dir(PIN_DATA_BASE + i, GPIO_IN);
        gpio_set_pulls(PIN_DATA_BASE + i, false, false);
    }
}

static void mc6845_init_chip(void) {
    printf("Initializing MC6845...\n");

    mc6845_init_gpio();
    init_clock_pio(pio0, SM_CLOCK, PIN_MC6845_CLK, MC6845_CLOCK_FREQ);

    for (int r = 0; r < 16; r++) {
        mc6845_write_register(r, mc6845_defaults[r]);
        sleep_us(10);
    }
    printf("Default register values for 80x24 text mode written.\n\n");
}

// Инициализация GPIO как входов для MA/RA
static void mc6845_init_address_inputs(void) {
    for (int i = 0; i < MA_WIDTH; i++) {
        gpio_init(PIN_MA_BASE + i);
        gpio_set_dir(PIN_MA_BASE + i, GPIO_IN);
        gpio_set_pulls(PIN_MA_BASE + i, false, false);
    }
    for (int i = 0; i < RA_WIDTH; i++) {
        gpio_init(PIN_RA_BASE + i);
        gpio_set_dir(PIN_RA_BASE + i, GPIO_IN);
        gpio_set_pulls(PIN_RA_BASE + i, false, false);
    }
}



[[noreturn]] int main() {
    // Configure RP2040 system clock
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    set_sys_clock_hz(SYSTEM_CLOCK_HZ, true);
    busy_wait_ms(25);

    stdio_usb_init();
    busy_wait_ms(1000);

    printf("\nMC6845 demo start!\n");

    mc6845_init_chip();
    mc6845_init_address_inputs();

    // Dump back register values
    for (int r = 0; r < 16; r++) {
        printf("R%02d = 0x%02X\n", r, mc6845_read_register(r));
        sleep_us(30);
    }

    // Light pen registers
    const uint8_t lp_hi = mc6845_read_register(16);
    const uint8_t lp_lo = mc6845_read_register(17);
    printf("Light-pen R16/R17 = 0x%02X 0x%02X\n", lp_hi, lp_lo);

    // Цикл наблюдения за MA/RA
    uint32_t previous_pins = 0xFFFFFFFF;

    while (1) {
        // Прямое чтение всех GPIO
        const uint32_t pins = gpio_get_all() & 0x1FFFF;

        if (pins != previous_pins) {
            previous_pins = pins;
            // MA = биты 0..13
            const uint16_t address = pins & 0x3FFF; // 14 бит

            // RA = биты 14..16
            const uint8_t row = pins >> 14;   // 3 бита

            printf("MA=%04X RA=%u\n", address, row);
        }
        tight_loop_contents();
    }
}
