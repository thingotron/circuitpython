
#include <string.h>

#include "shared-module/displayio/__init__.h"

#include "lib/utils/interrupt_char.h"
#include "py/reload.h"
#include "py/runtime.h"
#include "shared-bindings/board/__init__.h"
#include "shared-bindings/displayio/Bitmap.h"
#include "shared-bindings/displayio/Display.h"
#include "shared-bindings/displayio/Group.h"
#include "shared-bindings/displayio/Palette.h"
#include "shared-module/displayio/area.h"
#include "supervisor/shared/autoreload.h"
#include "supervisor/shared/display.h"
#include "supervisor/memory.h"

primary_display_t displays[CIRCUITPY_DISPLAY_LIMIT];

// Check for recursive calls to displayio_background.
bool displayio_background_in_progress = false;

void displayio_background(void) {
    if (mp_hal_is_interrupted()) {
        return;
    }
    if (reload_requested) {
        // Reload is about to happen, so don't redisplay.
        return;
    }

    if (displayio_background_in_progress) {
        // Don't allow recursive calls to this routine.
        return;
    }

    displayio_background_in_progress = true;

    for (uint8_t i = 0; i < CIRCUITPY_DISPLAY_LIMIT; i++) {
        if (displays[i].display.base.type == NULL || displays[i].display.base.type == &mp_type_NoneType) {
            // Skip null display.
            continue;
        }
        if (displays[i].display.base.type == &displayio_display_type) {
            displayio_display_background(&displays[i].display);
        } else if (displays[i].epaper_display.base.type == &displayio_epaperdisplay_type) {
            displayio_epaperdisplay_background(&displays[i].epaper_display);
        }
    }

    // All done.
    displayio_background_in_progress = false;
}

void common_hal_displayio_release_displays(void) {
    // Release displays before busses so that they can send any final commands to turn the display
    // off properly.
    for (uint8_t i = 0; i < CIRCUITPY_DISPLAY_LIMIT; i++) {
        mp_const_obj_t display_type = displays[i].display.base.type;
        if (display_type == NULL || display_type == &mp_type_NoneType) {
            continue;
        } else if (display_type == &displayio_display_type) {
            release_display(&displays[i].display);
        } else if (display_type == &displayio_epaperdisplay_type) {
            release_epaperdisplay(&displays[i].epaper_display);
        }
        displays[i].display.base.type = &mp_type_NoneType;
    }
    for (uint8_t i = 0; i < CIRCUITPY_DISPLAY_LIMIT; i++) {
        mp_const_obj_t bus_type = displays[i].fourwire_bus.base.type;
        if (bus_type == NULL || bus_type == &mp_type_NoneType) {
            continue;
        } else if (bus_type == &displayio_fourwire_type) {
            common_hal_displayio_fourwire_deinit(&displays[i].fourwire_bus);
        } else if (bus_type == &displayio_i2cdisplay_type) {
            common_hal_displayio_i2cdisplay_deinit(&displays[i].i2cdisplay_bus);
        } else if (bus_type == &displayio_parallelbus_type) {
            common_hal_displayio_parallelbus_deinit(&displays[i].parallel_bus);
        }
        displays[i].fourwire_bus.base.type = &mp_type_NoneType;
    }

    supervisor_stop_terminal();
}

