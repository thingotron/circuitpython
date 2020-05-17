/*
 * This file is part of the Circuit Python project, https://github.com/adafruit/circuitpython
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Roy Hooper
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

#include "py/obj.h"
#include "py/objarray.h"
#include "py/objtype.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include "py/binary.h"
#include "py/objproperty.h"
#include "py/runtime.h"
#include "py/gc.h"

#include <string.h>

#include "shared-bindings/_pixelbuf/PixelBuf.h"
#include "shared-module/_pixelbuf/PixelBuf.h"
#include "shared-bindings/digitalio/DigitalInOut.h"

extern const int32_t colorwheel(float pos);

static void parse_byteorder(mp_obj_t byteorder_obj, pixelbuf_byteorder_details_t* parsed);

//| .. currentmodule:: pixelbuf
//|
//| :class:`PixelBuf` -- A fast RGB[W] pixel buffer for LED and similar devices
//| ===========================================================================
//|
//| :class:`~_pixelbuf.PixelBuf` implements an RGB[W] bytearray abstraction.
//|
//| .. class:: PixelBuf(size, *, byteorder="BGR", brightness=0, auto_write=False, header=b"", trailer=b"")
//|
//|   Create a PixelBuf object of the specified size, byteorder, and bits per pixel.
//|
//|   When brightness is less than 1.0, a second buffer will be used to store the color values
//|   before they are adjusted for brightness.
//|
//|   When ``P`` (pwm duration) is present as the 4th character of the byteorder
//|   string, the 4th value in the tuple/list for a pixel is the individual pixel
//|   brightness (0.0-1.0) and will enable a Dotstar compatible 1st byte in the
//|   output buffer (``buf``).
//|
//|   :param ~int size: Number of pixelsx
//|   :param ~str byteorder: Byte order string (such as "BGR" or "PBGR")
//|   :param ~float brightness: Brightness (0 to 1.0, default 1.0)
//|   :param ~bool auto_write: Whether to automatically write pixels (Default False)
//|   :param bytes header: Sequence of bytes to always send before pixel values.
//|   :param bytes trailer: Sequence of bytes to always send after pixel values.
//|
STATIC mp_obj_t pixelbuf_pixelbuf_make_new(const mp_obj_type_t *type, size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    mp_arg_check_num(n_args, kw_args, 1, MP_OBJ_FUN_ARGS_MAX, true);
    enum { ARG_size, ARG_byteorder, ARG_brightness, ARG_auto_write, ARG_header, ARG_trailer };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_size, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_byteorder, MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NEW_QSTR(MP_QSTR_BGR) } },
        { MP_QSTR_brightness, MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_auto_write, MP_ARG_KW_ONLY | MP_ARG_BOOL, {.u_bool = false} },
        { MP_QSTR_header, MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_trailer, MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = mp_const_none } },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    pixelbuf_byteorder_details_t byteorder_details;

    parse_byteorder(args[ARG_byteorder].u_obj, &byteorder_details);

    mp_buffer_info_t header_bufinfo;
    mp_buffer_info_t trailer_bufinfo;

    if (!mp_get_buffer(args[ARG_header].u_obj, &header_bufinfo, MP_BUFFER_READ)) {
        header_bufinfo.buf = NULL;
        header_bufinfo.len = 0;
    }
    if (!mp_get_buffer(args[ARG_trailer].u_obj, &trailer_bufinfo, MP_BUFFER_READ)) {
        trailer_bufinfo.buf = NULL;
        trailer_bufinfo.len = 0;
    }

    float brightness = 1.0;
    if (args[ARG_brightness].u_obj != mp_const_none) {
        brightness = mp_obj_get_float(args[ARG_brightness].u_obj);
        if (brightness < 0) {
            brightness = 0;
        } else if (brightness > 1) {
            brightness = 1;
        }
    }

    // Validation complete, allocate and populate object.
    pixelbuf_pixelbuf_obj_t *self = m_new_obj(pixelbuf_pixelbuf_obj_t);
    self->base.type = &pixelbuf_pixelbuf_type;
    common_hal__pixelbuf_pixelbuf_construct(self, args[ARG_size].u_int,
    &byteorder_details, brightness, args[ARG_auto_write].u_bool, header_bufinfo.buf,
    header_bufinfo.len, trailer_bufinfo.buf, trailer_bufinfo.len);

    return MP_OBJ_FROM_PTR(self);
}

static void parse_byteorder(mp_obj_t byteorder_obj, pixelbuf_byteorder_details_t* parsed) {
    if (!MP_OBJ_IS_STR(byteorder_obj)) {
        mp_raise_TypeError(translate("byteorder is not a string"));
    }

    size_t bo_len;
    const char *byteorder = mp_obj_str_get_data(byteorder_obj, &bo_len);
    if (bo_len < 3 || bo_len > 4) {
        mp_raise_ValueError(translate("Invalid byteorder string"));
    }
    parsed->order_string = byteorder_obj;

    parsed->bpp = bo_len;
    char *dotstar = strchr(byteorder, 'P');
    char *r = strchr(byteorder, 'R');
    char *g = strchr(byteorder, 'G');
    char *b = strchr(byteorder, 'B');
    char *w = strchr(byteorder, 'W');
    int num_chars = (dotstar ? 1 : 0) + (w ? 1 : 0) + (r ? 1 : 0) + (g ? 1 : 0) + (b ? 1 : 0);
    if ((num_chars < parsed->bpp) || !(r && b && g)) {
        mp_raise_ValueError(translate("Invalid byteorder string"));
    }
    parsed->is_dotstar = dotstar ? true : false;
    parsed->has_white = w ? true : false;
    parsed->byteorder.r = r - byteorder;
    parsed->byteorder.g = g - byteorder;
    parsed->byteorder.b = b - byteorder;
    parsed->byteorder.w = w ? w - byteorder : 0;
    // The dotstar brightness byte is always first (as it goes with the pixel start bits)
    if (dotstar && byteorder[0] != 'P') {
        mp_raise_ValueError(translate("Invalid byteorder string"));
    }
    if (parsed->has_white && parsed->is_dotstar) {
        mp_raise_ValueError(translate("Invalid byteorder string"));
    }
}

//|   .. attribute:: bpp
//|
//|     The number of bytes per pixel in the buffer (read-only)
//|
STATIC mp_obj_t pixelbuf_pixelbuf_obj_get_bpp(mp_obj_t self_in) {
    return MP_OBJ_NEW_SMALL_INT(common_hal__pixelbuf_pixelbuf_get_bpp(self_in));
}
MP_DEFINE_CONST_FUN_OBJ_1(pixelbuf_pixelbuf_get_bpp_obj, pixelbuf_pixelbuf_obj_get_bpp);

const mp_obj_property_t pixelbuf_pixelbuf_bpp_obj = {
    .base.type = &mp_type_property,
    .proxy = {(mp_obj_t)&pixelbuf_pixelbuf_get_bpp_obj,
              (mp_obj_t)&mp_const_none_obj,
              (mp_obj_t)&mp_const_none_obj},
};


//|   .. attribute:: brightness
//|
//|     Float value between 0 and 1.  Output brightness.
//|
//|     When brightness is less than 1.0, a second buffer will be used to store the color values
//|     before they are adjusted for brightness.
//|
STATIC mp_obj_t pixelbuf_pixelbuf_obj_get_brightness(mp_obj_t self_in) {
    return mp_obj_new_float(common_hal__pixelbuf_pixelbuf_get_brightness(self_in));
}
MP_DEFINE_CONST_FUN_OBJ_1(pixelbuf_pixelbuf_get_brightness_obj, pixelbuf_pixelbuf_obj_get_brightness);


STATIC mp_obj_t pixelbuf_pixelbuf_obj_set_brightness(mp_obj_t self_in, mp_obj_t value) {
    mp_float_t brightness = mp_obj_float_get(value);
    if (brightness > 1) {
        brightness = 1;
    } else if (brightness < 0) {
        brightness = 0;
    }
    common_hal__pixelbuf_pixelbuf_set_brightness(self_in, brightness);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(pixelbuf_pixelbuf_set_brightness_obj, pixelbuf_pixelbuf_obj_set_brightness);

const mp_obj_property_t pixelbuf_pixelbuf_brightness_obj = {
    .base.type = &mp_type_property,
    .proxy = {(mp_obj_t)&pixelbuf_pixelbuf_get_brightness_obj,
              (mp_obj_t)&pixelbuf_pixelbuf_set_brightness_obj,
              (mp_obj_t)&mp_const_none_obj},
};

//|   .. attribute:: auto_write
//|
//|     Whether to automatically write the pixels after each update.
//|
STATIC mp_obj_t pixelbuf_pixelbuf_obj_get_auto_write(mp_obj_t self_in) {
    return mp_obj_new_bool(common_hal__pixelbuf_pixelbuf_get_auto_write(self_in));
}
MP_DEFINE_CONST_FUN_OBJ_1(pixelbuf_pixelbuf_get_auto_write_obj, pixelbuf_pixelbuf_obj_get_auto_write);


STATIC mp_obj_t pixelbuf_pixelbuf_obj_set_auto_write(mp_obj_t self_in, mp_obj_t value) {
    common_hal__pixelbuf_pixelbuf_set_auto_write(self_in, mp_obj_is_true(value));
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(pixelbuf_pixelbuf_set_auto_write_obj, pixelbuf_pixelbuf_obj_set_auto_write);

const mp_obj_property_t pixelbuf_pixelbuf_auto_write_obj = {
    .base.type = &mp_type_property,
    .proxy = {(mp_obj_t)&pixelbuf_pixelbuf_get_auto_write_obj,
              (mp_obj_t)&pixelbuf_pixelbuf_set_auto_write_obj,
              (mp_obj_t)&mp_const_none_obj},
};

//|   .. attribute:: byteorder
//|
//|     byteorder string for the buffer (read-only)
//|
STATIC mp_obj_t pixelbuf_pixelbuf_obj_get_byteorder(mp_obj_t self_in) {
    return common_hal__pixelbuf_pixelbuf_get_byteorder_string(self_in);
}
MP_DEFINE_CONST_FUN_OBJ_1(pixelbuf_pixelbuf_get_byteorder_str, pixelbuf_pixelbuf_obj_get_byteorder);

const mp_obj_property_t pixelbuf_pixelbuf_byteorder_str = {
    .base.type = &mp_type_property,
    .proxy = {(mp_obj_t)&pixelbuf_pixelbuf_get_byteorder_str,
              (mp_obj_t)&mp_const_none_obj,
              (mp_obj_t)&mp_const_none_obj},
};

STATIC mp_obj_t pixelbuf_pixelbuf_unary_op(mp_unary_op_t op, mp_obj_t self_in) {
    switch (op) {
        case MP_UNARY_OP_BOOL: return mp_const_true;
        case MP_UNARY_OP_LEN:
            return MP_OBJ_NEW_SMALL_INT(common_hal__pixelbuf_pixelbuf_get_len(self_in));
        default: return MP_OBJ_NULL; // op not supported
    }
}

//|   .. method:: show()
//|
//|     Transmits the color data to the pixels so that they are shown. This is done automatically
//|     when `auto_write` is True.
//|

STATIC mp_obj_t pixelbuf_pixelbuf_show(mp_obj_t self_in) {
    common_hal__pixelbuf_pixelbuf_show(self_in);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(pixelbuf_pixelbuf_show_obj, pixelbuf_pixelbuf_show);

//| .. function:: fill(color)
//|
//|   Fills the given pixelbuf with the given color.
//|

STATIC mp_obj_t pixelbuf_pixelbuf_fill(mp_obj_t self_in, mp_obj_t value) {
    common_hal__pixelbuf_pixelbuf_fill(self_in, value);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(pixelbuf_pixelbuf_fill_obj, pixelbuf_pixelbuf_fill);


//|   .. method:: __getitem__(index)
//|
//|     Returns the pixel value at the given index as a tuple of (Red, Green, Blue[, White]) values
//|     between 0 and 255.
//|
//|   .. method:: __setitem__(index, value)
//|
//|     Sets the pixel value at the given index. Value can either be a tuple of (Red, Green, Blue
//|     [, White]) values between 0 and 255 or an integer where the red, green and blue values are
//|     packed into the lower three bytes (0xRRGGBB).
//|
STATIC mp_obj_t pixelbuf_pixelbuf_subscr(mp_obj_t self_in, mp_obj_t index_in, mp_obj_t value) {
    if (value == MP_OBJ_NULL) {
        // delete item
        // slice deletion
        return MP_OBJ_NULL; // op not supported
    }

    if (0) {
#if MICROPY_PY_BUILTINS_SLICE
    } else if (MP_OBJ_IS_TYPE(index_in, &mp_type_slice)) {
        mp_bound_slice_t slice;

        size_t length = common_hal__pixelbuf_pixelbuf_get_len(self_in);
        mp_seq_get_fast_slice_indexes(length, index_in, &slice);

        if (slice.step < 0) {
            mp_raise_IndexError(translate("Negative step not supported"));
        }

        if (value == MP_OBJ_SENTINEL) { // Get
            size_t len = slice.stop - slice.start;
            if (slice.step > 1) {
                len = (len / slice.step) + (len % slice.step ? 1 : 0);
            }
            mp_obj_tuple_t* t = MP_OBJ_TO_PTR(mp_obj_new_tuple(len, NULL));
            for (uint i = 0; i < len; i++) {
                t->items[i] = common_hal__pixelbuf_pixelbuf_get_pixel(self_in, i * slice.step);
            }
            return MP_OBJ_FROM_PTR(t);
        } else { // Set
            #if MICROPY_PY_ARRAY_SLICE_ASSIGN

            if (!(MP_OBJ_IS_TYPE(value, &mp_type_list) || MP_OBJ_IS_TYPE(value, &mp_type_tuple))) {
                mp_raise_ValueError(translate("tuple/list required on RHS"));
            }

            size_t dst_len = (slice.stop - slice.start);
            if (slice.step > 1) {
                dst_len = (dst_len / slice.step) + (dst_len % slice.step ? 1 : 0);
            }
            mp_obj_t *src_objs;
            size_t num_items;
            if (MP_OBJ_IS_TYPE(value, &mp_type_list)) {
                mp_obj_list_t *t = MP_OBJ_TO_PTR(value);
                num_items = t->len;
                src_objs = t->items;
            } else {
                mp_obj_tuple_t *l = MP_OBJ_TO_PTR(value);
                num_items = l->len;
                src_objs = l->items;
            }
            if (num_items != dst_len) {
                mp_raise_ValueError_varg(translate("Unmatched number of items on RHS (expected %d, got %d)."),
                                                   dst_len, num_items);
            }

            common_hal__pixelbuf_pixelbuf_set_pixels(self_in, slice.start, slice.stop, slice.step, src_objs);
            return mp_const_none;
            #else
            return MP_OBJ_NULL; // op not supported
            #endif
        }
#endif
    } else { // Single index rather than slice.
        size_t length = common_hal__pixelbuf_pixelbuf_get_len(self_in);
        size_t index = mp_get_index(mp_obj_get_type(self_in), length, index_in, false);

        if (value == MP_OBJ_SENTINEL) { // Get
            return common_hal__pixelbuf_pixelbuf_get_pixel(self_in, index);
        } else { // Store
            common_hal__pixelbuf_pixelbuf_set_pixel(self_in, index, value);
            return mp_const_none;
        }
    }
}

STATIC const mp_rom_map_elem_t pixelbuf_pixelbuf_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_auto_write), MP_ROM_PTR(&pixelbuf_pixelbuf_auto_write_obj)},
    { MP_ROM_QSTR(MP_QSTR_bpp), MP_ROM_PTR(&pixelbuf_pixelbuf_bpp_obj)},
    { MP_ROM_QSTR(MP_QSTR_brightness), MP_ROM_PTR(&pixelbuf_pixelbuf_brightness_obj)},
    { MP_ROM_QSTR(MP_QSTR_byteorder), MP_ROM_PTR(&pixelbuf_pixelbuf_byteorder_str)},
    { MP_ROM_QSTR(MP_QSTR_show), MP_ROM_PTR(&pixelbuf_pixelbuf_show_obj)},
    { MP_ROM_QSTR(MP_QSTR_fill), MP_ROM_PTR(&pixelbuf_pixelbuf_fill_obj)},
};

STATIC MP_DEFINE_CONST_DICT(pixelbuf_pixelbuf_locals_dict, pixelbuf_pixelbuf_locals_dict_table);


const mp_obj_type_t pixelbuf_pixelbuf_type = {
        { &mp_type_type },
        .name = MP_QSTR_PixelBuf,
        .subscr = pixelbuf_pixelbuf_subscr,
        .make_new = pixelbuf_pixelbuf_make_new,
        .unary_op = pixelbuf_pixelbuf_unary_op,
        .getiter = mp_obj_new_generic_iterator,
        .print = NULL,
        .locals_dict = (mp_obj_t)&pixelbuf_pixelbuf_locals_dict,
};
