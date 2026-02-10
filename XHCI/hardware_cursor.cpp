/**
 * Hardware Cursor with VirtIO GPU support and Cursor Resource System
 * Prefers VirtIO GPU hardware cursor, falls back to XOR software cursor
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "hardware_cursor.h"
#include "framebuffer.h"
#include "cursor_resources.h"
#include "../../../lib/memory.h" // for memcpy
#include "../../../qw/Surface.h"
#include "../../../qw/Compositor.h"
#include "../../../qw/cursor/vmware_svga.h"
#include "../../../lib/kprintf.h"

// C-linkage globals from framebuffer.cpp
extern "C" {
    extern uint32_t fb_width, fb_height, fb_pitch;
}

namespace QV {
    // Cursor state
    static bool cursor_initialized = false;
    static bool cursor_visible = false;
    static bool use_hardware_cursor = false;
    static int cursor_x = 0;
    static int cursor_y = 0;
    static int prev_cursor_x = -1;
    static int prev_cursor_y = -1;
    static int cursor_width = 15;
    static int cursor_height = 20;
    static int cursor_hotspot_x = 0;
    static int cursor_hotspot_y = 0;

    // Background buffer for cursor save/restore
    #define MAX_CURSOR_SIZE 64
    static uint32_t cursor_background[MAX_CURSOR_SIZE * MAX_CURSOR_SIZE];
    static bool background_saved = false;

    // Simple arrow cursor pattern (1 = white, 0 = black outline)
    static const uint8_t arrow_cursor[20] = {
        0b10000000,  // X
        0b11000000,  // XX
        0b11100000,  // XXX
        0b11110000,  // XXXX
        0b11111000,  // XXXXX
        0b11111100,  // XXXXXX
        0b11111110,  // XXXXXXX
        0b11111100,  // XXXXXX
        0b11011100,  // XX XXX
        0b10001110,  // X  XXX
        0b00001110,  //    XXX
        0b00000110,  //     XX
        0b00000110,  //     XX
        0b00000011,  //      XX
        0b00000011,  //      XX
        0b00000000,  //
        0b00000000,  //
        0b00000000,  //
        0b00000000,  //
        0b00000000,  //
    };


    /**
     * Normalize an arbitrary cursor_image_t to a 32x32 ARGB bitmap suitable
     * for VMware SVGA cursor definitions.
     *
     * Returns true on success and fills out 'out'. Uses a static backing buffer
     * for the pixel data, so 'out->pixels' remains valid after return.
     */
    static bool cursor_to_32x32_bitmap(cursor_image_t* src, QV::cursor_bitmap_t* out) {
        if (!src || !src->loaded || !src->pixels) {
            serial_debug("[CURSOR] ERROR: cursor_to_32x32: invalid source image\n");
            return false;
        }

        // Static backing store for the normalized bitmap
        static uint32_t hw_pixels[32 * 32];

        if (src->width == 64 && src->height == 64) {
            // Downsample 64→32 by taking every other pixel
            for (uint32_t y = 0; y < 32; y++) {
                for (uint32_t x = 0; x < 32; x++) {
                    hw_pixels[y * 32 + x] =
                        src->pixels[(y * 2) * 64 + (x * 2)];
                }
            }
            out->hot_x = src->hotspot_x / 2;
            out->hot_y = src->hotspot_y / 2;
        }
        else if (src->width == 32 && src->height == 32) {
            // Already correct size
                QLIB::memcpy(hw_pixels, src->pixels, sizeof(hw_pixels));
            out->hot_x = src->hotspot_x;
            out->hot_y = src->hotspot_y;
        }
        else {
            serial_debug("[CURSOR] ERROR: cursor_to_32x32: unsupported size\n");
            return false;
        }

        out->width  = 32;
        out->height = 32;
        out->pixels = hw_pixels;
        return true;
    }

    /**
     * Initialize hardware cursor - tries VMware SVGA first, falls back to software
     */
    bool hardware_cursor_init(void) {
        cursor_resources_init();   // Load cursor themes/bitmaps

        cursor_image_t* src = cursor_res_get_active();
        if (!src || !src->loaded || !src->pixels) {
            serial_debug("[CURSOR] ERROR: No valid cursor resource\n");
            goto fallback;
        }

        // Try VMware SVGA hardware cursor
        if (vmware_svga_init() && vmware_svga_cursor_available()) {
            cursor_bitmap_t hw;

            if (!cursor_to_32x32_bitmap(src, &hw)) {
                // size/format not supported → fallback
                goto fallback;
            }

            use_hardware_cursor = true;
            serial_debug("[CURSOR] Using VMware SVGA hardware cursor\n");

            vmware_svga_cursor_define(
                hw.pixels,
                hw.width,
                hw.height,
                hw.hot_x,
                hw.hot_y
            );

            cursor_initialized = true;
            cursor_visible = false;
            return true;
        }

    fallback:
        use_hardware_cursor = false;
        serial_debug("[CURSOR] Using software cursor overlay\n");

        cursor_initialized = true;
        cursor_visible = false;
        return true;
    }

    /**
     * Draw cursor with alpha blending to specified buffer (back buffer for flicker-free rendering)
     * This is the core rendering function that composites the cursor over existing content
     */
    static void render_cursor_to_buffer(uint32_t* buffer, int x, int y) {
        if (use_hardware_cursor)
        return;

        if (!buffer) return;
        
        // Get current cursor image
        cursor_image_t* cursor = cursor_res_get_active();
        if (!cursor || !cursor->loaded || !cursor->pixels) return;
        
        int screen_x = x - (int)cursor->hotspot_x;
        int screen_y = y - (int)cursor->hotspot_y;
        
        // Draw cursor with alpha blending directly to buffer
        for (uint32_t cy = 0; cy < cursor->height; cy++) {
            int py = screen_y + cy;
            if (py < 0 || py >= (int)::fb_height) continue;
            
            for (uint32_t cx = 0; cx < cursor->width; cx++) {
                int px = screen_x + cx;
                if (px < 0 || px >= (int)::fb_width) continue;
                
                uint32_t cursor_pixel = cursor->pixels[cy * cursor->width + cx];
                uint32_t alpha = (cursor_pixel >> 24) & 0xFF;
                
                if (alpha == 0) continue;  // Fully transparent
                
                uint32_t offset = (py * (::fb_pitch / 4)) + px;
                
                if (alpha == 255) {
                    // Fully opaque - direct copy
                    buffer[offset] = cursor_pixel | 0xFF000000;
                } else {
                    // Alpha blend with background
                    uint32_t bg = buffer[offset];
                    
                    uint32_t bg_r = (bg >> 16) & 0xFF;
                    uint32_t bg_g = (bg >> 8) & 0xFF;
                    uint32_t bg_b = bg & 0xFF;
                    
                    uint32_t fg_r = (cursor_pixel >> 16) & 0xFF;
                    uint32_t fg_g = (cursor_pixel >> 8) & 0xFF;
                    uint32_t fg_b = cursor_pixel & 0xFF;
                    
                    uint32_t inv_alpha = 255 - alpha;
                    uint32_t out_r = (fg_r * alpha + bg_r * inv_alpha) / 255;
                    uint32_t out_g = (fg_g * alpha + bg_g * inv_alpha) / 255;
                    uint32_t out_b = (fg_b * alpha + bg_b * inv_alpha) / 255;
                    
                    buffer[offset] = 0xFF000000 | (out_r << 16) | (out_g << 8) | out_b;
                }
            }
        }
    }

    /**
     * Draw cursor to back buffer - called by compositor before framebuffer_swap()
     * This provides flicker-free cursor rendering with double buffering
     */
    static void draw_cursor_overlay(void) {
        if (use_hardware_cursor)
        return;
        if (!cursor_initialized || !cursor_visible) return;
        
        // CRITICAL: Draw to BACK BUFFER (desktop_buffer), not front buffer!
        // This is the key to flicker-free rendering
        auto& fb = QV::Framebuffer::Instance();
        uint32_t* target = fb.GetBufferPtr();   // or whatever your SimpleVector uses
        // if (!target) {
        //     target = framebuffer_get_desktop_buffer();
        // }
        if (!target) return;
        
        // Render cursor with alpha blending to selected buffer
        render_cursor_to_buffer(target, cursor_x, cursor_y);
    }

    /**
     * Set cursor position
     * With hardware cursor, updates MMIO registers directly
     * With software cursor, position will be rendered next frame
     */
    void hardware_cursor_set_position(int x, int y) {
        if (!cursor_initialized) return;
        
        cursor_x = x;
        cursor_y = y;
        
        // Use hardware cursor if available (updates MMIO registers)
        if (use_hardware_cursor && cursor_visible) {
            vmware_svga_cursor_set_pos(x, y);
        }
        // Software overlay: cursor will be drawn at new position in next frame
    }

    /**
     * Enable or disable cursor visibility
     */
    void hardware_cursor_set_enabled(bool enabled) {
        if (!cursor_initialized) return;
        
        cursor_visible = enabled;
        
        // Use hardware cursor if available (updates GPU MMIO enable register)
        if (use_hardware_cursor) {
            vmware_svga_cursor_set_visible(enabled);
        }
        // Software overlay cursor visibility is handled by draw function
    }

    /**
     * Set cursor image (not supported for simple XOR cursor)
     * For XOR cursor, we use a fixed pattern for maximum speed
     */
    bool hardware_cursor_set_image(const uint32_t* data, int width, int height, 
                                    int hotspot_x, int hotspot_y) {
        (void)data;
        (void)width;
        (void)height;
        
        // Update hotspot for XOR cursor
        cursor_hotspot_x = hotspot_x;
        cursor_hotspot_y = hotspot_y;
        
        return true;  // Hotspot updated
    }

    /**
     * Check if hardware cursor is available
     */
    bool hardware_cursor_is_available(void) {
        return cursor_initialized;
    }

    /**
     * Change cursor type (normal, text, resize, etc.)
     */
    void hardware_cursor_set_type(int type) {
        if (!cursor_initialized) return;
        
        // Set active cursor in resource system
        cursor_res_set_active((cursor_type_t)type);
    }

    /**
     * Invalidate cursor (no-op with double buffering)
     * With double buffering, cursor is always redrawn fresh each frame
     */
    void hardware_cursor_invalidate(void) {
        // No-op: Double buffering renders cursor fresh each frame
    }

    /**
     * Draw cursor to back buffer (called by compositor before swap)
     * CRITICAL: This must be called AFTER all desktop rendering but BEFORE framebuffer_swap()
     */
    void hardware_cursor_draw(void) {
        if (use_hardware_cursor)
            return; // hardware cursor draws itself

        draw_cursor_overlay();
    }
} // namespace QV