void reset_displays(void) {
    // The SPI buses used by FourWires may be allocated on the heap so we need to move them inline.
    for (uint8_t i = 0; i < CIRCUITPY_DISPLAY_LIMIT; i++) {
        if (displays[i].fourwire_bus.base.type == &displayio_fourwire_type) {
            displayio_fourwire_obj_t* fourwire = &displays[i].fourwire_bus;
            if (((uint32_t) fourwire->bus) < ((uint32_t) &displays) ||
                ((uint32_t) fourwire->bus) > ((uint32_t) &displays + CIRCUITPY_DISPLAY_LIMIT)) {
                busio_spi_obj_t* original_spi = fourwire->bus;
                #if BOARD_SPI
                    // We don't need to move original_spi if it is the board.SPI object because it is
                    // statically allocated already. (Doing so would also make it impossible to reference in
                    // a subsequent VM run.)
                    if (original_spi == common_hal_board_get_spi()) {
                        continue;
                    }
                #endif
                memcpy(&fourwire->inline_bus, original_spi, sizeof(busio_spi_obj_t));
                fourwire->bus = &fourwire->inline_bus;
                // Check for other displays that use the same spi bus and swap them too.
                for (uint8_t j = i + 1; j < CIRCUITPY_DISPLAY_LIMIT; j++) {
                    if (displays[i].fourwire_bus.base.type == &displayio_fourwire_type &&
                            displays[i].fourwire_bus.bus == original_spi) {
                        displays[i].fourwire_bus.bus = &fourwire->inline_bus;
                    }
                }
            }
        } else if (displays[i].i2cdisplay_bus.base.type == &displayio_i2cdisplay_type) {
            displayio_i2cdisplay_obj_t* i2c = &displays[i].i2cdisplay_bus;
            if (((uint32_t) i2c->bus) < ((uint32_t) &displays) ||
                ((uint32_t) i2c->bus) > ((uint32_t) &displays + CIRCUITPY_DISPLAY_LIMIT)) {
                busio_i2c_obj_t* original_i2c = i2c->bus;
                #if BOARD_I2C
                    // We don't need to move original_i2c if it is the board.I2C object because it is
                    // statically allocated already. (Doing so would also make it impossible to reference in
                    // a subsequent VM run.)
                    if (original_i2c == common_hal_board_get_i2c()) {
                        continue;
                    }
                #endif
                memcpy(&i2c->inline_bus, original_i2c, sizeof(busio_i2c_obj_t));
                i2c->bus = &i2c->inline_bus;
                // Check for other displays that use the same i2c bus and swap them too.
                for (uint8_t j = i + 1; j < CIRCUITPY_DISPLAY_LIMIT; j++) {
                    if (displays[i].i2cdisplay_bus.base.type == &displayio_i2cdisplay_type &&
                            displays[i].i2cdisplay_bus.bus == original_i2c) {
                        displays[i].i2cdisplay_bus.bus = &i2c->inline_bus;
                    }
                }
            }
        } else {
            // Not an active display bus.
            continue;
        }
    }

    for (uint8_t i = 0; i < CIRCUITPY_DISPLAY_LIMIT; i++) {
        // Reset the displayed group. Only the first will get the terminal but
        // that's ok.
        if (displays[i].display.base.type == &displayio_display_type) {
            reset_display(&displays[i].display);
        } else if (displays[i].epaper_display.base.type == &displayio_epaperdisplay_type) {
            displayio_epaperdisplay_obj_t* display = &displays[i].epaper_display;
            common_hal_displayio_epaperdisplay_show(display, NULL);
        }
    }
}

void displayio_gc_collect(void) {
    for (uint8_t i = 0; i < CIRCUITPY_DISPLAY_LIMIT; i++) {
        if (displays[i].display.base.type == NULL) {
            continue;
        }

        // Alternatively, we could use gc_collect_root over the whole object,
        // but this is more precise, and is the only field that needs marking.
        if (displays[i].display.base.type == &displayio_display_type) {
            displayio_display_collect_ptrs(&displays[i].display);
        } else if (displays[i].epaper_display.base.type == &displayio_epaperdisplay_type) {
            displayio_epaperdisplay_collect_ptrs(&displays[i].epaper_display);
        }
    }
}

void displayio_area_expand(displayio_area_t* original, const displayio_area_t* addition) {
    if (addition->x1 < original->x1) {
        original->x1 = addition->x1;
    }
    if (addition->y1 < original->y1) {
        original->y1 = addition->y1;
    }
    if (addition->x2 > original->x2) {
        original->x2 = addition->x2;
    }
    if (addition->y2 > original->y2) {
        original->y2 = addition->y2;
    }
}

