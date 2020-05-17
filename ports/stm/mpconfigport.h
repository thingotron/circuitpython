/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Glenn Ruben Bakke
 * Copyright (c) 2019 Dan Halbert for Adafruit Industries
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

#ifndef STM32_MPCONFIGPORT_H__
#define STM32_MPCONFIGPORT_H__

#include <stdint.h>

#define MICROPY_PY_COLLECTIONS_ORDEREDDICT       (1)
#define MICROPY_PY_FUNCTION_ATTRS                (1)
#define MICROPY_PY_IO                            (1)
#define MICROPY_PY_REVERSE_SPECIAL_METHODS       (1)
#define MICROPY_PY_UJSON                         (1)

extern uint8_t _ld_default_stack_size;

// 24kiB stack
// #define CIRCUITPY_DEFAULT_STACK_SIZE            0x6000
#define CIRCUITPY_DEFAULT_STACK_SIZE            ((uint32_t) &_ld_default_stack_size)

#include "py/circuitpy_mpconfig.h"

// The STM32 HAL file is included virtually everywhere:
#include STM32_HAL_H

// These prevent you from accidentally omitting a python file that links mpconfigport
// and having a file accept a lack of chip family as an option. 
#if defined(STM32F4)
#define CPY_STM32F4 1
#define CPY_STM32F7 0
#define CPY_STM32H7 0
#elif defined(STM32F7)
#define CPY_STM32F4 0
#define CPY_STM32F7 1
#define CPY_STM32H7 0
#elif defined(STM32H7)
#define CPY_STM32F4 0
#define CPY_STM32F7 0
#define CPY_STM32H7 1
#else 
#error undefined processor
#endif

// Board flags:
#ifndef BOARD_OVERWRITE_SWD
#define BOARD_OVERWRITE_SWD (0)
#endif
#ifndef BOARD_VTOR_DEFER
#define BOARD_VTOR_DEFER (0)
#endif
#ifndef BOARD_NO_VBUS_SENSE
#define BOARD_NO_VBUS_SENSE (0)
#endif

#define MAX_UART 10 //how many UART are implemented

#define MICROPY_PORT_ROOT_POINTERS \
    void *cpy_uart_obj_all[MAX_UART]; \
	CIRCUITPY_COMMON_ROOT_POINTERS

#endif  // __INCLUDED_MPCONFIGPORT_H
