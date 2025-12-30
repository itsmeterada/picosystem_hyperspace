/*
 * Hyperspace - PicoSystem Port
 * Original game by J-Fry for PICO-8
 * SDL2 port and PicoSystem port by itsmeterada
 * Uses libfixmath for fixed-point arithmetic
 *
 * This file contains PicoSystem-specific code and includes
 * hyperspace_game.h for the shared game logic.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "picosystem_hardware.h"
#include "libfixmath/fixmath.h"

// Flash storage for persistent data (use last sector of flash)
// RP2040 has 2MB flash, sector size is 4KB
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define FLASH_MAGIC 0x48595045  // "HYPE" in hex

extern struct picosystem_hw pshw;

// ============================================================================
// Screen and Fixed-Point Constants
// ============================================================================

// Screen dimensions (PicoSystem uses 120x120 with pixel doubling)
#define SCREEN_WIDTH 120
#define SCREEN_HEIGHT 120

// Fixed-point constants
#define FIX_HALF F16(0.5)
#define FIX_TWO F16(2.0)
#define FIX_PI fix16_pi
#define FIX_TWO_PI F16(6.28318530718)
#define FIX_SCREEN_CENTER F16(60.0)  // 120/2 for PicoSystem
#define FIX_PROJ_CONST F16(-75.0)    // Adjusted for 120px screen (was -80 for 128px)

// ============================================================================
// Pico-8 Color Palette for PicoSystem
// ============================================================================

// Format: ggggbbbbaaaarrrr
// Pre-calculated using (value * 15 + 127) / 255 for accurate 8-bit to 4-bit conversion
// This provides proper rounding instead of truncation (>> 4)
static const color_t PICO8_PALETTE[16] = {
    0x00F0,  //  0: black        #000000 → ( 0,  0,  0)
    0x35F2,  //  1: dark blue    #1D2B53 → ( 2,  3,  5)
    0x25F7,  //  2: dark purple  #7E2553 → ( 7,  2,  5)
    0x85F0,  //  3: dark green   #008751 → ( 0,  8,  5)
    0x53FA,  //  4: brown        #AB5236 → (10,  5,  3)
    0x55F6,  //  5: dark gray    #5F574F → ( 6,  5,  5)
    0xBCFB,  //  6: light gray   #C2C3C7 → (11, 11, 12)
    0xEEFF,  //  7: white        #FFF1E8 → (15, 14, 14)
    0x05FF,  //  8: red          #FF004D → (15,  0,  5)
    0xA0FF,  //  9: orange       #FFA300 → (15, 10,  0)
    0xE2FF,  // 10: yellow       #FFEC27 → (15, 14,  2)
    0xD3F0,  // 11: green        #00E436 → ( 0, 13,  3)
    0xAFF2,  // 12: blue         #29ADFF → ( 2, 10, 15)
    0x79F8,  // 13: indigo       #83769C → ( 8,  7,  9)
    0x7AFF,  // 14: pink         #FF77A8 → (15,  7, 10)
    0xCAFF,  // 15: peach        #FFCCAA → (15, 12, 10)
};

// ============================================================================
// Buffers and State (required by hyperspace_game.h)
// ============================================================================

// Virtual screen buffer (120x120)
static uint8_t screen[SCREEN_HEIGHT][SCREEN_WIDTH];

// Sprite sheet (128x128 pixels)
static uint8_t spritesheet[128][128];

// Map memory (for mesh data)
static uint8_t map_memory[0x1000];

// Palette mapping for pal()
static uint8_t palette_map[16];

// Drawing color
static uint8_t draw_color = 7;

// Clip region
static int clip_x1 = 0, clip_y1 = 0, clip_x2 = SCREEN_WIDTH - 1, clip_y2 = SCREEN_HEIGHT - 1;

// Random seed
static uint32_t rnd_state = 1;

// Button states
static bool btn_state[6] = {false};
static bool btn_prev[6] = {false};
#ifdef DEBUG_BUILD
static bool btn_y_held = false;  // Y button for palette display
#endif

// Cart data (persistent storage)
static int32_t cart_data[64] = {0};
static bool cart_data_dirty = false;

// ============================================================================
// Flash Storage
// ============================================================================

typedef struct {
    uint32_t magic;
    int32_t data[64];
} FlashSaveData;

static void load_cart_data(void) {
    const FlashSaveData* flash_data = (const FlashSaveData*)(XIP_BASE + FLASH_TARGET_OFFSET);
    if (flash_data->magic == FLASH_MAGIC) {
        memcpy(cart_data, flash_data->data, sizeof(cart_data));
    }
}

static void save_cart_data(void) {
    if (!cart_data_dirty) return;

    FlashSaveData save_data;
    save_data.magic = FLASH_MAGIC;
    memcpy(save_data.data, cart_data, sizeof(cart_data));

    // Pad to flash page size (minimum write size is 256 bytes)
    // save_data is 260 bytes (4 + 64*4), so we need 512 bytes
    uint8_t buffer[512] __attribute__((aligned(4)));
    memset(buffer, 0xFF, sizeof(buffer));
    memcpy(buffer, &save_data, sizeof(save_data));

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, buffer, sizeof(buffer));
    restore_interrupts(ints);

    cart_data_dirty = false;
}

// ============================================================================
// Pico-8 API Implementation (required by hyperspace_game.h)
// ============================================================================

static void cls(void) {
    memset(screen, 0, sizeof(screen));
}

static void pset(int x, int y, int c) {
    if (x >= clip_x1 && x <= clip_x2 && y >= clip_y1 && y <= clip_y2 &&
        x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        screen[y][x] = palette_map[c & 15];
    }
}

// Fast pset - no clipping, no bounds check (for rasterizer inner loop)
// Still uses palette_map for palette animation to work
#define PSET_FAST(x, y, c) (screen[(y)][(x)] = palette_map[(c) & 15])

static uint8_t pget(int x, int y) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        return screen[y][x];
    }
    return 0;
}

static uint8_t sget(int x, int y) {
    if (x >= 0 && x < 128 && y >= 0 && y < 128) {
        return spritesheet[y][x];
    }
    return 0;
}

// Fast texture fetch - no bounds checking (caller must ensure valid coords)
#define SGET_FAST(x, y) (spritesheet[(y)][(x)])

static void line(int x0, int y0, int x1, int y1, int c) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    while (1) {
        pset(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

static void rectfill(int x0, int y0, int x1, int y1, int c) {
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            pset(x, y, c);
        }
    }
}

static void circfill(int cx, int cy, int r, int c) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x*x + y*y <= r*r) {
                pset(cx + x, cy + y, c);
            }
        }
    }
}

static void spr(int n, int x, int y, int w, int h) {
    int sx = (n & 15) * 8;  // bitmask instead of modulo
    int sy = (n / 16) * 8;
    for (int py = 0; py < h * 8; py++) {
        for (int px = 0; px < w * 8; px++) {
            uint8_t c = sget(sx + px, sy + py);
            if (c != 0) {
                pset(x + px, y + py, palette_map[c]);
            }
        }
    }
}

static void pal_reset(void) {
    for (int i = 0; i < 16; i++) palette_map[i] = i;
}

static void pal(int c0, int c1) {
    palette_map[c0 & 15] = c1 & 15;
}

static void clip_set(int x, int y, int w, int h) {
    clip_x1 = x;
    clip_y1 = y;
    clip_x2 = x + w - 1;
    clip_y2 = y + h - 1;
}

static void clip_reset(void) {
    clip_x1 = 0;
    clip_y1 = 0;
    clip_x2 = SCREEN_WIDTH - 1;
    clip_y2 = SCREEN_HEIGHT - 1;
}

static void color(int c) {
    draw_color = c & 15;
}

// ============================================================================
// Platform-specific Sound Implementation
// ============================================================================

#define PLATFORM_SFX
void platform_sfx(int n, int channel) {
    picosystem_sfx(n, channel);
}

// ============================================================================
// Include Shared Game Logic
// ============================================================================

#include "hyperspace_game.h"

// ============================================================================
// Input Handling
// ============================================================================

static void update_input(void) {
    memcpy(btn_prev, btn_state, sizeof(btn_prev));

    uint32_t io = pshw.io;

    // Map PicoSystem buttons to PICO-8 style
    btn_state[0] = !(io & (1 << PICOSYSTEM_INPUT_LEFT));   // Left
    btn_state[1] = !(io & (1 << PICOSYSTEM_INPUT_RIGHT));  // Right
    btn_state[2] = !(io & (1 << PICOSYSTEM_INPUT_UP));     // Up
    btn_state[3] = !(io & (1 << PICOSYSTEM_INPUT_DOWN));   // Down
    // A and B both fire, X and Y both do barrel roll
    btn_state[4] = !(io & (1 << PICOSYSTEM_INPUT_A)) || !(io & (1 << PICOSYSTEM_INPUT_B));  // A/B = fire
    btn_state[5] = !(io & (1 << PICOSYSTEM_INPUT_X)) || !(io & (1 << PICOSYSTEM_INPUT_Y));  // X/Y = barrel roll

#ifdef DEBUG_BUILD
    // Y button alone for palette display
    btn_y_held = !(io & (1 << PICOSYSTEM_INPUT_Y));
#endif
}

#ifdef DEBUG_BUILD
// ============================================================================
// Palette Display (for PICO-8 color comparison)
// ============================================================================

static void draw_palette_display(void) {
    // 4x4 grid of 16 colors
    // Each cell is 30x30 pixels (120/4 = 30)
    const int cell_size = 30;

    for (int i = 0; i < 16; i++) {
        int col = i % 4;
        int row = i / 4;
        int x0 = col * cell_size;
        int y0 = row * cell_size;

        // Fill rectangle with color i (bypass palette_map to show true colors)
        for (int y = y0; y < y0 + cell_size; y++) {
            for (int x = x0; x < x0 + cell_size; x++) {
                screen[y][x] = i;
            }
        }

        // Draw border (color 0 or 7 for contrast)
        int border_color = (i == 0 || i == 1 || i == 2 || i == 5) ? 7 : 0;
        for (int x = x0; x < x0 + cell_size; x++) {
            screen[y0][x] = border_color;
            screen[y0 + cell_size - 1][x] = border_color;
        }
        for (int y = y0; y < y0 + cell_size; y++) {
            screen[y][x0] = border_color;
            screen[y][x0 + cell_size - 1] = border_color;
        }
    }
}
#endif

// ============================================================================
// Screen Flip
// ============================================================================

static void flip_screen(void) {
    // Convert screen buffer to PicoSystem framebuffer
    buffer_t* fb = pshw.screen;
    if (!fb || !fb->data) return;

    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            fb->data[y * SCREEN_WIDTH + x] = PICO8_PALETTE[screen[y][x] & 15];
        }
    }

    picosystem_flip();
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    // Initialize PicoSystem
    picosystem_init();
    picosystem_audio_init();  // Initialize audio system
    stdio_init_all();

    printf("Hyperspace for PicoSystem\r\n");

    // Keep the screen off during init
    picosystem_backlight(0);
    picosystem_flip();
    while(picosystem_is_flipping()) { sleep_ms(1); }
    picosystem_wait_vsync();
    picosystem_wait_vsync();

    // Load sprite and map data
    load_embedded_data();

    // Initialize game
    rnd_state = picosystem_time();
    game_init();

    // Turn on backlight
    picosystem_backlight(75);

    pshw.io = picosystem_gpio_get();

    uint32_t last_frame_time = picosystem_time();
    const uint32_t frame_duration = 33;  // ~30 FPS

    // Main game loop
    while (true) {
        uint32_t current_time = picosystem_time();

        if (current_time - last_frame_time >= frame_duration) {
            last_frame_time = current_time;

            // Update input
            pshw.lio = pshw.io;
            pshw.io = picosystem_gpio_get();
            update_input();

            // Wait for previous flip to complete
            while(picosystem_is_flipping()) {}

            // Update and render
#ifdef DEBUG_BUILD
            if (btn_y_held) {
                // Show palette display when Y is held
                draw_palette_display();
            } else
#endif
            {
                game_update();
                game_draw();
            }

            // Update audio system
            picosystem_audio_update();

            // Flip to screen
            flip_screen();
        }

        sleep_ms(1);
    }

    return 0;
}
