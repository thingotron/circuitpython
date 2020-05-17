/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Scott Shawcroft for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>
#include <stdlib.h>

#include "boards/board.h"
#include "supervisor/port.h"

// ASF 4
#include "atmel_start_pins.h"
#include "hal/include/hal_delay.h"
#include "hal/include/hal_flash.h"
#include "hal/include/hal_gpio.h"
#include "hal/include/hal_init.h"
#include "hpl/gclk/hpl_gclk_base.h"
#include "hpl/pm/hpl_pm_base.h"

#ifdef SAMD21
#include "hri/hri_pm_d21.h"
#endif
#ifdef SAMD51
#include "hri/hri_rstc_d51.h"
#endif

#include "common-hal/analogio/AnalogIn.h"
#include "common-hal/analogio/AnalogOut.h"
#include "common-hal/audiobusio/PDMIn.h"
#include "common-hal/audiobusio/I2SOut.h"
#include "common-hal/audioio/AudioOut.h"
#include "common-hal/busio/SPI.h"
#include "common-hal/microcontroller/Pin.h"
#include "common-hal/pulseio/PulseIn.h"
#include "common-hal/pulseio/PulseOut.h"
#include "common-hal/pulseio/PWMOut.h"
#include "common-hal/ps2io/Ps2.h"
#include "common-hal/rtc/RTC.h"

#if CIRCUITPY_TOUCHIO_USE_NATIVE
#include "common-hal/touchio/TouchIn.h"
#endif

#include "samd/cache.h"
#include "samd/clocks.h"
#include "samd/events.h"
#include "samd/external_interrupts.h"
#include "samd/dma.h"
#include "shared-bindings/rtc/__init__.h"
#include "reset.h"
#include "tick.h"

#include "supervisor/shared/safe_mode.h"
#include "supervisor/shared/stack.h"

#include "tusb.h"

#if CIRCUITPY_GAMEPAD
#include "shared-module/gamepad/__init__.h"
#endif
#if CIRCUITPY_GAMEPADSHIFT
#include "shared-module/gamepadshift/__init__.h"
#endif
#include "shared-module/_pew/PewPew.h"

extern volatile bool mp_msc_enabled;

#if defined(SAMD21) && defined(ENABLE_MICRO_TRACE_BUFFER)
// Stores 2 ^ TRACE_BUFFER_MAGNITUDE_PACKETS packets.
// 7 -> 128 packets
#define TRACE_BUFFER_MAGNITUDE_PACKETS 7
// Size in uint32_t. Two per packet.
#define TRACE_BUFFER_SIZE (1 << (TRACE_BUFFER_MAGNITUDE_PACKETS + 1))
// Size in bytes. 4 bytes per uint32_t.
#define TRACE_BUFFER_SIZE_BYTES (TRACE_BUFFER_SIZE << 2)
__attribute__((__aligned__(TRACE_BUFFER_SIZE_BYTES))) uint32_t mtb[TRACE_BUFFER_SIZE] = {0};
#endif

#if CALIBRATE_CRYSTALLESS
static void save_usb_clock_calibration(void) {
    // If we are on USB lets double check our fine calibration for the clock and
    // save the new value if its different enough.
    SYSCTRL->DFLLSYNC.bit.READREQ = 1;
    uint16_t saved_calibration = 0x1ff;
    if (strcmp((char*) CIRCUITPY_INTERNAL_CONFIG_START_ADDR, "CIRCUITPYTHON1") == 0) {
        saved_calibration = ((uint16_t *) CIRCUITPY_INTERNAL_CONFIG_START_ADDR)[8];
    }
    while (SYSCTRL->PCLKSR.bit.DFLLRDY == 0) {
        // TODO(tannewt): Run the mass storage stuff if this takes a while.
    }
    int16_t current_calibration = SYSCTRL->DFLLVAL.bit.FINE;
    if (abs(current_calibration - saved_calibration) > 10) {
        // Copy the full internal config page to memory.
        uint8_t page_buffer[NVMCTRL_ROW_SIZE];
        memcpy(page_buffer, (uint8_t*) CIRCUITPY_INTERNAL_CONFIG_START_ADDR, NVMCTRL_ROW_SIZE);

        // Modify it.
        memcpy(page_buffer, "CIRCUITPYTHON1", 15);
        // First 16 bytes (0-15) are ID. Little endian!
        page_buffer[16] = current_calibration & 0xff;
        page_buffer[17] = current_calibration >> 8;

        // Write it back.
        // We don't use features that use any advanced NVMCTRL features so we can fake the descriptor
        // whenever we need it instead of storing it long term.
        struct flash_descriptor desc;
        desc.dev.hw = NVMCTRL;
        flash_write(&desc, (uint32_t) CIRCUITPY_INTERNAL_CONFIG_START_ADDR, page_buffer, NVMCTRL_ROW_SIZE);
    }
}
#endif

