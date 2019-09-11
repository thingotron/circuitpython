/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Scott Shawcroft for Adafruit Industries
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

#include "shared-bindings/displayio/ParallelBus.h"

#include <stdint.h>

#include "lib/utils/context_manager_helpers.h"
#include "py/binary.h"
#include "py/objproperty.h"
#include "py/runtime.h"
#include "shared-bindings/microcontroller/Pin.h"
#include "shared-bindings/util.h"
#include "shared-module/displayio/__init__.h"
#include "supervisor/shared/translate.h"

//| .. currentmodule:: displayio
//|
//| :class:`ParallelBus` -- Manage updating a display over 8-bit parallel bus
//| ==============================================================================
//|
//| Manage updating a display over 8-bit parallel bus in the background while Python code runs. This
//| protocol may be refered to as 8080-I Series Parallel Interface in datasheets. It doesn't handle
//| display initialization.
//|
//| .. class:: ParallelBus(*, data0, command, chip_select, write, read, reset)
//|
//|   Create a ParallelBus object associated with the given pins. The bus is inferred from data0
//|   by implying the next 7 additional pins on a given GPIO port.
//|
//|   The parallel bus and pins are then in use by the display until `displayio.release_displays()`
//|   is called even after a reload. (It does this so CircuitPython can use the display after your
//|   code is done.) So, the first time you initialize a display bus in code.py you should call
//|   :py:func`displayio.release_displays` first, otherwise it will error after the first code.py run.
//|
//|   :param microcontroller.Pin data0: The first data pin. The rest are implied
//|   :param microcontroller.Pin command: Data or command pin
//|   :param microcontroller.Pin chip_select: Chip select pin
//|   :param microcontroller.Pin write: Write pin
//|   :param microcontroller.Pin read: Read pin
//|   :param microcontroller.Pin reset: Reset pin
//|
STATIC mp_obj_t displayio_parallelbus_make_new(const mp_obj_type_t *type, size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_data0, ARG_command, ARG_chip_select, ARG_write, ARG_read, ARG_reset };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_data0, MP_ARG_OBJ | MP_ARG_KW_ONLY | MP_ARG_REQUIRED },
        { MP_QSTR_command, MP_ARG_OBJ | MP_ARG_KW_ONLY | MP_ARG_REQUIRED },
        { MP_QSTR_chip_select, MP_ARG_OBJ | MP_ARG_KW_ONLY | MP_ARG_REQUIRED },
        { MP_QSTR_write, MP_ARG_OBJ | MP_ARG_KW_ONLY | MP_ARG_REQUIRED },
        { MP_QSTR_read, MP_ARG_OBJ | MP_ARG_KW_ONLY | MP_ARG_REQUIRED },
        { MP_QSTR_reset, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_obj = mp_const_none} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_obj_t data0 = args[ARG_data0].u_obj;
    mp_obj_t command = args[ARG_command].u_obj;
    mp_obj_t chip_select = args[ARG_chip_select].u_obj;
    mp_obj_t write = args[ARG_write].u_obj;
    mp_obj_t read = args[ARG_read].u_obj;
    mp_obj_t reset = args[ARG_reset].u_obj;
    assert_pin_free(data0);
    assert_pin_free(command);
    assert_pin_free(chip_select);
    assert_pin_free(write);
    assert_pin_free(read);
    assert_pin_free(reset);

    displayio_parallelbus_obj_t* self = NULL;
    for (uint8_t i = 0; i < CIRCUITPY_DISPLAY_LIMIT; i++) {
        if (displays[i].parallel_bus.base.type== NULL ||
            displays[i].parallel_bus.base.type == &mp_type_NoneType) {
            self = &displays[i].parallel_bus;
            self->base.type = &displayio_parallelbus_type;
            break;
        }
    }
    if (self == NULL) {
        mp_raise_RuntimeError(translate("Too many display busses"));
    }

    common_hal_displayio_parallelbus_construct(self, data0, command, chip_select, write, read, reset);
    return self;
}

//|   .. method:: reset()
//|
//|     Performs a hardware reset via the reset pin. Raises an exception if called when no reset pin
//|     is available.
//|
STATIC mp_obj_t displayio_parallelbus_obj_reset(mp_obj_t self_in) {
    displayio_parallelbus_obj_t *self = self_in;

    if (!common_hal_displayio_parallelbus_reset(self)) {
        mp_raise_RuntimeError(translate("no reset pin available"));
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(displayio_parallelbus_reset_obj, displayio_parallelbus_obj_reset);

//|   .. method:: send(command, data)
//|
//|     Sends the given command value followed by the full set of data. Display state, such as
//|     vertical scroll, set via ``send`` may or may not be reset once the code is done.
//|
STATIC mp_obj_t displayio_parallelbus_obj_send(mp_obj_t self, mp_obj_t command_obj, mp_obj_t data_obj) {
    mp_int_t command_int = MP_OBJ_SMALL_INT_VALUE(command_obj);
    if (!MP_OBJ_IS_SMALL_INT(command_obj) || command_int > 255 || command_int < 0) {
        mp_raise_ValueError(translate("Command must be an int between 0 and 255"));
    }
    uint8_t command = command_int;
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data_obj, &bufinfo, MP_BUFFER_READ);

    // Wait for display bus to be available.
    while (!common_hal_displayio_parallelbus_begin_transaction(self)) {
        RUN_BACKGROUND_TASKS;
    }
    common_hal_displayio_parallelbus_send(self, DISPLAY_COMMAND, CHIP_SELECT_UNTOUCHED, &command, 1);
    common_hal_displayio_parallelbus_send(self, DISPLAY_DATA, CHIP_SELECT_UNTOUCHED, ((uint8_t*) bufinfo.buf), bufinfo.len);
    common_hal_displayio_parallelbus_end_transaction(self);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_3(displayio_parallelbus_send_obj, displayio_parallelbus_obj_send);

STATIC const mp_rom_map_elem_t displayio_parallelbus_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&displayio_parallelbus_reset_obj) },
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&displayio_parallelbus_send_obj) },
};
STATIC MP_DEFINE_CONST_DICT(displayio_parallelbus_locals_dict, displayio_parallelbus_locals_dict_table);

const mp_obj_type_t displayio_parallelbus_type = {
    { &mp_type_type },
    .name = MP_QSTR_ParallelBus,
    .make_new = displayio_parallelbus_make_new,
    .locals_dict = (mp_obj_dict_t*)&displayio_parallelbus_locals_dict,
};
