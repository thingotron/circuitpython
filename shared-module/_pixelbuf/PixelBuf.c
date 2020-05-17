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
#include "py/objstr.h"
#include "py/objtype.h"
#include "py/runtime.h"
#include "shared-bindings/_pixelbuf/PixelBuf.h"
#include <string.h>

// Helper to ensure we have the native super class instead of a subclass.
static pixelbuf_pixelbuf_obj_t* native_pixelbuf(mp_obj_t pixelbuf_obj) {
    mp_obj_t native_pixelbuf = mp_instance_cast_to_native_base(pixelbuf_obj, &pixelbuf_pixelbuf_type);
    mp_obj_assert_native_inited(native_pixelbuf);
    return MP_OBJ_TO_PTR(native_pixelbuf);
}

void common_hal__pixelbuf_pixelbuf_construct(pixelbuf_pixelbuf_obj_t *self, size_t n,
        pixelbuf_byteorder_details_t* byteorder, mp_float_t brightness, bool auto_write,
        uint8_t* header, size_t header_len, uint8_t* trailer, size_t trailer_len) {

    self->pixel_count = n;
    self->byteorder = *byteorder;  // Copied because we modify for dotstar
    self->bytes_per_pixel = byteorder->is_dotstar ? 4 : byteorder->bpp;
    self->auto_write = false;

    size_t pixel_len = self->pixel_count * self->bytes_per_pixel;
    self->transmit_buffer_obj = mp_obj_new_bytes_of_zeros(header_len + pixel_len + trailer_len);
    mp_obj_str_t *o = MP_OBJ_TO_PTR(self->transmit_buffer_obj);

    // Abuse the bytes object a bit by mutating it's data by dropping the const. If the user's
    // Python code holds onto it, they'll find out that it changes. At least this way it isn't
    // mutable by the code itself.
    uint8_t* transmit_buffer = (uint8_t*) o->data;
    memcpy(transmit_buffer, header, header_len);
    memcpy(transmit_buffer + header_len + pixel_len, trailer, trailer_len);
    self->post_brightness_buffer = transmit_buffer + header_len;

    if (self->byteorder.is_dotstar) {
        // Initialize the buffer with the dotstar start bytes.
        // Note: Header and end must be setup by caller
        for (uint i = 0; i < self->pixel_count * 4; i += 4) {
            self->post_brightness_buffer[i] = DOTSTAR_LED_START_FULL_BRIGHT;
        }
    }
    // Call set_brightness so that it can allocate a second buffer if needed.
    self->brightness = 1.0;
    common_hal__pixelbuf_pixelbuf_set_brightness(MP_OBJ_FROM_PTR(self), brightness);

    // Turn on auto_write. We don't want to do it with the above brightness call.
    self->auto_write = auto_write;
}

size_t common_hal__pixelbuf_pixelbuf_get_len(mp_obj_t self_in) {
    pixelbuf_pixelbuf_obj_t* self = native_pixelbuf(self_in);
    return self->pixel_count;
}

uint8_t common_hal__pixelbuf_pixelbuf_get_bpp(mp_obj_t self_in) {
    pixelbuf_pixelbuf_obj_t* self = native_pixelbuf(self_in);
    return self->byteorder.bpp;
}

mp_obj_t common_hal__pixelbuf_pixelbuf_get_byteorder_string(mp_obj_t self_in) {
    pixelbuf_pixelbuf_obj_t* self = native_pixelbuf(self_in);
    return self->byteorder.order_string;
}

bool common_hal__pixelbuf_pixelbuf_get_auto_write(mp_obj_t self_in) {
    pixelbuf_pixelbuf_obj_t* self = native_pixelbuf(self_in);
    return self->auto_write;
}

void common_hal__pixelbuf_pixelbuf_set_auto_write(mp_obj_t self_in, bool auto_write) {
    pixelbuf_pixelbuf_obj_t* self = native_pixelbuf(self_in);
    self->auto_write = auto_write;
}

mp_float_t common_hal__pixelbuf_pixelbuf_get_brightness(mp_obj_t self_in) {
    pixelbuf_pixelbuf_obj_t* self = native_pixelbuf(self_in);
    return self->brightness;
}