safe_mode_t port_init(void) {
#if defined(SAMD21)

    // Set brownout detection to ~2.7V. Default from factory is 1.7V,
    // which is too low for proper operation of external SPI flash chips (they are 2.7-3.6V).
    // Disable while changing level.
    SYSCTRL->BOD33.bit.ENABLE = 0;
    SYSCTRL->BOD33.bit.LEVEL = 39;  // 2.77V with hysteresis off. Table 37.20 in datasheet.
    SYSCTRL->BOD33.bit.ENABLE = 1;

    #ifdef ENABLE_MICRO_TRACE_BUFFER
        REG_MTB_POSITION = ((uint32_t) (mtb - REG_MTB_BASE)) & 0xFFFFFFF8;
        REG_MTB_FLOW = (((uint32_t) mtb - REG_MTB_BASE) + TRACE_BUFFER_SIZE_BYTES) & 0xFFFFFFF8;
        REG_MTB_MASTER = 0x80000000 + (TRACE_BUFFER_MAGNITUDE_PACKETS - 1);
    #else
        // Triple check that the MTB is off. Switching between debug and non-debug
        // builds can leave it set over reset and wreak havok as a result.
        REG_MTB_MASTER = 0x00000000 + 6;
    #endif
#endif

#if defined(SAMD51)
    // Set brownout detection to ~2.7V. Default from factory is 1.7V,
    // which is too low for proper operation of external SPI flash chips (they are 2.7-3.6V).
    // Disable while changing level.
    SUPC->BOD33.bit.ENABLE = 0;
    SUPC->BOD33.bit.LEVEL = 200;  // 2.7V: 1.5V + LEVEL * 6mV.
    SUPC->BOD33.bit.ENABLE = 1;

    // MPU (Memory Protection Unit) setup.
    // We hoped we could make the QSPI region be non-cachable with the MPU,
    // but the CMCC doesn't seem to pay attention to the MPU settings.
    // Leaving this code here disabled,
    // because it was hard enough to figure out, and maybe there's
    // a mistake that could make it work in the future.
#if 0
    // Designate QSPI memory mapped region as not cachable.

    // Turn off MPU in case it is on.
    MPU->CTRL = 0;
    // Configure region 0.
    MPU->RNR = 0;
    // Region base: start of QSPI mapping area.
    // QSPI region runs from 0x04000000 up to and not including 0x05000000: 16 megabytes
    MPU->RBAR = QSPI_AHB;
    MPU->RASR =
        0b011 << MPU_RASR_AP_Pos |     // full read/write access for privileged and user mode
        0b000 << MPU_RASR_TEX_Pos |    // caching not allowed, strongly ordered
        1 << MPU_RASR_S_Pos |          // sharable
        0 << MPU_RASR_C_Pos |          // not cachable
        0 << MPU_RASR_B_Pos |          // not bufferable
        0b10111 << MPU_RASR_SIZE_Pos | // 16MB region size
        1 << MPU_RASR_ENABLE_Pos       // enable this region
        ;
    // Turn off regions 1-7.
    for (uint32_t i = 1; i < 8; i ++) {
        MPU->RNR = i;
        MPU->RBAR = 0;
        MPU->RASR = 0;
    }

    // Turn on MPU. Turn on PRIVDEFENA, which defines a default memory
    // map for all privileged access, so we don't have to set up other regions
    // besides QSPI.
    MPU->CTRL = MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_ENABLE_Msk;
#endif

    samd_peripherals_enable_cache();
#endif

#ifdef SAMD21
    hri_nvmctrl_set_CTRLB_RWS_bf(NVMCTRL, 2);
    _pm_init();
#endif

#if CALIBRATE_CRYSTALLESS
    uint32_t fine = DEFAULT_DFLL48M_FINE_CALIBRATION;
    // The fine calibration data is stored in an NVM page after the text and data storage but before
    // the optional file system. The first 16 bytes are the identifier for the section.
    if (strcmp((char*) CIRCUITPY_INTERNAL_CONFIG_START_ADDR, "CIRCUITPYTHON1") == 0) {
        fine = ((uint16_t *) CIRCUITPY_INTERNAL_CONFIG_START_ADDR)[8];
    }
    clock_init(BOARD_HAS_CRYSTAL, fine);
#else
    // Use a default fine value
    clock_init(BOARD_HAS_CRYSTAL, DEFAULT_DFLL48M_FINE_CALIBRATION);
#endif

    // Configure millisecond timer initialization.
    tick_init();

#if CIRCUITPY_RTC
    rtc_init();
#endif

    init_shared_dma();

    // Reset everything into a known state before board_init.
    reset_port();

    #ifdef SAMD21
    if (PM->RCAUSE.bit.BOD33 == 1 || PM->RCAUSE.bit.BOD12 == 1) {
        return BROWNOUT;
    }
    #endif
    #ifdef SAMD51
    if (RSTC->RCAUSE.bit.BODVDD == 1 || RSTC->RCAUSE.bit.BODCORE == 1) {
        return BROWNOUT;
    }
    #endif

    if (board_requests_safe_mode()) {
        return USER_SAFE_MODE;
    }

    return NO_SAFE_MODE;
}

