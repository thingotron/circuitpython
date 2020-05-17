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

#include "common-hal/pulseio/PulseIn.h"

#include <stdint.h>
#include <string.h>

#include "py/mpconfig.h"
#include "py/gc.h"
#include "py/runtime.h"

#include "shared-bindings/microcontroller/__init__.h"
#include "shared-bindings/pulseio/PulseIn.h"

#include "tick.h"
#include "nrfx_gpiote.h"

// obj array to map pin -> self since nrfx hide the mapping
static pulseio_pulsein_obj_t* _objs[GPIOTE_CH_NUM];

// return index of the object in array
static int _find_pulsein_obj(pulseio_pulsein_obj_t* obj) {
    for(size_t i = 0; i < NRFX_ARRAY_SIZE(_objs); i++ ) {
        if ( _objs[i] == obj) {
            return i;
        }
    }

    return -1;
}

static void _pulsein_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
    // Grab the current time first.
    uint32_t current_us;
    uint64_t current_ms;
    current_tick(&current_ms, &current_us);

    // current_tick gives us the remaining us until the next tick but we want the number since the last ms.
    current_us = 1000 - current_us;

    pulseio_pulsein_obj_t* self = NULL;
    for(size_t i = 0; i < NRFX_ARRAY_SIZE(_objs); i++ ) {
        if ( _objs[i] && _objs[i]->pin == pin ) {
            self = _objs[i];
            break;
        }
    }
    if ( !self ) return;

    if (self->first_edge) {
        // first pulse is opposite state from idle
        bool state = nrf_gpio_pin_read(self->pin);
        if ( self->idle_state != state ) {
            self->first_edge = false;
        }
    }else {
        uint32_t ms_diff = current_ms - self->last_ms;
        uint16_t us_diff = current_us - self->last_us;
        uint32_t total_diff = us_diff;

        if (self->last_us > current_us) {
            total_diff = 1000 + current_us - self->last_us;
            if (ms_diff > 1) {
                total_diff += (ms_diff - 1) * 1000;
            }
        } else {
            total_diff += ms_diff * 1000;
        }
        uint16_t duration = 0xffff;
        if (total_diff < duration) {
            duration = total_diff;
        }

        uint16_t i = (self->start + self->len) % self->maxlen;
        self->buffer[i] = duration;
        if (self->len < self->maxlen) {
            self->len++;
        } else {
            self->start++;
        }
    }

    self->last_ms = current_ms;
    self->last_us = current_us;
}

void pulsein_reset(void) {
    if ( nrfx_gpiote_is_init() ) {
        nrfx_gpiote_uninit();
    }
    nrfx_gpiote_init(NRFX_GPIOTE_CONFIG_IRQ_PRIORITY);

    memset(_objs, 0, sizeof(_objs));
}

void common_hal_pulseio_pulsein_construct(pulseio_pulsein_obj_t* self, const mcu_pin_obj_t* pin, uint16_t maxlen, bool idle_state) {
    int idx = _find_pulsein_obj(NULL);
    if ( idx < 0 ) {
        mp_raise_NotImplementedError(NULL);
    }
    _objs[idx] = self;

    self->buffer = (uint16_t *) m_malloc(maxlen * sizeof(uint16_t), false);
    if (self->buffer == NULL) {
        mp_raise_msg_varg(&mp_type_MemoryError, translate("Failed to allocate RX buffer of %d bytes"), maxlen * sizeof(uint16_t));
    }

    self->pin = pin->number;
    self->maxlen = maxlen;
    self->idle_state = idle_state;
    self->start = 0;
    self->len = 0;
    self->first_edge = true;
    self->paused = false;
    self->last_us = 0;
    self->last_ms = 0;

    claim_pin(pin);

    nrfx_gpiote_in_config_t cfg = {
        .sense = NRF_GPIOTE_POLARITY_TOGGLE,
        .pull = NRF_GPIO_PIN_NOPULL, // idle_state ? NRF_GPIO_PIN_PULLDOWN : NRF_GPIO_PIN_PULLUP,
        .is_watcher = false, // nrf_gpio_cfg_watcher vs nrf_gpio_cfg_input
        .hi_accuracy = true,
        .skip_gpio_setup = false
    };
    nrfx_gpiote_in_init(self->pin, &cfg, _pulsein_handler);
    nrfx_gpiote_in_event_enable(self->pin, true);
}