void common_hal__pixelbuf_pixelbuf_set_brightness(mp_obj_t self_in, mp_float_t brightness) {
    pixelbuf_pixelbuf_obj_t* self = native_pixelbuf(self_in);
    // Skip out if the brightness is already set. The default of self->brightness is 1.0. So, this
    // also prevents the pre_brightness_buffer allocation when brightness is set to 1.0 again.
    mp_float_t change = brightness - self->brightness;
    if (-0.001 < change && change < 0.001) {
        return;
    }
    self->brightness = brightness;
    size_t pixel_len = self->pixel_count * self->bytes_per_pixel;
    if (self->pre_brightness_buffer == NULL) {
        self->pre_brightness_buffer = m_malloc(pixel_len, false);
        memcpy(self->pre_brightness_buffer, self->post_brightness_buffer, pixel_len);
    }
    for (size_t i = 0; i < pixel_len; i++) {
        // Don't adjust per-pixel luminance bytes in dotstar mode
        if (self->byteorder.is_dotstar && i % 4 == 0) {
            continue;
        }
        self->post_brightness_buffer[i] = self->pre_brightness_buffer[i] * self->brightness;
    }

    if (self->auto_write) {
        common_hal__pixelbuf_pixelbuf_show(self_in);
    }
}

void _pixelbuf_parse_color(pixelbuf_pixelbuf_obj_t* self, mp_obj_t color, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* w) {
    pixelbuf_byteorder_details_t *byteorder = &self->byteorder;
    // w is shared between white in NeoPixels and brightness in dotstars (so that DotStars can have
    // per-pixel brightness). Set the defaults here in case it isn't set below.
    if (byteorder->is_dotstar) {
        *w = 255;
    } else {
        *w = 0;
    }

    if (MP_OBJ_IS_INT(color)) {
        mp_int_t value = mp_obj_get_int_truncated(color);
        *r = value >> 16 & 0xff;
        *g = (value >> 8) & 0xff;
        *b = value & 0xff;
        // Int colors can't set white directly so convert to white when all components are equal.
        if (!byteorder->is_dotstar && byteorder->bpp == 4 && byteorder->has_white && *r == *g && *r == *b) {
            *w = *r;
            *r = 0;
            *g = 0;
            *b = 0;
        }
    } else {
        mp_obj_t *items;
        size_t len;
        mp_obj_get_array(color, &len, &items);
        if (len != byteorder->bpp && !byteorder->is_dotstar) {
            mp_raise_ValueError_varg(translate("Expected tuple of length %d, got %d"), byteorder->bpp, len);
        }

        *r = mp_obj_get_int_truncated(items[PIXEL_R]);
        *g = mp_obj_get_int_truncated(items[PIXEL_G]);
        *b = mp_obj_get_int_truncated(items[PIXEL_B]);
        if (len > 3) {
            if (mp_obj_is_float(items[PIXEL_W])) {
                *w = 255 * mp_obj_get_float(items[PIXEL_W]);
            } else {
                *w = mp_obj_get_int_truncated(items[PIXEL_W]);
            }
        }
    }
}

void _pixelbuf_set_pixel_color(pixelbuf_pixelbuf_obj_t* self, size_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
    // DotStars don't have white, instead they have 5 bit brightness so pack it into w. Shift right
    // by three to leave the top five bits.
    if (self->bytes_per_pixel == 4 && self->byteorder.is_dotstar) {
        w = DOTSTAR_LED_START | w >> 3;
    }
    pixelbuf_rgbw_t *rgbw_order = &self->byteorder.byteorder;
    size_t offset = index * self->bytes_per_pixel;
    if (self->pre_brightness_buffer != NULL) {
        uint8_t* pre_brightness_buffer = self->pre_brightness_buffer + offset;
        if (self->bytes_per_pixel == 4) {
            pre_brightness_buffer[rgbw_order->w] = w;
        }

        pre_brightness_buffer[rgbw_order->r] = r;
        pre_brightness_buffer[rgbw_order->g] = g;
        pre_brightness_buffer[rgbw_order->b] = b;
    }

    uint8_t* post_brightness_buffer = self->post_brightness_buffer + offset;
    if (self->bytes_per_pixel == 4) {
        // Only apply brightness if w is actually white (aka not DotStar.)
        if (!self->byteorder.is_dotstar) {
            w *= self->brightness;
        }
        post_brightness_buffer[rgbw_order->w] = w;
    }
    post_brightness_buffer[rgbw_order->r] = r * self->brightness;
    post_brightness_buffer[rgbw_order->g] = g * self->brightness;
    post_brightness_buffer[rgbw_order->b] = b * self->brightness;
}

