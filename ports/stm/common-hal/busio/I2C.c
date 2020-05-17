/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Scott Shawcroft
 * Copyright (c) 2019 Lucian Copeland for Adafruit Industries
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
#include <stdbool.h>

#include "shared-bindings/busio/I2C.h"
#include "py/mperrno.h"
#include "py/runtime.h"
#include "stm32f4xx_hal.h"

#include "shared-bindings/microcontroller/__init__.h"
#include "supervisor/shared/translate.h"
#include "common-hal/microcontroller/Pin.h"

//arrays use 0 based numbering: I2C1 is stored at index 0
#define MAX_I2C 3
STATIC bool reserved_i2c[MAX_I2C];
STATIC bool never_reset_i2c[MAX_I2C];

#define ALL_CLOCKS 0xFF
STATIC void i2c_clock_enable(uint8_t mask);
STATIC void i2c_clock_disable(uint8_t mask);

void i2c_reset(void) {
    uint16_t never_reset_mask = 0x00;
    for(int i = 0; i < MAX_I2C; i++) {
        if (!never_reset_i2c[i]) {
            reserved_i2c[i] = false;
        } else {
            never_reset_mask |= 1 << i;
        }
    }
    i2c_clock_disable(ALL_CLOCKS & ~(never_reset_mask));
}

void common_hal_busio_i2c_construct(busio_i2c_obj_t *self,
        const mcu_pin_obj_t* scl, const mcu_pin_obj_t* sda, uint32_t frequency, uint32_t timeout) {

    //match pins to I2C objects
    I2C_TypeDef * I2Cx;
    uint8_t sda_len = MP_ARRAY_SIZE(mcu_i2c_sda_list);
    uint8_t scl_len = MP_ARRAY_SIZE(mcu_i2c_scl_list);
    bool i2c_taken = false;

    for (uint i = 0; i < sda_len; i++) {
        if (mcu_i2c_sda_list[i].pin == sda) {
            for (uint j = 0; j < scl_len; j++) {
                if ((mcu_i2c_scl_list[j].pin == scl)
                    && (mcu_i2c_scl_list[j].i2c_index == mcu_i2c_sda_list[i].i2c_index)) {
                    //keep looking if the I2C is taken, could be another SCL that works
                    if (reserved_i2c[mcu_i2c_scl_list[i].i2c_index - 1]) {
                        i2c_taken = true;
                        continue;
                    }
                    self->scl = &mcu_i2c_scl_list[j];
                    self->sda = &mcu_i2c_sda_list[i];
                    break;
                }
            }
        }
    }

    //handle typedef selection, errors
    if (self->sda != NULL && self->scl != NULL ) {
        I2Cx = mcu_i2c_banks[self->sda->i2c_index - 1];
    } else {
        if (i2c_taken) {
            mp_raise_ValueError(translate("Hardware busy, try alternative pins"));
        } else {
            mp_raise_ValueError(translate("Invalid I2C pin selection"));
        }
    }

    //Start GPIO for each pin
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = pin_mask(sda->number);
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = self->sda->altfn_index;
    HAL_GPIO_Init(pin_port(sda->port), &GPIO_InitStruct);

    GPIO_InitStruct.Pin = pin_mask(scl->number);
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = self->scl->altfn_index;
    HAL_GPIO_Init(pin_port(scl->port), &GPIO_InitStruct);

    //Note: due to I2C soft reboot issue, do not relocate clock init.
    i2c_clock_enable(1 << (self->sda->i2c_index - 1));
    reserved_i2c[self->sda->i2c_index - 1] = true;

    self->handle.Instance = I2Cx;
    self->handle.Init.ClockSpeed = 100000;
    self->handle.Init.DutyCycle = I2C_DUTYCYCLE_2;
    self->handle.Init.OwnAddress1 = 0;
    self->handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    self->handle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    self->handle.Init.OwnAddress2 = 0;
    self->handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    self->handle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    self->handle.State = HAL_I2C_STATE_RESET;
    if (HAL_I2C_Init(&(self->handle)) != HAL_OK) {
        mp_raise_RuntimeError(translate("I2C Init Error"));
    }
    claim_pin(sda);
    claim_pin(scl);
}