bool common_hal_pulseio_pulsein_deinited(pulseio_pulsein_obj_t* self) {
  return self->pin == NO_PIN;
}

void common_hal_pulseio_pulsein_deinit(pulseio_pulsein_obj_t* self) {
    if (common_hal_pulseio_pulsein_deinited(self)) {
        return;
    }

    nrfx_gpiote_in_event_disable(self->pin);
    nrfx_gpiote_in_uninit(self->pin);

    // mark local array as invalid
    int idx = _find_pulsein_obj(self);
    if ( idx < 0 ) {
        mp_raise_NotImplementedError(NULL);
    }
    _objs[idx] = NULL;

    reset_pin_number(self->pin);
    self->pin = NO_PIN;
}

void common_hal_pulseio_pulsein_pause(pulseio_pulsein_obj_t* self) {
    nrfx_gpiote_in_event_disable(self->pin);
    self->paused = true;
}

void common_hal_pulseio_pulsein_resume(pulseio_pulsein_obj_t* self, uint16_t trigger_duration) {
    // Make sure we're paused.
    if ( !self->paused ) {
        common_hal_pulseio_pulsein_pause(self);
    }

    // Send the trigger pulse.
    if (trigger_duration > 0) {
        nrfx_gpiote_in_uninit(self->pin);

        nrf_gpio_cfg_output(self->pin);
        nrf_gpio_pin_write(self->pin, !self->idle_state);
        common_hal_mcu_delay_us((uint32_t)trigger_duration);
        nrf_gpio_pin_write(self->pin, self->idle_state);

        nrfx_gpiote_in_config_t cfg = {
            .sense = NRF_GPIOTE_POLARITY_TOGGLE,
            .pull = NRF_GPIO_PIN_NOPULL, // idle_state ? NRF_GPIO_PIN_PULLDOWN : NRF_GPIO_PIN_PULLUP,
            .is_watcher = false, // nrf_gpio_cfg_watcher vs nrf_gpio_cfg_input
            .hi_accuracy = true,
            .skip_gpio_setup = false
        };
        nrfx_gpiote_in_init(self->pin, &cfg, _pulsein_handler);
    }

    self->first_edge = true;
    self->paused = false;
    self->last_ms = 0;
    self->last_us = 0;

    nrfx_gpiote_in_event_enable(self->pin, true);
}

void common_hal_pulseio_pulsein_clear(pulseio_pulsein_obj_t* self) {
    if ( !self->paused ) {
        nrfx_gpiote_in_event_disable(self->pin);
    }

    self->start = 0;
    self->len = 0;

    if ( !self->paused ) {
        nrfx_gpiote_in_event_enable(self->pin, true);
    }
}

uint16_t common_hal_pulseio_pulsein_get_item(pulseio_pulsein_obj_t* self, int16_t index) {
    if ( !self->paused ) {
        nrfx_gpiote_in_event_disable(self->pin);
    }

    if (index < 0) {
        index += self->len;
    }
    if (index < 0 || index >= self->len) {
        if ( !self->paused ) {
            nrfx_gpiote_in_event_enable(self->pin, true);
        }
        mp_raise_IndexError(translate("index out of range"));
    }
    uint16_t value = self->buffer[(self->start + index) % self->maxlen];

    if ( !self->paused ) {
        nrfx_gpiote_in_event_enable(self->pin, true);
    }

    return value;
}

uint16_t common_hal_pulseio_pulsein_popleft(pulseio_pulsein_obj_t* self) {
    if (self->len == 0) {
        mp_raise_IndexError(translate("pop from an empty PulseIn"));
    }

    if ( !self->paused ) {
        nrfx_gpiote_in_event_disable(self->pin);
    }

    uint16_t value = self->buffer[self->start];
    self->start = (self->start + 1) % self->maxlen;
    self->len--;

    if ( !self->paused ) {
        nrfx_gpiote_in_event_enable(self->pin, true);
    }

    return value;
}

uint16_t common_hal_pulseio_pulsein_get_maxlen(pulseio_pulsein_obj_t* self) {
    return self->maxlen;
}

bool common_hal_pulseio_pulsein_get_paused(pulseio_pulsein_obj_t* self) {
    return self->paused;
}

uint16_t common_hal_pulseio_pulsein_get_len(pulseio_pulsein_obj_t* self) {
    return self->len;
}
