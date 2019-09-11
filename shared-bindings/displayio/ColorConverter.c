/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Scott Shawcroft for Adafruit Industries
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

#include "shared-bindings/displayio/ColorConverter.h"

#include <stdint.h>

#include "lib/utils/context_manager_helpers.h"
#include "py/binary.h"
#include "py/objproperty.h"
#include "py/runtime.h"
#include "shared-bindings/microcontroller/Pin.h"
#include "shared-bindings/util.h"
#include "supervisor/shared/translate.h"

//| .. currentmodule:: displayio
//|
//| :class:`ColorConverter` -- Converts one color format to another
//| =========================================================================================
//|
//| Converts one color format to another.
//|
//| .. class:: ColorConverter(*, dither=False)
//|
//|   Create a ColorConverter object to convert color formats. Only supports RGB888 to RGB565
//|   currently.
//|   :param bool dither: Adds random noise to dither the output image

// TODO(tannewt): Add support for other color formats.
//|
STATIC mp_obj_t displayio_colorconverter_make_new(const mp_obj_type_t *type, size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_dither};

    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_dither, MP_ARG_KW_ONLY | MP_ARG_BOOL, {.u_bool = false} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    displayio_colorconverter_t *self = m_new_obj(displayio_colorconverter_t);
    self->base.type = &displayio_colorconverter_type;

    common_hal_displayio_colorconverter_construct(self, args[ARG_dither].u_bool);

    return MP_OBJ_FROM_PTR(self);
}

//|   .. method:: convert(color)
//|
//|   Converts the given RGB888 color to RGB565
//|
STATIC mp_obj_t displayio_colorconverter_obj_convert(mp_obj_t self_in, mp_obj_t color_obj) {
    displayio_colorconverter_t *self = MP_OBJ_TO_PTR(self_in);

    mp_int_t color;
    if (!mp_obj_get_int_maybe(color_obj, &color)) {
        mp_raise_ValueError(translate("color should be an int"));
    }
    _displayio_colorspace_t colorspace;
    colorspace.depth = 16;
    uint32_t output_color;
    common_hal_displayio_colorconverter_convert(self, &colorspace, color, &output_color);
    return MP_OBJ_NEW_SMALL_INT(output_color);
}
MP_DEFINE_CONST_FUN_OBJ_2(displayio_colorconverter_convert_obj, displayio_colorconverter_obj_convert);

//|   .. attribute:: dither
//|
//|     When true the color converter dithers the output by adding random noise when
//|     truncating to display bitdepth
//|
STATIC mp_obj_t displayio_colorconverter_obj_get_dither(mp_obj_t self_in) {
    displayio_colorconverter_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_bool(common_hal_displayio_colorconverter_get_dither(self));
}
MP_DEFINE_CONST_FUN_OBJ_1(displayio_colorconverter_get_dither_obj, displayio_colorconverter_obj_get_dither);

STATIC mp_obj_t displayio_colorconverter_obj_set_dither(mp_obj_t self_in, mp_obj_t dither) {
    displayio_colorconverter_t *self = MP_OBJ_TO_PTR(self_in);

    common_hal_displayio_colorconverter_set_dither(self, mp_obj_is_true(dither));

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(displayio_colorconverter_set_dither_obj, displayio_colorconverter_obj_set_dither);

const mp_obj_property_t displayio_colorconverter_dither_obj = {
    .base.type = &mp_type_property,
    .proxy = {(mp_obj_t)&displayio_colorconverter_get_dither_obj,
              (mp_obj_t)&displayio_colorconverter_set_dither_obj,
              (mp_obj_t)&mp_const_none_obj},
};

STATIC const mp_rom_map_elem_t displayio_colorconverter_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_convert), MP_ROM_PTR(&displayio_colorconverter_convert_obj) },
    { MP_ROM_QSTR(MP_QSTR_dither), MP_ROM_PTR(&displayio_colorconverter_dither_obj) },
};
STATIC MP_DEFINE_CONST_DICT(displayio_colorconverter_locals_dict, displayio_colorconverter_locals_dict_table);

const mp_obj_type_t displayio_colorconverter_type = {
    { &mp_type_type },
    .name = MP_QSTR_ColorConverter,
    .make_new = displayio_colorconverter_make_new,
    .locals_dict = (mp_obj_dict_t*)&displayio_colorconverter_locals_dict,
};