void displayio_area_copy(const displayio_area_t* src, displayio_area_t* dst) {
    dst->x1 = src->x1;
    dst->y1 = src->y1;
    dst->x2 = src->x2;
    dst->y2 = src->y2;
}

void displayio_area_scale(displayio_area_t* area, uint16_t scale) {
    area->x1 *= scale;
    area->y1 *= scale;
    area->x2 *= scale;
    area->y2 *= scale;
}

void displayio_area_shift(displayio_area_t* area, int16_t dx, int16_t dy) {
    area->x1 += dx;
    area->y1 += dy;
    area->x2 += dx;
    area->y2 += dy;
}

bool displayio_area_compute_overlap(const displayio_area_t* a,
                                    const displayio_area_t* b,
                                    displayio_area_t* overlap) {
    overlap->x1 = a->x1;
    if (b->x1 > overlap->x1) {
        overlap->x1 = b->x1;
    }
    overlap->x2 = a->x2;
    if (b->x2 < overlap->x2) {
        overlap->x2 = b->x2;
    }
    if (overlap->x1 >= overlap->x2) {
        return false;
    }
    overlap->y1 = a->y1;
    if (b->y1 > overlap->y1) {
        overlap->y1 = b->y1;
    }
    overlap->y2 = a->y2;
    if (b->y2 < overlap->y2) {
        overlap->y2 = b->y2;
    }
    if (overlap->y1 >= overlap->y2) {
        return false;
    }
    return true;
}

void displayio_area_union(const displayio_area_t* a,
                          const displayio_area_t* b,
                          displayio_area_t* u) {
    u->x1 = a->x1;
    if (b->x1 < u->x1) {
        u->x1 = b->x1;
    }
    u->x2 = a->x2;
    if (b->x2 > u->x2) {
        u->x2 = b->x2;
    }

    u->y1 = a->y1;
    if (b->y1 < u->y1) {
        u->y1 = b->y1;
    }
    u->y2 = a->y2;
    if (b->y2 > u->y2) {
        u->y2 = b->y2;
    }
}

uint16_t displayio_area_width(const displayio_area_t* area) {
    return area->x2 - area->x1;
}

uint16_t displayio_area_height(const displayio_area_t* area) {
    return area->y2 - area->y1;
}

uint32_t displayio_area_size(const displayio_area_t* area) {
    return displayio_area_width(area) * displayio_area_height(area);
}

bool displayio_area_equal(const displayio_area_t* a, const displayio_area_t* b) {
    return a->x1 == b->x1 &&
           a->y1 == b->y1 &&
           a->x2 == b->x2 &&
           a->y2 == b->y2;
}

// Original and whole must be in the same coordinate space.
void displayio_area_transform_within(bool mirror_x, bool mirror_y, bool transpose_xy,
                                     const displayio_area_t* original,
                                     const displayio_area_t* whole,
                                     displayio_area_t* transformed) {
    if (mirror_x) {
        transformed->x1 = whole->x1 + (whole->x2 - original->x2);
        transformed->x2 = whole->x2 - (original->x1 - whole->x1);
    } else {
        transformed->x1 = original->x1;
        transformed->x2 = original->x2;
    }
    if (mirror_y) {
        transformed->y1 = whole->y1 + (whole->y2 - original->y2);
        transformed->y2 = whole->y2 - (original->y1 - whole->y1);
    } else {
        transformed->y1 = original->y1;
        transformed->y2 = original->y2;
    }
    if (transpose_xy) {
        int16_t y1 = transformed->y1;
        int16_t y2 = transformed->y2;
        transformed->y1 = whole->y1 + (transformed->x1 - whole->x1);
        transformed->y2 = whole->y1 + (transformed->x2 - whole->x1);
        transformed->x2 = whole->x1 + (y2 - whole->y1);
        transformed->x1 = whole->x1 + (y1 - whole->y1);
    }
}