void _pixelbuf_set_pixel(pixelbuf_pixelbuf_obj_t* self, size_t index, mp_obj_t value) {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
    _pixelbuf_parse_color(self, value, &r, &g, &b, &w);
    _pixelbuf_set_pixel_color(self, index, r, g, b, w);
}

void common_hal__pixelbuf_pixelbuf_set_pixels(mp_obj_t self_in, size_t start, size_t stop, size_t step, mp_obj_t* values) {
    pixelbuf_pixelbuf_obj_t* self = native_pixelbuf(self_in);
    size_t source_i = 0;
    for (size_t target_i = start; target_i < stop; target_i += step) {
        _pixelbuf_set_pixel(self, target_i, values[source_i]);
        source_i++;
    }
    if (self->auto_write) {
        common_hal__pixelbuf_pixelbuf_show(self_in);
    }
}

void common_hal__pixelbuf_pixelbuf_set_pixel(mp_obj_t self_in, size_t index, mp_obj_t value) {
    pixelbuf_pixelbuf_obj_t* self = native_pixelbuf(self_in);
    _pixelbuf_set_pixel(self, index, value);
    if (self->auto_write) {
        common_hal__pixelbuf_pixelbuf_show(self_in);
    }
}

mp_obj_t common_hal__pixelbuf_pixelbuf_get_pixel(mp_obj_t self_in, size_t index) {
    pixelbuf_pixelbuf_obj_t* self = native_pixelbuf(self_in);
    mp_obj_t elems[self->byteorder.bpp];
    uint8_t* pixel_buffer = self->post_brightness_buffer;
    if (self->pre_brightness_buffer != NULL) {
        pixel_buffer = self->pre_brightness_buffer;
    }
    pixel_buffer += self->byteorder.bpp * index;

    pixelbuf_rgbw_t *rgbw_order = &self->byteorder.byteorder;
    elems[0] = MP_OBJ_NEW_SMALL_INT(pixel_buffer[rgbw_order->r]);
    elems[1] = MP_OBJ_NEW_SMALL_INT(pixel_buffer[rgbw_order->g]);
    elems[2] = MP_OBJ_NEW_SMALL_INT(pixel_buffer[rgbw_order->b]);
    if (self->byteorder.bpp > 3) {
        uint8_t w = pixel_buffer[rgbw_order->w];
        if (self->byteorder.is_dotstar) {
            elems[3] = mp_obj_new_float((w & 0b00011111) / 31.0);
        } else {
            elems[3] = MP_OBJ_NEW_SMALL_INT(w);
        }
    }

    return mp_obj_new_tuple(self->byteorder.bpp, elems);
}

void common_hal__pixelbuf_pixelbuf_show(mp_obj_t self_in) {
    pixelbuf_pixelbuf_obj_t* self = native_pixelbuf(self_in);
    mp_obj_t dest[2 + 1];
    mp_load_method(self_in, MP_QSTR__transmit, dest);

    dest[2] = self->transmit_buffer_obj;

    mp_call_method_n_kw(1, 0, dest);
}

void common_hal__pixelbuf_pixelbuf_fill(mp_obj_t self_in, mp_obj_t fill_color) {
    pixelbuf_pixelbuf_obj_t* self = native_pixelbuf(self_in);

    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
    _pixelbuf_parse_color(self, fill_color, &r, &g, &b, &w);

    for (size_t i = 0; i < self->pixel_count; i++) {
        _pixelbuf_set_pixel_color(self, i, r, g, b, w);
    }
    if (self->auto_write) {
        common_hal__pixelbuf_pixelbuf_show(self_in);
    }
}