void reset_port(void) {
    reset_sercoms();

#if CIRCUITPY_AUDIOIO
    audio_dma_reset();
    audioout_reset();
#endif
#if CIRCUITPY_AUDIOBUSIO
    i2sout_reset();
    //pdmin_reset();
#endif

#if CIRCUITPY_TOUCHIO && CIRCUITPY_TOUCHIO_USE_NATIVE
    touchin_reset();
#endif
    eic_reset();
#if CIRCUITPY_PULSEIO
    pulseout_reset();
    pwmout_reset();
#endif

#if CIRCUITPY_ANALOGIO
    analogin_reset();
    analogout_reset();
#endif
#if CIRCUITPY_RTC
    rtc_reset();
#endif

    reset_gclks();

#if CIRCUITPY_GAMEPAD
    gamepad_reset();
#endif
#if CIRCUITPY_GAMEPADSHIFT
    gamepadshift_reset();
#endif
#if CIRCUITPY_PEW
    pew_reset();
#endif

    reset_event_system();

    reset_all_pins();

    // Output clocks for debugging.
    // not supported by SAMD51G; uncomment for SAMD51J or update for 51G
    // #ifdef SAMD51
    // gpio_set_pin_function(PIN_PA10, GPIO_PIN_FUNCTION_M); // GCLK4, D3
    // gpio_set_pin_function(PIN_PA11, GPIO_PIN_FUNCTION_M); // GCLK5, A4
    // gpio_set_pin_function(PIN_PB14, GPIO_PIN_FUNCTION_M); // GCLK0, D5
    // gpio_set_pin_function(PIN_PB15, GPIO_PIN_FUNCTION_M); // GCLK1, D6
    // #endif

#if CALIBRATE_CRYSTALLESS
    if (tud_cdc_connected()) {
        save_usb_clock_calibration();
    }
#endif
}

void reset_to_bootloader(void) {
    _bootloader_dbl_tap = DBL_TAP_MAGIC;
    reset();
}

void reset_cpu(void) {
    reset();
}

uint32_t *port_stack_get_limit(void) {
    return &_ebss;
}

uint32_t *port_stack_get_top(void) {
    return &_estack;
}

uint32_t *port_heap_get_bottom(void) {
    return port_stack_get_limit();
}

uint32_t *port_heap_get_top(void) {
    return port_stack_get_top();
}

// Place the word to save 8k from the end of RAM so we and the bootloader don't clobber it.
#ifdef SAMD21
uint32_t* safe_word = (uint32_t*) (HMCRAMC0_ADDR + HMCRAMC0_SIZE - 0x2000);
#endif
#ifdef SAMD51
uint32_t* safe_word = (uint32_t*) (HSRAM_ADDR + HSRAM_SIZE - 0x2000);
#endif

void port_set_saved_word(uint32_t value) {
    *safe_word = value;
}

uint32_t port_get_saved_word(void) {
    return *safe_word;
}

/**
 * \brief Default interrupt handler for unused IRQs.
 */
__attribute__((used)) void HardFault_Handler(void)
{
#ifdef ENABLE_MICRO_TRACE_BUFFER
    // Turn off the micro trace buffer so we don't fill it up in the infinite
    // loop below.
    REG_MTB_MASTER = 0x00000000 + 6;
#endif

    reset_into_safe_mode(HARD_CRASH);
    while (true) {
        asm("nop;");
    }
}
