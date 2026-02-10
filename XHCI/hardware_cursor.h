/**
 * Hardware Cursor Support
 * Provides GPU hardware cursor overlay when available
 */

#ifndef HARDWARE_CURSOR_H
#define HARDWARE_CURSOR_H
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

namespace QV {

    typedef struct {
        uint32_t width;
        uint32_t height;
        uint32_t hot_x;
        uint32_t hot_y;
        const uint32_t* pixels;
    } cursor_bitmap_t;



    /**
     * Initialize hardware cursor support
     * Returns true if hardware cursor is available, false if software cursor must be used
     */
    bool hardware_cursor_init(void);

    /**
     * Set hardware cursor position (in screen coordinates)
     */
    void hardware_cursor_set_position(int x, int y);

    /**
     * Enable or disable hardware cursor visibility
     */
    void hardware_cursor_set_enabled(bool enabled);

    /**
     * Upload cursor image to hardware
     * data: ARGB pixel data
     * width, height: cursor dimensions (typically 32x32 or 64x64)
     * hotspot_x, hotspot_y: cursor hotspot offset
     * Returns true if successful
     */
    bool hardware_cursor_set_image(const uint32_t* data, int width, int height,
                                    int hotspot_x, int hotspot_y);

    /**
     * Check if hardware cursor is available
     */
    bool hardware_cursor_is_available(void);

    /**
     * Change cursor type (0=normal, 1=text, 2=resize_h, etc.)
     */
    void hardware_cursor_set_type(int type);

    /**
     * Draw cursor overlay (called by compositor each frame)
     */
    void hardware_cursor_draw(void);
    void hardware_cursor_invalidate(void);

    #endif // HARDWARE_CURSOR_H
} // namespace QV