void common_hal_busio_i2c_never_reset(busio_i2c_obj_t *self) {
    for (size_t i = 0; i < MP_ARRAY_SIZE(mcu_i2c_banks); i++) {
        if (self->handle.Instance == mcu_i2c_banks[i]) {
            never_reset_i2c[i] = true;

            never_reset_pin_number(self->scl->pin->port, self->scl->pin->number);
            never_reset_pin_number(self->sda->pin->port, self->sda->pin->number);
            break;
        }
    }
}

bool common_hal_busio_i2c_deinited(busio_i2c_obj_t *self) {
    return self->sda == NULL;
}

void common_hal_busio_i2c_deinit(busio_i2c_obj_t *self) {
    if (common_hal_busio_i2c_deinited(self)) {
        return;
    }

    i2c_clock_disable(1 << (self->sda->i2c_index - 1));
    reserved_i2c[self->sda->i2c_index - 1] = false;
    never_reset_i2c[self->sda->i2c_index - 1] = false;

    reset_pin_number(self->sda->pin->port,self->sda->pin->number);
    reset_pin_number(self->scl->pin->port,self->scl->pin->number);
    self->sda = NULL;
    self->scl = NULL;
}

bool common_hal_busio_i2c_probe(busio_i2c_obj_t *self, uint8_t addr) {
    return HAL_I2C_IsDeviceReady(&(self->handle), (uint16_t)(addr << 1), 2, 2) == HAL_OK;
}

bool common_hal_busio_i2c_try_lock(busio_i2c_obj_t *self) {
    bool grabbed_lock = false;

    //Critical section code that may be required at some point.
    // uint32_t store_primask = __get_PRIMASK();
    // __disable_irq();
    // __DMB();

        if (!self->has_lock) {
            grabbed_lock = true;
            self->has_lock = true;
        }

    // __DMB();
    // __set_PRIMASK(store_primask);

    return grabbed_lock;
}

bool common_hal_busio_i2c_has_lock(busio_i2c_obj_t *self) {
    return self->has_lock;
}

void common_hal_busio_i2c_unlock(busio_i2c_obj_t *self) {
    self->has_lock = false;
}

uint8_t common_hal_busio_i2c_write(busio_i2c_obj_t *self, uint16_t addr,
                                   const uint8_t *data, size_t len, bool transmit_stop_bit) {
    HAL_StatusTypeDef result = HAL_I2C_Master_Transmit(&(self->handle), (uint16_t)(addr << 1),
                                                        (uint8_t *)data, (uint16_t)len, 500);
    return result == HAL_OK ? 0 : MP_EIO;
}

uint8_t common_hal_busio_i2c_read(busio_i2c_obj_t *self, uint16_t addr,
        uint8_t *data, size_t len) {
    return HAL_I2C_Master_Receive(&(self->handle), (uint16_t)(addr<<1), data, (uint16_t)len, 500)
                                    == HAL_OK ? 0 : MP_EIO;
}

STATIC void i2c_clock_enable(uint8_t mask) {
    //Note: hard reset required due to soft reboot issue.
    #ifdef I2C1
    if (mask & (1 << 0)) {
        __HAL_RCC_I2C1_CLK_ENABLE();
        __HAL_RCC_I2C1_FORCE_RESET();
        __HAL_RCC_I2C1_RELEASE_RESET();
    }
    #endif
    #ifdef I2C2
    if (mask & (1 << 1)) {
        __HAL_RCC_I2C2_CLK_ENABLE();
        __HAL_RCC_I2C2_FORCE_RESET();
        __HAL_RCC_I2C2_RELEASE_RESET();
    }
    #endif
    #ifdef I2C3
    if (mask & (1 << 2)) {
        __HAL_RCC_I2C3_CLK_ENABLE();
        __HAL_RCC_I2C3_FORCE_RESET();
        __HAL_RCC_I2C3_RELEASE_RESET();
    }
    #endif
}

STATIC void i2c_clock_disable(uint8_t mask) {
    #ifdef I2C1
    if (mask & (1 << 0)) {
        __HAL_RCC_I2C1_CLK_DISABLE();
    }
    #endif
    #ifdef I2C2
    if (mask & (1 << 1)) {
        __HAL_RCC_I2C2_CLK_DISABLE();
    }
    #endif
    #ifdef I2C3
    if (mask & (1 << 2)) {
        __HAL_RCC_I2C3_CLK_DISABLE();
    }
    #endif
}
