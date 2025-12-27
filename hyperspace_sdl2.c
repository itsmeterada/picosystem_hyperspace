/*
 * Hyperspace - SDL2 Port
 * Original game by J-Fry for PICO-8
 * Ported to SDL2
 */

#define _USE_MATH_DEFINES
#include <math.h>

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Pico-8 screen dimensions
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128
#define SCALE 4

// Pico-8 color palette
static const uint32_t PICO8_PALETTE[16] = {
    0xFF000000, // 0: black
    0xFF1D2B53, // 1: dark blue
    0xFF7E2553, // 2: dark purple
    0xFF008751, // 3: dark green
    0xFFAB5236, // 4: brown
    0xFF5F574F, // 5: dark gray
    0xFFC2C3C7, // 6: light gray
    0xFFFFF1E8, // 7: white
    0xFFFF004D, // 8: red
    0xFFFFA300, // 9: orange
    0xFFFFEC27, // 10: yellow
    0xFF00E436, // 11: green
    0xFF29ADFF, // 12: blue
    0xFF83769C, // 13: indigo
    0xFFFF77A8, // 14: pink
    0xFFFFCCAA  // 15: peach
};

// Virtual screen buffer (128x128)
static uint8_t screen[SCREEN_HEIGHT][SCREEN_WIDTH];

// Sprite sheet (128x128 pixels)
static uint8_t spritesheet[128][128];

// Map memory (for mesh data)
static uint8_t map_memory[0x1000];

// Palette mapping for pal()
static uint8_t palette_map[16];

// Drawing color
static uint8_t draw_color = 7;

// Cursor position
static int cursor_x = 0, cursor_y = 0;

// Clip region
static int clip_x1 = 0, clip_y1 = 0, clip_x2 = 127, clip_y2 = 127;

// Random seed
static uint32_t rnd_state = 1;

// Button states
static bool btn_state[6] = {false};
static bool btn_prev[6] = {false};

// Cart data (persistent storage)
static int32_t cart_data[64] = {0};
static bool cart_data_loaded = false;
static char cart_data_filename[256];

// SDL globals
static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;
static uint32_t pixel_buffer[SCREEN_HEIGHT * SCREEN_WIDTH];

// Forward declarations
static void game_init(void);
static void game_update(void);
static void game_draw(void);

// ============================================================================
// Pico-8 API Implementation
// ============================================================================

static void cls(void) {
    memset(screen, 0, sizeof(screen));
}

static void pset(int x, int y, int c) {
    if (x >= clip_x1 && x <= clip_x2 && y >= clip_y1 && y <= clip_y2) {
        screen[y][x] = palette_map[c & 15];
    }
}

static uint8_t pget(int x, int y) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        return screen[y][x];
    }
    return 0;
}

static uint8_t sget(int x, int y) {
    if (x >= 0 && x < 128 && y >= 0 && y < 128) {
        return spritesheet[(int)y][(int)x];
    }
    return 0;
}

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

static void rect(int x0, int y0, int x1, int y1, int c) {
    line(x0, y0, x1, y0, c);
    line(x1, y0, x1, y1, c);
    line(x1, y1, x0, y1, c);
    line(x0, y1, x0, y0, c);
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

static void circ(int cx, int cy, int r, int c) {
    int x = r;
    int y = 0;
    int err = 1 - r;

    while (x >= y) {
        pset(cx + x, cy + y, c);
        pset(cx + y, cy + x, c);
        pset(cx - y, cy + x, c);
        pset(cx - x, cy + y, c);
        pset(cx - x, cy - y, c);
        pset(cx - y, cy - x, c);
        pset(cx + y, cy - x, c);
        pset(cx + x, cy - y, c);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
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
    int sx = (n % 16) * 8;
    int sy = (n / 16) * 8;
    for (int py = 0; py < h * 8; py++) {
        for (int px = 0; px < w * 8; px++) {
            uint8_t c = sget(sx + px, sy + py);
            if (c != 0) {  // Transparent color
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
    clip_x2 = 127;
    clip_y2 = 127;
}

static void color(int c) {
    draw_color = c & 15;
}

// PICO-8 compatible 3x5 font
static const uint8_t font_data[96][5] = {
    {0x0,0x0,0x0,0x0,0x0}, // space
    {0x2,0x2,0x2,0x0,0x2}, // !
    {0x5,0x5,0x0,0x0,0x0}, // "
    {0x5,0x7,0x5,0x7,0x5}, // #
    {0x6,0x3,0x6,0x3,0x6}, // $
    {0x1,0x4,0x2,0x1,0x4}, // %
    {0x2,0x5,0x2,0x5,0x3}, // &
    {0x2,0x2,0x0,0x0,0x0}, // '
    {0x1,0x2,0x2,0x2,0x1}, // (
    {0x4,0x2,0x2,0x2,0x4}, // )
    {0x5,0x2,0x5,0x0,0x0}, // *
    {0x0,0x2,0x7,0x2,0x0}, // +
    {0x0,0x0,0x0,0x2,0x4}, // ,
    {0x0,0x0,0x7,0x0,0x0}, // -
    {0x0,0x0,0x0,0x0,0x2}, // .
    {0x1,0x1,0x2,0x4,0x4}, // /
    {0x2,0x5,0x5,0x5,0x2}, // 0
    {0x2,0x6,0x2,0x2,0x7}, // 1
    {0x6,0x1,0x2,0x4,0x7}, // 2
    {0x6,0x1,0x2,0x1,0x6}, // 3
    {0x5,0x5,0x7,0x1,0x1}, // 4
    {0x7,0x4,0x6,0x1,0x6}, // 5
    {0x3,0x4,0x6,0x5,0x2}, // 6
    {0x7,0x1,0x2,0x2,0x2}, // 7
    {0x2,0x5,0x2,0x5,0x2}, // 8
    {0x2,0x5,0x3,0x1,0x6}, // 9
    {0x0,0x2,0x0,0x2,0x0}, // :
    {0x0,0x2,0x0,0x2,0x4}, // ;
    {0x1,0x2,0x4,0x2,0x1}, // <
    {0x0,0x7,0x0,0x7,0x0}, // =
    {0x4,0x2,0x1,0x2,0x4}, // >
    {0x2,0x5,0x1,0x0,0x2}, // ?
    {0x2,0x5,0x5,0x4,0x3}, // @
    {0x2,0x5,0x7,0x5,0x5}, // A
    {0x6,0x5,0x6,0x5,0x6}, // B
    {0x3,0x4,0x4,0x4,0x3}, // C
    {0x6,0x5,0x5,0x5,0x6}, // D
    {0x7,0x4,0x6,0x4,0x7}, // E
    {0x7,0x4,0x6,0x4,0x4}, // F
    {0x3,0x4,0x5,0x5,0x3}, // G
    {0x5,0x5,0x7,0x5,0x5}, // H
    {0x7,0x2,0x2,0x2,0x7}, // I
    {0x1,0x1,0x1,0x5,0x2}, // J
    {0x5,0x5,0x6,0x5,0x5}, // K
    {0x4,0x4,0x4,0x4,0x7}, // L
    {0x5,0x7,0x5,0x5,0x5}, // M
    {0x5,0x7,0x7,0x5,0x5}, // N
    {0x2,0x5,0x5,0x5,0x2}, // O
    {0x6,0x5,0x6,0x4,0x4}, // P
    {0x2,0x5,0x5,0x6,0x3}, // Q
    {0x6,0x5,0x6,0x5,0x5}, // R
    {0x3,0x4,0x2,0x1,0x6}, // S
    {0x7,0x2,0x2,0x2,0x2}, // T
    {0x5,0x5,0x5,0x5,0x2}, // U
    {0x5,0x5,0x5,0x5,0x2}, // V
    {0x5,0x5,0x5,0x7,0x5}, // W
    {0x5,0x5,0x2,0x5,0x5}, // X
    {0x5,0x5,0x2,0x2,0x2}, // Y
    {0x7,0x1,0x2,0x4,0x7}, // Z
    {0x3,0x2,0x2,0x2,0x3}, // [
    {0x4,0x4,0x2,0x1,0x1}, // backslash
    {0x6,0x2,0x2,0x2,0x6}, // ]
    {0x2,0x5,0x0,0x0,0x0}, // ^
    {0x0,0x0,0x0,0x0,0x7}, // _
    {0x4,0x2,0x0,0x0,0x0}, // `
    {0x0,0x3,0x5,0x5,0x3}, // a
    {0x4,0x6,0x5,0x5,0x6}, // b
    {0x0,0x3,0x4,0x4,0x3}, // c
    {0x1,0x3,0x5,0x5,0x3}, // d
    {0x0,0x2,0x5,0x6,0x3}, // e
    {0x1,0x2,0x7,0x2,0x2}, // f
    {0x0,0x3,0x5,0x3,0x6}, // g
    {0x4,0x6,0x5,0x5,0x5}, // h
    {0x2,0x0,0x2,0x2,0x2}, // i
    {0x1,0x0,0x1,0x1,0x6}, // j
    {0x4,0x5,0x6,0x5,0x5}, // k
    {0x2,0x2,0x2,0x2,0x1}, // l
    {0x0,0x5,0x7,0x5,0x5}, // m
    {0x0,0x6,0x5,0x5,0x5}, // n
    {0x0,0x2,0x5,0x5,0x2}, // o
    {0x0,0x6,0x5,0x6,0x4}, // p
    {0x0,0x3,0x5,0x3,0x1}, // q
    {0x0,0x3,0x4,0x4,0x4}, // r
    {0x0,0x3,0x6,0x1,0x6}, // s
    {0x2,0x7,0x2,0x2,0x1}, // t
    {0x0,0x5,0x5,0x5,0x3}, // u
    {0x0,0x5,0x5,0x5,0x2}, // v
    {0x0,0x5,0x5,0x7,0x5}, // w
    {0x0,0x5,0x2,0x2,0x5}, // x
    {0x0,0x5,0x5,0x3,0x6}, // y
    {0x0,0x7,0x1,0x4,0x7}, // z
    {0x1,0x2,0x6,0x2,0x1}, // {
    {0x2,0x2,0x2,0x2,0x2}, // |
    {0x4,0x2,0x3,0x2,0x4}, // }
    {0x5,0x2,0x0,0x0,0x0}, // ~
    {0x0,0x0,0x0,0x0,0x0}, // DEL
};

static void print_char(char c, int x, int y, int col) {
    if (c < 32 || c > 127) return;
    int idx = c - 32;
    for (int row = 0; row < 5; row++) {
        uint8_t bits = font_data[idx][row];
        for (int col_idx = 0; col_idx < 3; col_idx++) {
            if (bits & (0x4 >> col_idx)) {
                pset(x + col_idx, y + row, col);
            }
        }
    }
}

static void print_str(const char* str, int x, int y, int col) {
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            y += 6;
            cx = x;
        } else {
            print_char(*str, cx, y, col);
            cx += 4;
        }
        str++;
    }
}

static float rnd(float max) {
    rnd_state = rnd_state * 1103515245 + 12345;
    return (float)(rnd_state % 10000) / 10000.0f * max;
}

static int flr(float x) {
    return (int)floor(x);
}

static float mid_f(float a, float b, float c) {
    if (a > b) { float t = a; a = b; b = t; }
    if (b > c) { b = c; }
    if (a > b) { b = a; }
    return b;
}

static float sgn(float x) {
    if (x > 0) return 1;
    if (x < 0) return -1;
    return 0;
}

static uint8_t peek(int addr) {
    if (addr >= 0x2000 && addr < 0x3000) {
        return map_memory[addr - 0x2000];
    }
    return 0;
}

static bool btn(int n) {
    return btn_state[n];
}

static bool btnp(int n) {
    return btn_state[n] && !btn_prev[n];
}

// Cart data persistence
static void cartdata(const char* name) {
    snprintf(cart_data_filename, sizeof(cart_data_filename), "%s.sav", name);
    FILE* f = fopen(cart_data_filename, "rb");
    if (f) {
        fread(cart_data, sizeof(cart_data), 1, f);
        fclose(f);
    }
    cart_data_loaded = true;
}

static int32_t dget(int n) {
    if (n >= 0 && n < 64) return cart_data[n];
    return 0;
}

static void dset(int n, int32_t v) {
    if (n >= 0 && n < 64) {
        cart_data[n] = v;
        // Save immediately
        FILE* f = fopen(cart_data_filename, "wb");
        if (f) {
            fwrite(cart_data, sizeof(cart_data), 1, f);
            fclose(f);
        }
    }
}

// sfx stub (no audio in this port)
static void sfx(int n, int channel) {
    // Sound effects not implemented
}

// ============================================================================
// Game Data Types
// ============================================================================

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    float m[12];  // 3x4 matrix
} Mat34;

typedef struct {
    Vec3 pos;
    int tri[3];
    float uv[3][2];
    Vec3 normal;
    float z;  // for sorting
} Triangle;

typedef struct {
    Vec3* vertices;
    Vec3* projected;
    Triangle* triangles;
    int num_vertices;
    int num_triangles;
} Mesh;

typedef struct {
    int x, y;
    int light_x;
} Texture;

typedef struct {
    Vec3 pos0, pos1;
    Vec3 proj0, proj1;
    Vec3 spd;
} Laser;

typedef struct {
    Vec3 pos0, pos1;
    Vec3 proj0, proj1;
    float spd;
    int col;
} Trail;

typedef struct {
    Vec3 pos;
    float spd;
    int index;
    Vec3 proj;
} Background;

typedef struct {
    Vec3 pos;
    int type;
    Vec3* proj;
    int life;
    Vec3 light_dir;
    int hit_t;
    Vec3 hit_pos;
    float rot_x, rot_y;
    float rot_x_spd, rot_y_spd;
    Vec3 spd;
    Vec3 waypoint;
    float laser_t;
    float stop_laser_t;
    float next_laser_t;
    float laser_offset_x[2];
    float laser_offset_y[2];
} Enemy;

// ============================================================================
// Game State
// ============================================================================

// Ship mesh
static Mesh ship_mesh;
static Texture ship_tex, ship_tex_laser_lit;

// Enemy meshes (4 types)
static Mesh nme_meshes[4];
static Texture nme_tex[4];
static Texture nme_tex_hit;

static float nme_scale[4] = {1.0f, 2.5f, 3.0f, 5.0f};
static int nme_life[4] = {1, 3, 10, 80};
static int nme_score[4] = {1, 10, 10, 100};
static float nme_radius[4] = {3.25f, 6.0f, 8.0f, 16.0f};
static float nme_bounds[3] = {-50.0f, -50.0f, -100.0f};
static float nme_rot[3] = {0.18f, 0.24f, 0.06f};
static float nme_spd[3] = {1.0f, 0.5f, 0.6f};

// Trails
#define MAX_TRAILS 64
static Trail trails[MAX_TRAILS];
static int trail_color[5] = {7, 7, 6, 13, 1};

// Backgrounds
#define MAX_BGS 64
static Background bgs[MAX_BGS];
static int bg_color[3] = {12, 13, 6};

// Lasers
#define MAX_LASERS 100
static Laser lasers[MAX_LASERS];
static int num_lasers = 0;
static Laser nme_lasers[MAX_LASERS];
static int num_nme_lasers = 0;

// Enemies
#define MAX_ENEMIES 50
static Enemy enemies[MAX_ENEMIES];
static int num_enemies = 0;
static int nb_nme_ship = 0;

// Camera
static Mat34 cam_mat;
static float cam_x = 0, cam_y = 0;
static float cam_angle_z = -0.4f;
static float cam_angle_x = 0;
static float cam_depth = 22.5f;

// Ship state
static Mat34 ship_mat, inv_ship_mat, ship_pos_mat;
static float ship_x = 0, ship_y = 0;
static float ship_spd_x = 0, ship_spd_y = 0;
static float roll_angle = 0, roll_spd = 0;
static float pitch_angle = 0, pitch_spd = 0;
static float roll_f = 0, pitch_f = 0;
static float cur_noise_t = 0, tgt_noise_t = 0;
static float cur_noise_roll = 0, old_noise_roll = 0;
static float cur_noise_pitch = 0, old_noise_pitch = 0;

// Light
static Mat34 light_mat;
static Vec3 light_dir, ship_light_dir;

// Game state
static int cur_mode = 0;
static int life = 4;
static int score = 0;
static int best_score = 0;
static float global_t = 0;
static float game_spd = 1.0f;
static int hit_t = -1;
static Vec3 hit_pos;
static float barrel_cur_t = -1;
static int barrel_dir = 0;
static bool laser_on = false;
static bool laser_spawned = false;
static float aim_z = -200;
static Vec3 aim_proj;
static Vec3* tgt_pos = NULL;
static Vec3 interp_tgt_pos;
static float aim_life_ratio = -1;
static float cur_thrust = 0;
static float fade_ratio = -1;
static int manual_fire = 0;
static int non_inverted_y = 0;
static float cur_laser_t = 0;
static int cur_laser_side = -1;
static float cur_nme_t = 0;
static float asteroid_mul_t = 1;
static int cur_sequencer_x = 96;
static int cur_sequencer_y = 96;
static float next_sequencer_t = 0;
static bool waiting_nme_clear = false;
static bool spawn_asteroids = false;
static Vec3 star_proj;

// For camera interpolation
static float src_cam_angle_z, src_cam_angle_x;
static float src_cam_x, src_cam_y;
static float dst_cam_angle_z, dst_cam_angle_x;
static float dst_cam_x, dst_cam_y;
static float interpolation_ratio, interpolation_spd;

// Current texture for rendering
static Texture* cur_tex;
static Vec3* t_light_dir;

// Palette animation
static int ngn_colors[4] = {13, 12, 7, 12};
static int laser_ngn_colors[4] = {3, 11, 7, 11};
static float ngn_col_idx = 0;
static float ngn_laser_col_idx = 0;

// Flare
static int flare_offset = 0;

// Explosion colors
static int explosion_color[4] = {9, 10, 15, 7};

// mem_pos for decoding
static int mem_pos = 0;

// ============================================================================
// Math Functions
// ============================================================================

static void vec3_copy(Vec3* dst, const Vec3* src) {
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

static void vec3_set(Vec3* v, float x, float y, float z) {
    v->x = x;
    v->y = y;
    v->z = z;
}

static void vec3_mul(Vec3* v, float f) {
    v->x *= f;
    v->y *= f;
    v->z *= f;
}

static Vec3 vec3_minus(const Vec3* v0, const Vec3* v1) {
    Vec3 res = {v0->x - v1->x, v0->y - v1->y, v0->z - v1->z};
    return res;
}

static float vec3_dot(const Vec3* v0, const Vec3* v1) {
    return v0->x * v1->x + v0->y * v1->y + v0->z * v1->z;
}

static float vec3_length(const Vec3* v) {
    return sqrtf(vec3_dot(v, v));
}

static void vec3_normalize(Vec3* v) {
    vec3_mul(v, 0.1f);
    float invl = 1.0f / vec3_length(v);
    vec3_mul(v, invl);
}

static void mat_rotx(Mat34* m, float a) {
    float cos_a = cosf(a * 2 * M_PI);
    float sin_a = sinf(a * 2 * M_PI);
    m->m[0] = 1; m->m[1] = 0; m->m[2] = 0; m->m[3] = 0;
    m->m[4] = 0; m->m[5] = cos_a; m->m[6] = sin_a; m->m[7] = 0;
    m->m[8] = 0; m->m[9] = -sin_a; m->m[10] = cos_a; m->m[11] = 0;
}

static void mat_roty(Mat34* m, float a) {
    float cos_a = cosf(a * 2 * M_PI);
    float sin_a = sinf(a * 2 * M_PI);
    m->m[0] = cos_a; m->m[1] = 0; m->m[2] = sin_a; m->m[3] = 0;
    m->m[4] = 0; m->m[5] = 1; m->m[6] = 0; m->m[7] = 0;
    m->m[8] = -sin_a; m->m[9] = 0; m->m[10] = cos_a; m->m[11] = 0;
}

static void mat_rotz(Mat34* m, float a) {
    float cos_a = cosf(a * 2 * M_PI);
    float sin_a = sinf(a * 2 * M_PI);
    m->m[0] = cos_a; m->m[1] = sin_a; m->m[2] = 0; m->m[3] = 0;
    m->m[4] = -sin_a; m->m[5] = cos_a; m->m[6] = 0; m->m[7] = 0;
    m->m[8] = 0; m->m[9] = 0; m->m[10] = 1; m->m[11] = 0;
}

static void mat_translation(Mat34* m, float x, float y, float z) {
    m->m[0] = 1; m->m[1] = 0; m->m[2] = 0; m->m[3] = x;
    m->m[4] = 0; m->m[5] = 1; m->m[6] = 0; m->m[7] = y;
    m->m[8] = 0; m->m[9] = 0; m->m[10] = 1; m->m[11] = z;
}

static void mat_mul(Mat34* res, const Mat34* m0, const Mat34* m1) {
    float r[12];
    r[0] = m0->m[0]*m1->m[0] + m0->m[1]*m1->m[4] + m0->m[2]*m1->m[8];
    r[1] = m0->m[0]*m1->m[1] + m0->m[1]*m1->m[5] + m0->m[2]*m1->m[9];
    r[2] = m0->m[0]*m1->m[2] + m0->m[1]*m1->m[6] + m0->m[2]*m1->m[10];
    r[3] = m0->m[0]*m1->m[3] + m0->m[1]*m1->m[7] + m0->m[2]*m1->m[11] + m0->m[3];

    r[4] = m0->m[4]*m1->m[0] + m0->m[5]*m1->m[4] + m0->m[6]*m1->m[8];
    r[5] = m0->m[4]*m1->m[1] + m0->m[5]*m1->m[5] + m0->m[6]*m1->m[9];
    r[6] = m0->m[4]*m1->m[2] + m0->m[5]*m1->m[6] + m0->m[6]*m1->m[10];
    r[7] = m0->m[4]*m1->m[3] + m0->m[5]*m1->m[7] + m0->m[6]*m1->m[11] + m0->m[7];

    r[8] = m0->m[8]*m1->m[0] + m0->m[9]*m1->m[4] + m0->m[10]*m1->m[8];
    r[9] = m0->m[8]*m1->m[1] + m0->m[9]*m1->m[5] + m0->m[10]*m1->m[9];
    r[10] = m0->m[8]*m1->m[2] + m0->m[9]*m1->m[6] + m0->m[10]*m1->m[10];
    r[11] = m0->m[8]*m1->m[3] + m0->m[9]*m1->m[7] + m0->m[10]*m1->m[11] + m0->m[11];

    memcpy(res->m, r, sizeof(r));
}

static void mat_mul_vec(Vec3* res, const Mat34* m, const Vec3* v) {
    res->x = v->x * m->m[0] + v->y * m->m[1] + v->z * m->m[2];
    res->y = v->x * m->m[4] + v->y * m->m[5] + v->z * m->m[6];
    res->z = v->x * m->m[8] + v->y * m->m[9] + v->z * m->m[10];
}

static void mat_mul_pos(Vec3* res, const Mat34* m, const Vec3* v) {
    mat_mul_vec(res, m, v);
    res->x += m->m[3];
    res->y += m->m[7];
    res->z += m->m[11];
}

static void mat_transpose_rot(Mat34* res, const Mat34* m) {
    res->m[0] = m->m[0];
    res->m[1] = m->m[4];
    res->m[2] = m->m[8];
    res->m[3] = m->m[3];
    res->m[4] = m->m[1];
    res->m[5] = m->m[5];
    res->m[6] = m->m[9];
    res->m[7] = m->m[7];
    res->m[8] = m->m[2];
    res->m[9] = m->m[6];
    res->m[10] = m->m[10];
    res->m[11] = m->m[11];
}

static float normalize_angle(float a) {
    a = fmodf(a, 1.0f);
    if (a > 0.5f) a -= 1.0f;
    if (a < -0.5f) a += 1.0f;
    return a;
}

static float smoothstep(float ratio) {
    return ratio * ratio * (3 - 2 * ratio);
}

static float sym_random(float f) {
    return f - rnd(f * 2);
}

static int get_random_idx(int max) {
    return flr(rnd((float)max));
}

// ============================================================================
// Mesh Decoding
// ============================================================================

static float decode_byte(void) {
    int res = map_memory[mem_pos];
    mem_pos++;
    if (res >= 128) res = res | 0xFF00;
    return (float)((int16_t)res) * 0.5f;
}

static void decode_mesh(Mesh* mesh, float scale) {
    int nb_vert = (int)decode_byte();
    mesh->num_vertices = nb_vert;
    mesh->vertices = (Vec3*)calloc(nb_vert, sizeof(Vec3));
    mesh->projected = (Vec3*)calloc(nb_vert, sizeof(Vec3));

    printf("Decoding mesh: %d vertices at mem_pos=%d\n", nb_vert, mem_pos);

    for (int i = 0; i < nb_vert; i++) {
        mesh->vertices[i].x = decode_byte() * scale;
        mesh->vertices[i].y = decode_byte() * scale;
        mesh->vertices[i].z = decode_byte() * scale;
    }

    int nb_tri = (int)decode_byte();
    mesh->num_triangles = nb_tri;
    mesh->triangles = (Triangle*)calloc(nb_tri, sizeof(Triangle));

    printf("Decoding mesh: %d triangles\n", nb_tri);

    for (int i = 0; i < nb_tri; i++) {
        Triangle* tri = &mesh->triangles[i];

        // For each of the 3 vertices of the triangle:
        // - vertex index (1 byte) - PICO-8 uses 1-based indexing
        // - normal component (1 byte, /63.5)
        // - uv coords (2 bytes)
        tri->tri[0] = (int)decode_byte() - 1;  // Convert to 0-based
        tri->normal.x = decode_byte() / 63.5f;
        tri->uv[0][0] = decode_byte();
        tri->uv[0][1] = decode_byte();

        tri->tri[1] = (int)decode_byte() - 1;  // Convert to 0-based
        tri->normal.y = decode_byte() / 63.5f;
        tri->uv[1][0] = decode_byte();
        tri->uv[1][1] = decode_byte();

        tri->tri[2] = (int)decode_byte() - 1;  // Convert to 0-based
        tri->normal.z = decode_byte() / 63.5f;
        tri->uv[2][0] = decode_byte();
        tri->uv[2][1] = decode_byte();
    }
}

// ============================================================================
// Projection
// ============================================================================

static void transform_pos(Vec3* proj, const Mat34* mat, const Vec3* pos) {
    mat_mul_pos(proj, mat, pos);

    float c = -80.0f / proj->z;

    proj->x = 64.0f + proj->x * c;
    proj->y = 64.0f - proj->y * c;

    if (c > 0 && c <= 10) {
        proj->z = c;
    } else {
        proj->z = 0;
    }
}

// ============================================================================
// Rasterization
// ============================================================================

static void rasterize_flat_tri(Vec3* v0, Vec3* v1, Vec3* v2,
                                float* uv0, float* uv1, float* uv2, float light) {
    float y0 = v0->y;
    float y1 = v1->y;

    float firstline, lastline;

    if (y0 < y1) {
        firstline = floorf(y0 + 0.5f) + 0.5f;
        lastline = floorf(y1 - 0.5f) + 0.5f;
    } else if (y0 == y1) {
        return;
    } else {
        firstline = floorf(y1 + 0.5f) + 0.5f;
        lastline = floorf(y0 - 0.5f) + 0.5f;
    }

    if (firstline < 0.5f) firstline = 0.5f;
    if (lastline > 127.5f) lastline = 127.5f;

    float x0 = v0->x, z0 = v0->z;
    float x1 = v1->x, z1 = v1->z;
    float x2 = v2->x, y2 = v2->y, z2 = v2->z;

    float uv0x = uv0[0], uv0y = uv0[1];
    float uv1x = uv1[0], uv1y = uv1[1];
    float uv2x = uv2[0], uv2y = uv2[1];

    float cb0 = x1 * y2 - x2 * y1;
    float cb1 = x2 * y0 - x0 * y2;

    float d = cb0 + cb1 + x0 * y1 - x1 * y0;
    if (fabsf(d) < 0.001f) return;

    float invdy = 1.0f / (y1 - y0);
    if (!isfinite(invdy)) return;

    int tex_x = cur_tex->x;
    int tex_y = cur_tex->y;
    int tex_lit_x = cur_tex->light_x;

    for (float y = firstline; y <= lastline; y += 1.0f) {
        float coef = (y - y0) * invdy;
        float xfirst = floorf(x0 + coef * (x1 - x0) + 0.48f) + 0.5f;
        float xlast = floorf(x0 + coef * (x2 - x0) - 0.48f) + 0.5f;

        if (xfirst < 0.5f) xfirst = 0.5f;
        if (xlast > 127.5f) xlast = 127.5f;

        float x0y = x0 * y;
        float x1y = x1 * y;
        float x2y = x2 * y;

        for (float x = xfirst; x <= xlast; x += 1.0f) {
            float b0 = (cb0 + x * y1 + x2y - x * y2 - x1y) / d;
            float b1 = (cb1 + x * y2 + x0y - x * y0 - x2y) / d;
            float b2 = 1 - b0 - b1;

            b0 *= z0;
            b1 *= z1;
            b2 *= z2;

            float d2 = b0 + b1 + b2;
            if (fabsf(d2) < 0.001f) continue;

            float uvx = (b0 * uv0x + b1 * uv1x + b2 * uv2x) / d2;
            float uvy = (b0 * uv0y + b1 * uv1y + b2 * uv2y) / d2;

            int offset_x = tex_x;
            int dither_val = sget(((int)x) % 8, 56 + ((int)y) % 8);
            if (light <= 7 + dither_val * 0.125f) offset_x += tex_lit_x;

            pset((int)x, (int)y, sget((int)uvx + offset_x, (int)uvy + tex_y));
        }
    }
}

static void rasterize_tri(int index, Triangle* tris, Vec3* projs) {
    Triangle* tri = &tris[index];

    // Validate indices and texture
    if (tri->tri[0] < 0 || tri->tri[1] < 0 || tri->tri[2] < 0) return;
    if (cur_tex == NULL) return;

    Vec3* v0 = &projs[tri->tri[0]];
    Vec3* v1 = &projs[tri->tri[1]];
    Vec3* v2 = &projs[tri->tri[2]];

    float x0 = v0->x, y0 = v0->y;
    float x1 = v1->x, y1 = v1->y;
    float x2 = v2->x, y2 = v2->y;

    // Backface cull
    float nz = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
    if (nz < 0) return;

    float* uv0 = tri->uv[0];
    float* uv1 = tri->uv[1];
    float* uv2 = tri->uv[2];

    // Sort by Y
    Vec3 *tv0 = v0, *tv1 = v1, *tv2 = v2;
    float *tuv0 = uv0, *tuv1 = uv1, *tuv2 = uv2;

    if (tv1->y < tv0->y) { Vec3* t = tv1; tv1 = tv0; tv0 = t; float* tu = tuv1; tuv1 = tuv0; tuv0 = tu; }
    if (tv2->y < tv0->y) { Vec3* t = tv2; tv2 = tv0; tv0 = t; float* tu = tuv2; tuv2 = tuv0; tuv0 = tu; }
    if (tv2->y < tv1->y) { Vec3* t = tv2; tv2 = tv1; tv1 = t; float* tu = tuv2; tuv2 = tuv1; tuv1 = tu; }

    y0 = tv0->y; y1 = tv1->y; y2 = tv2->y;
    x0 = tv0->x;
    float z0 = tv0->z, z2 = tv2->z;

    if (y0 == y2) return;

    float light = 15.0f * vec3_dot(t_light_dir, &tri->normal);

    float c = (y1 - y0) / (y2 - y0);
    Vec3 v3 = {x0 + c * (tv2->x - x0), y1, z0 + c * (z2 - z0)};

    float b0 = (1 - c) * z0;
    float b1 = c * z2;
    float invd = (b0 + b1 > 0.001f) ? 1.0f / (b0 + b1) : 0;

    float uv3[2] = {
        (b0 * tuv0[0] + b1 * tuv2[0]) * invd,
        (b0 * tuv0[1] + b1 * tuv2[1]) * invd
    };

    if (tv1->x <= v3.x) {
        rasterize_flat_tri(tv0, tv1, &v3, tuv0, tuv1, uv3, light);
        rasterize_flat_tri(tv2, tv1, &v3, tuv2, tuv1, uv3, light);
    } else {
        rasterize_flat_tri(tv0, &v3, tv1, tuv0, uv3, tuv1, light);
        rasterize_flat_tri(tv2, &v3, tv1, tuv2, uv3, tuv1, light);
    }
}

static int tri_compare(const void* a, const void* b) {
    const Triangle* ta = (const Triangle*)a;
    const Triangle* tb = (const Triangle*)b;
    if (ta->z < tb->z) return -1;
    if (ta->z > tb->z) return 1;
    return 0;
}

static void sort_tris(Triangle* tris, int num, Vec3* projs) {
    for (int i = 0; i < num; i++) {
        Triangle* tri = &tris[i];
        tri->z = projs[tri->tri[0]].z + projs[tri->tri[1]].z + projs[tri->tri[2]].z;
    }
    qsort(tris, num, sizeof(Triangle), tri_compare);
}

// ============================================================================
// Game Initialization
// ============================================================================

static void init_ship(void) {
    mem_pos = 0;
    decode_mesh(&ship_mesh, 1.0f);

    ship_tex.x = 0;
    ship_tex.y = 96;
    ship_tex.light_x = 48;

    ship_tex_laser_lit.x = 0;
    ship_tex_laser_lit.y = 64;
    ship_tex_laser_lit.light_x = 48;
}

static void init_nme(void) {
    for (int i = 0; i < 4; i++) {
        decode_mesh(&nme_meshes[i], nme_scale[i]);
        nme_tex[i].x = i * 32;
        nme_tex[i].y = 32;
        nme_tex[i].light_x = 16;
    }

    nme_tex_hit.x = 96;
    nme_tex_hit.y = 64;
    nme_tex_hit.light_x = 16;
}

static void init_single_trail(Trail* trail, float z) {
    vec3_set(&trail->pos0, sym_random(100) + ship_x, sym_random(100) + ship_y, z);
    trail->spd = (2.5f + rnd(5)) * game_spd;
    trail->col = flr(rnd(4)) + 1;
}

static void init_trail(void) {
    for (int i = 0; i < MAX_TRAILS; i++) {
        init_single_trail(&trails[i], sym_random(150));
    }
}

static void init_single_bg(Background* bg, float z) {
    float a = rnd(1);
    float r = 150 + rnd(150);
    vec3_set(&bg->pos, r * cosf(a * 2 * M_PI), r * sinf(a * 2 * M_PI), z);
    bg->spd = 0.05f + rnd(0.05f);
    if (flr(rnd(6)) == 0) {
        bg->index = 8 + (int)rnd(8);
    } else {
        bg->index = -bg_color[get_random_idx(3)];
    }
}

static void init_bg(void) {
    for (int i = 0; i < MAX_BGS; i++) {
        init_single_bg(&bgs[i], sym_random(400));
    }
}

static void init_main(void) {
    cur_mode = 0;
    cam_angle_z = -0.4f;
    cam_angle_x = (flr(rnd(2)) * 2 - 1) * (0.03f + rnd(0.1f));

    ship_x = 0;
    ship_y = 0;
    cam_x = 0;
    cam_y = 0;
    ship_spd_x = 0;
    ship_spd_y = 0;
    life = 4;
    barrel_cur_t = -1;
    num_enemies = 0;
    num_lasers = 0;
    num_nme_lasers = 0;
    hit_t = -1;
    laser_on = false;
    nb_nme_ship = 0;
    aim_z = -200;
    cur_thrust = 0;
    roll_f = 0;
    pitch_f = 0;
    global_t = 0;
    asteroid_mul_t = 1;
    cur_sequencer_x = 96;
    cur_sequencer_y = 96;
    next_sequencer_t = 0;
    waiting_nme_clear = false;
    spawn_asteroids = false;
    game_spd = 1;
    cam_depth = 22.5f;
    cur_nme_t = 0;
    best_score = dget(0);
}

// ============================================================================
// Laser Management
// ============================================================================

static Laser* spawn_laser(Laser* lasers, int* count, Vec3 pos) {
    if (*count >= MAX_LASERS) return NULL;
    Laser* laser = &lasers[*count];
    vec3_copy(&laser->pos0, &pos);
    (*count)++;
    return laser;
}

static void remove_laser(Laser* lasers, int* count, int idx) {
    if (idx < *count - 1) {
        lasers[idx] = lasers[*count - 1];
    }
    (*count)--;
}

// ============================================================================
// Enemy Management
// ============================================================================

static Enemy* spawn_nme(int type, Vec3 pos) {
    if (num_enemies >= MAX_ENEMIES) return NULL;
    Enemy* nme = &enemies[num_enemies];
    memset(nme, 0, sizeof(Enemy));
    vec3_copy(&nme->pos, &pos);
    nme->type = type;
    nme->proj = (Vec3*)calloc(nme_meshes[type - 1].num_vertices, sizeof(Vec3));
    nme->life = nme_life[type - 1];
    nme->hit_t = -1;
    num_enemies++;
    return nme;
}

static void spawn_nme_ship(int type) {
    nb_nme_ship++;
    next_sequencer_t = global_t + 0.25f;
    // type 2/3/4 -> index 0/1/2
    float desc_bounds = nme_bounds[type - 2] * 2;
    Vec3 pos = {
        mid_f(-100, sym_random(50) + ship_x, 100),
        mid_f(-100, sym_random(50) + ship_y, 100),
        desc_bounds - 200
    };
    Enemy* nme = spawn_nme(type, pos);
    if (nme) {
        vec3_set(&nme->spd, 0, 0, 8);
        vec3_copy(&nme->waypoint, &nme->pos);
        nme->waypoint.z = desc_bounds;
    }
}

// ============================================================================
// Collision
// ============================================================================

static void hit_ship(Vec3* pos, float sqr_size) {
    if (hit_t == -1 && barrel_cur_t == -1) {
        float dx = (pos->x - ship_x) * 0.2f;
        float dy = (pos->y - ship_y) * 0.2f;
        float sqrd = dx * dx + dy * dy;
        if (sqrd < sqr_size) {
            float n = 1.0f / sqrtf(sqrd + 0.001f);
            dx *= n;
            dy *= n;
            roll_f += dx * 0.05f;
            pitch_f -= dy * 0.02f;
            hit_t = 0;
            vec3_copy(&hit_pos, pos);
            life--;
            sfx(2, 1);
            if (life == 0) {
                fade_ratio = 0;
                sfx(7, 2);
            }
        }
    }
}

// ============================================================================
// Update Functions
// ============================================================================

static void update_enemies(void) {
    for (int i = 0; i < num_enemies; i++) {
        Enemy* nme = &enemies[i];
        nme->pos.x += nme->spd.x * game_spd;
        nme->pos.y += nme->spd.y * game_spd;
        nme->pos.z += nme->spd.z * game_spd;
        nme->rot_x += nme->rot_x_spd;
        nme->rot_y += nme->rot_y_spd;

        int type = nme->type;

        // type 1 = asteroid, type 2/3/4 = enemy ships
        if (type > 1) {
            int sub_type = type - 1;  // 1, 2, or 3 for ship types
            if (sub_type >= 1 && sub_type <= 3) {
                float desc_bounds = nme_bounds[sub_type - 1];
                float desc_spd = nme_spd[sub_type - 1];

                Vec3 dir = vec3_minus(&nme->waypoint, &nme->pos);
                vec3_mul(&dir, 0.1f);
                float dist = vec3_dot(&dir, &dir);

                if (dist < game_spd * game_spd || nme->hit_t == 0) {
                    vec3_set(&nme->waypoint, sym_random(100), sym_random(100),
                             desc_bounds - rnd(-desc_bounds));
                }

                vec3_normalize(&dir);
                nme->spd.x += dir.x * desc_spd * 0.1f;
                nme->spd.y += dir.y * desc_spd * 0.1f;
                nme->spd.z += dir.z * desc_spd * 0.1f;

                if (nme->pos.z >= desc_bounds * 2) {
                    float spd_len = vec3_length(&nme->spd);
                    if (spd_len > desc_spd) {
                        vec3_mul(&nme->spd, desc_spd / spd_len);
                    }
                    nme->rot_x = -0.08f * nme->spd.y;
                    nme->rot_y = -nme_rot[sub_type - 1] * nme->spd.x;

                    // Enemy laser logic
                    int nb_lasers = type - 2;  // type 2=0 lasers (shoots differently), type 3=1 laser, type 4=2 lasers

                    // Type 4 (boss) or any hit enemy starts shooting immediately
                    if ((type == 4 || nme->hit_t == 0) && nme->laser_t < 0) {
                        nme->laser_t = 0;
                    }

                    nme->laser_t += 1;

                    if (nme->laser_t > nme->stop_laser_t) {
                        nme->laser_t = -(60 + rnd(60)) / game_spd;
                        nme->stop_laser_t = 60 + rnd(60);
                        float c = -0.5f * nme->pos.z / game_spd;
                        for (int j = 0; j < nb_lasers && j < 2; j++) {
                            nme->laser_offset_x[j] = sym_random(30) + ship_spd_x * c;
                            nme->laser_offset_y[j] = sym_random(30) + ship_spd_y * c;
                        }
                    }

                    float laser_t_val = nme->laser_t;
                    float t = 6.0f / game_spd;
                    if (laser_t_val > 0) {
                        nme->next_laser_t += 1;

                        if (nme->next_laser_t >= t) {
                            nme->next_laser_t -= t;

                            if (type != 2) {
                                // Type 3 and 4: aimed lasers
                                float ratio = cosf(laser_t_val / 120.0f * 2 * M_PI);

                                for (int j = 0; j < nb_lasers && j < 2; j++) {
                                    Vec3 laser_pos;
                                    Mesh* mesh = &nme_meshes[type - 1];
                                    if (j < mesh->num_vertices) {
                                        laser_pos.x = nme->pos.x + mesh->vertices[j].x;
                                        laser_pos.y = nme->pos.y;
                                        laser_pos.z = nme->pos.z + mesh->vertices[j].z;
                                    } else {
                                        laser_pos = nme->pos;
                                    }

                                    Laser* laser = spawn_laser(nme_lasers, &num_nme_lasers, laser_pos);
                                    if (laser) {
                                        Vec3 target = {
                                            ship_x + nme->laser_offset_x[j] * ratio + sym_random(5),
                                            ship_y + nme->laser_offset_y[j] * ratio + sym_random(5),
                                            0
                                        };
                                        Vec3 dir = vec3_minus(&target, &laser_pos);
                                        vec3_mul(&dir, 0.1f);
                                        float len = vec3_length(&dir);
                                        float v = (len > 0.001f) ? 2.0f * game_spd / len : 2.0f * game_spd;
                                        laser->spd.x = dir.x * v;
                                        laser->spd.y = dir.y * v;
                                        laser->spd.z = dir.z * v;
                                    }
                                }
                            } else {
                                // Type 2: straight laser with random spread
                                Vec3 laser_pos = {nme->pos.x, nme->pos.y, nme->pos.z + 12};
                                Laser* laser = spawn_laser(nme_lasers, &num_nme_lasers, laser_pos);
                                if (laser) {
                                    laser->spd.x = sym_random(0.05f);
                                    laser->spd.y = sym_random(0.05f);
                                    laser->spd.z = 2.0f * game_spd;
                                }
                            }
                        }
                    }
                }
            }
        }

        bool del = false;
        if (nme->pos.z > 0) {
            hit_ship(&nme->pos, 2.5f);
            del = true;
        }

        if (nme->life <= 0) {
            nme->life--;
            if (nme->life < -15) del = true;
        }

        if (nme->hit_t > -1) {
            nme->hit_t++;
            if (nme->hit_t > 5) nme->hit_t = -1;
        }

        if (del) {
            if (nme->type > 1) nb_nme_ship--;  // Only count ships, not asteroids
            if (nme->proj) free(nme->proj);
            enemies[i] = enemies[num_enemies - 1];
            num_enemies--;
            i--;
        }
    }

    cur_nme_t--;
    if (spawn_asteroids && cur_nme_t <= 0) {
        cur_nme_t = (30 + rnd(60)) * asteroid_mul_t / game_spd;
        float posx = mid_f(-100, 10 * ship_spd_x + ship_x + sym_random(30), 100);
        float posy = mid_f(-100, 10 * ship_spd_y + ship_y + sym_random(30), 100);
        Vec3 pos = {posx, posy, -50};
        Enemy* nme = spawn_nme(1, pos);  // type 1 = asteroid
        if (nme) {
            vec3_set(&nme->spd,
                mid_f((-100 - posx) * 0.005f, sym_random(0.25f), (100 - posx) * 0.005f),
                mid_f((-100 - posy) * 0.005f, sym_random(0.25f), (100 - posy) * 0.005f),
                0.25f);
            nme->rot_x_spd = sym_random(0.015f);
            nme->rot_y_spd = sym_random(0.015f);
        }
    }

    // Sort enemies by distance
    for (int i = 1; i < num_enemies; i++) {
        Enemy nme = enemies[i];
        int j = i - 1;
        while (j >= 0 && nme.pos.z > enemies[j].pos.z) {
            enemies[j + 1] = enemies[j];
            j--;
        }
        enemies[j + 1] = nme;
    }
}

static void update_nme_lasers(void) {
    for (int i = 0; i < num_nme_lasers; i++) {
        Laser* laser = &nme_lasers[i];
        vec3_copy(&laser->pos1, &laser->pos0);
        laser->pos0.x += laser->spd.x;
        laser->pos0.y += laser->spd.y;
        laser->pos0.z += laser->spd.z;

        if (laser->pos0.z >= 0) {
            hit_ship(&laser->pos0, 1.5f);
            hit_ship(&laser->pos1, 1.5f);
            remove_laser(nme_lasers, &num_nme_lasers, i);
            i--;
        }
    }
}

static void update_lasers(void) {
    cur_laser_t++;
    laser_spawned = false;

    if (laser_on && cur_laser_t > 2) {
        cur_laser_t = 0;
        laser_spawned = true;
        Vec3 pos = {(float)cur_laser_side, -1.5f, -8.0f};
        Vec3 world_pos;
        mat_mul_pos(&world_pos, &ship_mat, &pos);
        spawn_laser(lasers, &num_lasers, world_pos);
        cur_laser_side = -cur_laser_side;
    }

    for (int i = 0; i < num_lasers; i++) {
        Laser* laser = &lasers[i];
        vec3_copy(&laser->pos1, &laser->pos0);
        laser->pos0.z -= 5;

        if (laser->pos0.z <= -200) {
            remove_laser(lasers, &num_lasers, i);
            i--;
        }
    }
}

static void update_trail(void) {
    for (int i = 0; i < MAX_TRAILS; i++) {
        Trail* trail = &trails[i];
        if (trail->pos0.z >= 150) {
            init_single_trail(trail, -150);
        }
        vec3_copy(&trail->pos1, &trail->pos0);
        trail->pos0.z += trail->spd;
    }

    for (int i = 0; i < MAX_BGS; i++) {
        Background* bg = &bgs[i];
        bg->pos.z += bg->spd * game_spd;
        if (bg->pos.z >= 400) {
            init_single_bg(bg, -400);
        }
    }
}

static void update_collisions(void) {
    int laser_idx = 0;
    int nme_idx = num_enemies - 1;

    while (laser_idx < num_lasers && nme_idx >= 0) {
        Vec3* laser_pos0 = &lasers[laser_idx].pos0;
        Vec3* laser_pos1 = &lasers[laser_idx].pos1;
        Enemy* nme = &enemies[nme_idx];
        float nme_z = nme->pos.z;

        if (nme_z > laser_pos1->z) {
            laser_idx++;
        } else {
            if (nme->life > 0 && nme_z >= laser_pos0->z) {
                float dx = (laser_pos0->x - nme->pos.x) * 0.2f;
                float dy = (laser_pos0->y - nme->pos.y) * 0.2f;

                float radius = nme_radius[nme->type - 1];
                if (dx * dx + dy * dy <= radius * radius * 0.04f) {
                    nme->life--;
                    if (nme->life == 0) {
                        nme->hit_t = -1;
                        sfx(2, 1);
                        score += nme_score[nme->type - 1];
                    } else {
                        vec3_copy(&nme->hit_pos, laser_pos0);
                        nme->hit_t = 0;
                        sfx(5, 1);
                    }
                    remove_laser(lasers, &num_lasers, laser_idx);
                    continue;
                }
            }
            nme_idx--;
        }
    }
}

// ============================================================================
// Main Update
// ============================================================================

static void game_update(void) {
    float dx = 0, dy = 0;
    if (btn(0)) dx -= 1;
    if (btn(1)) dx += 1;
    if (btn(2)) dy -= 1;
    if (btn(3)) dy += 1;

    if (cur_mode == 2) {
        global_t += 0.033f;
        game_spd = 1 + global_t * 0.002f;

        if (dx == 0 && dy == 0) cur_thrust = 0;
        else cur_thrust = fminf(0.5f, cur_thrust + 0.1f);
        float mul_spd = cur_thrust;

        if (non_inverted_y != 0) dy = -dy;

        if (barrel_cur_t > -1 || life <= 0) {
            dx = 0;
            dy = 0;
        }

        if (btn(5) && dx != 0 && barrel_cur_t == -1) {
            sfx(1, 0);
            barrel_cur_t = 0;
            barrel_dir = (int)sgn(dx);
        }

        if (barrel_cur_t != -1) {
            barrel_cur_t++;
            if (barrel_cur_t >= 0) {
                dx = barrel_dir * 9;
                dy = 0;
                mul_spd = 0.1f;
                if (barrel_cur_t > 5) {
                    barrel_cur_t = -20;
                }
            }
        }

        if (fabsf(ship_x) > 100) dx = -sgn(ship_x) * 0.4f;
        if (fabsf(ship_y) > 100) dy = -sgn(ship_y) * 0.4f;

        ship_spd_x += dx * mul_spd;
        ship_spd_y += dy * mul_spd;

        roll_f -= 0.003f * dx;
        pitch_f += 0.0008f * dy;

        ship_spd_x *= 0.85f;
        ship_spd_y *= 0.85f;

        ship_x += ship_spd_x;
        ship_y += ship_spd_y;

        cam_x = 1.05f * ship_x;
        cam_y = ship_y + 11.5f;

        if (hit_t != -1) {
            cam_x += sym_random(2);
            cam_y += sym_random(2);
        } else if (life <= 0) {
            hit_t = 0;
            vec3_set(&hit_pos, ship_x, ship_y, 0);
            sfx(2, 1);
        }

        cam_angle_z = cam_x * 0.0005f;
        cam_angle_x = cam_y * 0.0003f;

        // Sequencer
        if (waiting_nme_clear) {
            if (nb_nme_ship == 0) {
                next_sequencer_t = 0;
                waiting_nme_clear = false;
            } else {
                next_sequencer_t = 32767;
            }
        }

        if (global_t >= next_sequencer_t) {
            int value = sget(cur_sequencer_x, cur_sequencer_y);
            cur_sequencer_x++;
            if (cur_sequencer_x > 127) {
                cur_sequencer_x = 96;
                cur_sequencer_y++;
            }

            if (value == 1) spawn_nme_ship(3);       // Medium ship
            else if (value == 13) { spawn_nme_ship(4); sfx(6, 2); }  // Boss
            else if (value == 2) spawn_nme_ship(2);  // Small ship
            else if (value == 6) { spawn_asteroids = true; asteroid_mul_t = 1; }
            else if (value == 7) { spawn_asteroids = true; asteroid_mul_t = 0.5f; }
            else if (value == 5) spawn_asteroids = false;
            else if (value == 10) next_sequencer_t = global_t + 1;
            else if (value == 9) next_sequencer_t = global_t + 10;
            else if (value == 11) waiting_nme_clear = true;
            else { cur_sequencer_x = 96; cur_sequencer_y = 96; }
        }

    } else if (cur_mode == 0) {
        if (dx == 0 && dy == 0) dx = -0.25f;
        cam_angle_z += dx * 0.007f;
        cam_angle_x -= dy * 0.007f;

        if (btnp(5)) {
            printf("Button 5 pressed - going to options (mode 3)\n");
            cur_mode = 3;
            manual_fire = dget(1);
            non_inverted_y = dget(2);
        }
    } else if (cur_mode == 3) {
        cam_angle_z -= 0.00175f;

        if (btnp(0) || btnp(1)) {
            manual_fire = 1 - manual_fire;
            dset(1, manual_fire);
        }
        if (btnp(2) || btnp(3)) {
            non_inverted_y = 1 - non_inverted_y;
            dset(2, non_inverted_y);
        }

        if (btnp(5)) {
            printf("Button 5 pressed - starting game transition (mode 1)\n");
            src_cam_angle_z = normalize_angle(cam_angle_z);
            src_cam_angle_x = normalize_angle(cam_angle_x);
            src_cam_x = cam_x;
            src_cam_y = cam_y;

            dst_cam_x = 1.05f * ship_x;
            dst_cam_y = ship_y + 11.5f;
            dst_cam_angle_z = dst_cam_x * 0.0005f;
            dst_cam_angle_x = dst_cam_y * 0.0003f;

            Vec3 src = {src_cam_x, src_cam_y, 26};
            Vec3 dst = {dst_cam_x, dst_cam_y, 22.5f};
            Vec3 diff = vec3_minus(&src, &dst);
            float len = vec3_length(&diff);
            interpolation_spd = (len > 0.01f) ? 0.25f / len : 1.0f;
            interpolation_ratio = 0;
            cur_mode = 1;
        }
    } else {
        interpolation_ratio += interpolation_spd;

        if (interpolation_ratio >= 1) {
            printf("Game started! (mode 2)\n");
            cur_mode = 2;
            score = 0;
        } else {
            float smoothed_ratio = smoothstep(interpolation_ratio);
            cam_x = src_cam_x + smoothed_ratio * (dst_cam_x - src_cam_x);
            cam_y = src_cam_y + smoothed_ratio * (dst_cam_y - src_cam_y);
            cam_depth = 22.5f + smoothed_ratio * 3.5f;
            cam_angle_z = src_cam_angle_z + smoothed_ratio * (dst_cam_angle_z - src_cam_angle_z);
            cam_angle_x = src_cam_angle_x + smoothed_ratio * (dst_cam_angle_x - src_cam_angle_x);
        }
    }

    bool old_laser_on = laser_on;
    laser_on = (cur_mode != 2 && btn(4)) ||
               ((btn(4) || (manual_fire != 1 && tgt_pos)) && barrel_cur_t == -1 && hit_t == -1);

    if (laser_on != old_laser_on) {
        if (laser_on) sfx(0, 0); else sfx(-2, 0);
    }

    // Build camera matrix
    Mat34 trans, rot;
    mat_translation(&trans, 0, 0, -cam_depth);
    mat_rotx(&rot, cam_angle_x);
    mat_mul(&cam_mat, &trans, &rot);
    mat_roty(&rot, cam_angle_z);
    mat_mul(&cam_mat, &cam_mat, &rot);
    mat_translation(&trans, -cam_x, -cam_y, 0);
    mat_mul(&cam_mat, &cam_mat, &trans);

    // Roll/pitch noise
    cur_noise_t++;
    float noise_attenuation = cosf(mid_f(-0.25f, roll_angle * 1.2f, 0.25f) * 2 * M_PI);

    if (cur_noise_t > tgt_noise_t) {
        old_noise_roll = cur_noise_roll;
        old_noise_pitch = cur_noise_pitch;
        cur_noise_t = 0;

        float new_roll_sign = -sgn(cur_noise_roll);
        if (new_roll_sign == 0) new_roll_sign = 1;

        cur_noise_roll = new_roll_sign * (0.01f + rnd(0.03f));
        tgt_noise_t = (60 + rnd(40)) * noise_attenuation * fabsf(cur_noise_roll - old_noise_roll) * 10;
        cur_noise_pitch = sym_random(0.01f);
    }

    float noise_ratio = smoothstep(tgt_noise_t > 0 ? cur_noise_t / tgt_noise_t : 0);

    roll_f -= roll_angle * 0.02f;
    roll_spd = roll_spd * 0.8f + roll_f;
    roll_angle += roll_spd;

    pitch_f -= pitch_angle * 0.02f;
    pitch_spd = pitch_spd * 0.8f + pitch_f;
    pitch_angle += pitch_spd;

    roll_f = 0;
    pitch_f = 0;

    float noise_roll = noise_attenuation * (old_noise_roll + noise_ratio * (cur_noise_roll - old_noise_roll));
    float noise_pitch = noise_attenuation * (old_noise_pitch + noise_ratio * (cur_noise_pitch - old_noise_pitch));

    roll_angle = normalize_angle(roll_angle);

    mat_translation(&ship_pos_mat, ship_x, ship_y, 0);
    mat_rotx(&rot, normalize_angle(pitch_angle + noise_pitch));
    mat_mul(&ship_mat, &ship_pos_mat, &rot);
    mat_rotz(&rot, normalize_angle(roll_angle + noise_roll));
    mat_mul(&ship_mat, &ship_mat, &rot);

    mat_transpose_rot(&inv_ship_mat, &ship_mat);

    update_trail();
    if (cur_mode == 2) {
        update_enemies();
    }

    update_lasers();
    update_nme_lasers();
    update_collisions();

    if (hit_t != -1) {
        hit_t++;
        if (hit_t > 15) hit_t = -1;
    }

    mat_mul(&ship_mat, &cam_mat, &ship_mat);
    mat_mul(&ship_pos_mat, &cam_mat, &ship_pos_mat);

    mat_rotx(&light_mat, 0.14f);
    mat_roty(&rot, 0.34f + global_t * 0.003f);
    mat_mul(&light_mat, &light_mat, &rot);
    Vec3 light_src = {0, 0, -1};
    mat_mul_vec(&light_dir, &light_mat, &light_src);

    mat_mul_vec(&ship_light_dir, &inv_ship_mat, &light_dir);

    if (fade_ratio >= 0) {
        if (cur_mode == 2) fade_ratio += 2;
        else fade_ratio -= 2;
        if (fade_ratio >= 100) {
            if (score > best_score) {
                best_score = score;
                dset(0, best_score);
            }
            init_main();
        }
    }
}

// ============================================================================
// Rendering
// ============================================================================

static void transform_vert(void) {
    for (int i = 0; i < ship_mesh.num_vertices; i++) {
        transform_pos(&ship_mesh.projected[i], &ship_mat, &ship_mesh.vertices[i]);
    }

    Vec3 aim_pos = {ship_x, ship_y - 1.5f, aim_z};
    transform_pos(&aim_proj, &cam_mat, &aim_pos);

    float auto_aim_dist = 30;
    tgt_pos = NULL;
    aim_life_ratio = -1;

    if (cur_mode == 2) {
        float aim_x = aim_proj.x;
        float aim_y = aim_proj.y;

        for (int i = 0; i < num_enemies; i++) {
            Enemy* nme = &enemies[i];

            Mat34 nme_mat, nme_rot_x, nme_rot_z, inv_nme_mat;
            mat_translation(&nme_mat, nme->pos.x, nme->pos.y, nme->pos.z);
            mat_rotx(&nme_rot_x, nme->rot_x);
            mat_mul(&nme_mat, &nme_mat, &nme_rot_x);
            mat_rotz(&nme_rot_z, nme->rot_y);
            mat_mul(&nme_mat, &nme_mat, &nme_rot_z);

            mat_transpose_rot(&inv_nme_mat, &nme_mat);
            mat_mul_vec(&nme->light_dir, &inv_nme_mat, &light_dir);

            Mat34 final_nme_mat;
            mat_mul(&final_nme_mat, &cam_mat, &nme_mat);

            Mesh* mesh = &nme_meshes[nme->type - 1];
            for (int j = 0; j < mesh->num_vertices; j++) {
                transform_pos(&nme->proj[j], &final_nme_mat, &mesh->vertices[j]);
            }

            if (nme->life > 0) {
                float dx = (nme->proj[0].x - aim_x) * 0.1f;
                float dy = (nme->proj[0].y - aim_y) * 0.1f;
                float sqr_dist = dx * dx + dy * dy;
                if (sqr_dist < auto_aim_dist) {
                    auto_aim_dist = sqr_dist;
                    tgt_pos = &nme->pos;
                    float laser_t = -game_spd * nme->pos.z / 5;
                    interp_tgt_pos.x = nme->pos.x + nme->spd.x * laser_t;
                    interp_tgt_pos.y = nme->pos.y + nme->spd.y * laser_t;
                    interp_tgt_pos.z = fminf(0, nme->pos.z + nme->spd.z * laser_t);
                    if (nme->type != 1) {  // Not asteroid
                        aim_life_ratio = (float)nme->life / nme_life[nme->type - 1];
                    }
                }
            }
        }
    }

    float tgt_z = -200;
    if (tgt_pos) tgt_z = tgt_pos->z;
    aim_z += (tgt_z - aim_z) * 0.2f;

    Vec3 star_pos = {light_mat.m[2] * 100, light_mat.m[6] * 100, light_mat.m[10] * 100};
    transform_pos(&star_proj, &ship_pos_mat, &star_pos);
}

static void draw_explosion(Vec3* proj, float size) {
    float invz = proj->z;
    int col = explosion_color[get_random_idx(4)];
    circfill((int)(proj->x + sym_random(size * 0.5f) * invz),
             (int)(proj->y + sym_random(size * 0.5f) * invz),
             (int)(invz * (size + rnd(size))), col);
}

static void print_3d(const char* str, int x, int y) {
    print_str(str, x + 2, y + 2, 1);
    print_str(str, x + 1, y + 1, 13);
    print_str(str, x, y, 7);
}

static void draw_lasers(Laser* in_lasers, int count, int col) {
    Vec3 p0, p1;
    color(col);

    for (int i = 0; i < count; i++) {
        Laser* laser = &in_lasers[i];
        transform_pos(&p0, &cam_mat, &laser->pos0);
        transform_pos(&p1, &cam_mat, &laser->pos1);

        if (p0.z > 0 && p1.z > 0) {
            line((int)p0.x, (int)p0.y, (int)p1.x, (int)p1.y, col);
        }
    }
}

static void set_ngn_pal(void) {
    ngn_col_idx = fmodf(ngn_col_idx + 1, 4);
    ngn_laser_col_idx = fmodf(ngn_laser_col_idx + 0.2f, 4);

    pal(12, ngn_colors[(int)ngn_col_idx]);

    int index = (int)ngn_laser_col_idx;
    pal(8, laser_ngn_colors[index]);
    pal(14, laser_ngn_colors[(index + 1) % 4]);
    pal(15, laser_ngn_colors[(index + 2) % 4]);
}

static void draw_lens_flare(void) {
    if (pget((int)star_proj.x, (int)star_proj.y) != 7) return;

    float vx = 64 - star_proj.x;
    float vy = 64 - star_proj.y;

    // Simple flare circles
    float factors[] = {-0.3f, 0.4f, 0.5f, 0.9f, 1.0f};
    int sizes[] = {3, 4, 5, 3, 4};

    for (int i = 0; i < 5; i++) {
        int px = (int)(60 + vx * factors[i]);
        int py = (int)(60 + vy * factors[i]);
        spr(40 + (i % 4), px, py, 1, 1);
    }

    flare_offset = 1 - flare_offset;
}

static void game_draw(void) {
    Vec3 p0, p1;

    cls();
    transform_vert();

    // Draw backgrounds
    for (int i = 0; i < MAX_BGS; i++) {
        Background* bg = &bgs[i];
        transform_pos(&p0, &ship_pos_mat, &bg->pos);

        if (p0.z > 0) {
            int index = bg->index;
            if (index > 0) {
                spr(index + 16 * flr(rnd(2)), (int)p0.x, (int)p0.y, 1, 1);
            } else {
                int col = 7;
                if (rnd(1) > 0.5f) col = -index;
                pset((int)p0.x, (int)p0.y, col);
            }
        }
    }

    // Draw sun
    bool star_visible = star_proj.z > 0 && star_proj.x >= 0 && star_proj.x < 128 &&
                        star_proj.y >= 0 && star_proj.y < 128;
    if (star_visible) {
        int index = 32 + flr(rnd(4)) * 2;
        spr(index, (int)star_proj.x - 7, (int)star_proj.y - 7, 2, 2);
    }

    // Draw trails
    float trail_color_coef = 0.45f * 5;
    for (int i = 0; i < MAX_TRAILS; i++) {
        Trail* trail = &trails[i];
        transform_pos(&p0, &cam_mat, &trail->pos0);
        transform_pos(&p1, &cam_mat, &trail->pos1);

        if (p0.z > 0 && p1.z > 0) {
            int index = (int)mid_f((float)trail->col, trail_color_coef / p0.z + 1, 5) - 1;
            if (index < 0) index = 0;
            if (index > 4) index = 4;
            line((int)p0.x, (int)p0.y, (int)p1.x, (int)p1.y, trail_color[index]);
        }
    }

    // Draw enemies
    if (cur_mode == 2) {
        for (int i = num_enemies - 1; i >= 0; i--) {
            Enemy* nme = &enemies[i];
            Mesh* mesh = &nme_meshes[nme->type - 1];
            cur_tex = &nme_tex[nme->type - 1];

            if (nme->life < 0 || nme->hit_t > -1) {
                if (nme->life < 0) {
                    float ratio = 0.5f + (15.0f + nme->life) / 30.0f;
                    float size = ratio * nme_radius[nme->type - 1] * 0.8f;
                    if ((-nme->life) % 2 == 0) cur_tex = &nme_tex_hit;
                    for (int j = 0; j < 3; j++) {
                        int idx = get_random_idx(mesh->num_vertices);
                        draw_explosion(&nme->proj[idx], size);
                    }
                } else {
                    float ratio = 0.5f + (6.0f - nme->hit_t) / 12.0f;
                    float size = ratio * 3;
                    if (nme->hit_t % 2 == 0) cur_tex = &nme_tex_hit;
                    transform_pos(&p0, &cam_mat, &nme->hit_pos);
                    draw_explosion(&p0, size);
                }
            }

            if (cur_tex) {
                t_light_dir = &nme->light_dir;
                for (int j = 0; j < mesh->num_triangles; j++) {
                    rasterize_tri(j, mesh->triangles, nme->proj);
                }
            }
        }
    }

    // Draw enemy lasers
    draw_lasers(nme_lasers, num_nme_lasers, 8);

    // Draw player lasers
    draw_lasers(lasers, num_lasers, 11);

    // Draw aim
    if (cur_mode == 2) {
        int idx = 97;
        if (tgt_pos) {
            idx = 98;
            transform_pos(&p0, &cam_mat, tgt_pos);
            transform_pos(&p1, &cam_mat, &interp_tgt_pos);

            int x = (int)p0.x - 2;
            int y = (int)p0.y - 4;

            spr(113, x - 1, y + 1, 1, 1);
            spr(114, (int)p1.x - 3, (int)p1.y - 3, 1, 1);

            if (aim_life_ratio >= 0) {
                rectfill(x, y, x + 4, y, 3);
                rectfill(x, y, x + (int)(aim_life_ratio * 4), y, 11);
            }
        }
        spr(idx, (int)aim_proj.x - 3, (int)aim_proj.y - 3, 1, 1);
    }

    // Draw ship
    if (laser_spawned) cur_tex = &ship_tex_laser_lit;
    else cur_tex = &ship_tex;

    sort_tris(ship_mesh.triangles, ship_mesh.num_triangles, ship_mesh.projected);

    if (hit_t != -1) {
        transform_pos(&p0, &cam_mat, &hit_pos);
        draw_explosion(&p0, 3);

        if (hit_t % 2 == 0) {
            pal(0, 2);
            pal(1, 8);
            pal(6, 14);
            pal(9, 8);
            pal(10, 14);
            pal(13, 14);
        }
    }

    t_light_dir = &ship_light_dir;
    set_ngn_pal();

    for (int i = 0; i < ship_mesh.num_triangles; i++) {
        rasterize_tri(i, ship_mesh.triangles, ship_mesh.projected);
    }

    pal_reset();

    // Draw lens flare
    if (star_visible) {
        draw_lens_flare();
    }

    // Draw HUD
    if (cur_mode == 2) {
        char buf[32];
        snprintf(buf, sizeof(buf), "SCORE %d", score);
        print_3d(buf, 1, 1);

        spr(16, 63, 1, 8, 1);
        clip_set(63, 1, life * 16, 7);
        spr(0, 63, 1, 8, 1);
        clip_reset();
    } else if (cur_mode != 1) {
        print_3d("HYPERSPACE By J-FRy", 1, 1);
        print_3d("PORTED By ITSMETERADA", 1, 8);
        if (cur_mode == 0) {
            print_3d("PRESS X TO START", 30, 100);
            if (score > 0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "LAST SCORE %d", score);
                print_3d(buf, 1, 112);
            }
            char buf[32];
            snprintf(buf, sizeof(buf), "BEST SCORE %d", best_score);
            print_3d(buf, 1, 120);
        } else {
            // Options screen (mode 3)
            print_3d("PRESS X TO PLAY", 30, 50);
            print_3d("ARROWS: OPTION", 30, 60);
            const char* option_str[] = {"AUTO FIRE", "MANUAL FIRE", "INVERTED y", "NON-INVERTED y"};
            spr(99, 1, 112, 1, 2);
            print_3d(option_str[manual_fire], 9, 112);
            print_3d(option_str[non_inverted_y + 2], 9, 120);
        }
    }

    // Fade effect
    if (fade_ratio > 0) {
        Vec3 center = {64, 64, 1};
        draw_explosion(&center, fade_ratio);
    }
}

// ============================================================================
// Sprite Data Loading
// ============================================================================

static int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static void load_p8_data(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("Could not open %s\n", filename);
        return;
    }

    char line[300];
    int mode = 0;  // 0: none, 1: gfx, 2: map
    int gfx_row = 0;
    int map_row = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "__gfx__", 7) == 0) {
            mode = 1;
            gfx_row = 0;
            continue;
        }
        if (strncmp(line, "__map__", 7) == 0) {
            mode = 2;
            map_row = 0;
            continue;
        }
        if (strncmp(line, "__gff__", 7) == 0 ||
            strncmp(line, "__sfx__", 7) == 0 ||
            strncmp(line, "__music__", 9) == 0 ||
            strncmp(line, "__label__", 9) == 0) {
            mode = 0;
            continue;
        }

        if (mode == 1 && gfx_row < 128) {
            // Parse GFX data (each char is a pixel color)
            for (int i = 0; i < 128 && line[i] && line[i] != '\n' && line[i] != '\r'; i++) {
                spritesheet[gfx_row][i] = hex_to_int(line[i]);
            }
            gfx_row++;
        }

        if (mode == 2 && map_row < 64) {
            // Parse MAP data - each pair of hex chars is one byte
            // In PICO-8 p8 format: hi nibble first, then lo nibble
            int len = strlen(line);
            for (int i = 0; i + 1 < len && line[i] != '\n' && line[i] != '\r'; i += 2) {
                int byte = (hex_to_int(line[i]) << 4) | hex_to_int(line[i+1]);
                int addr = map_row * 128 + i/2;
                if (addr < 0x1000) {  // Only first 4KB is dedicated map memory
                    map_memory[addr] = byte;
                }
            }
            map_row++;
        }
    }

    fclose(f);

    printf("Loaded: gfx_rows=%d, map_rows=%d\n", gfx_row, map_row);
    printf("First map bytes: %02x %02x %02x %02x\n",
           map_memory[0], map_memory[1], map_memory[2], map_memory[3]);
}

// ============================================================================
// Main
// ============================================================================

static void game_init(void) {
    // Reset palette
    pal_reset();

    // Initialize random
    rnd_state = (uint32_t)time(NULL);

    // Load cart data
    cartdata("hyperspace");

    // Initialize game
    init_main();
    init_ship();
    init_nme();
    init_trail();
    init_bg();
}

int main(int argc, char* argv[]) {
    // Load sprite data
    const char* p8_file = "hyperspace.lua.p8";
    if (argc > 1) {
        p8_file = argv[1];
    }
    load_p8_data(p8_file);

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("Hyperspace",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              SCREEN_WIDTH * SCALE, SCREEN_HEIGHT * SCALE,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return 1;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!texture) {
        printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
        return 1;
    }

    // Initialize game
    game_init();

    // Main loop
    bool running = true;
    Uint32 last_time = SDL_GetTicks();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                bool pressed = (event.type == SDL_KEYDOWN);
                switch (event.key.keysym.sym) {
                    case SDLK_LEFT:  btn_state[0] = pressed; break;
                    case SDLK_RIGHT: btn_state[1] = pressed; break;
                    case SDLK_UP:    btn_state[2] = pressed; break;
                    case SDLK_DOWN:  btn_state[3] = pressed; break;
                    case SDLK_z:
                    case SDLK_c:
                    case SDLK_n:     btn_state[4] = pressed; break;
                    case SDLK_x:
                    case SDLK_v:
                    case SDLK_m:     btn_state[5] = pressed; break;
                    case SDLK_ESCAPE: running = false; break;
                }
            }
        }

        // Update at 30 FPS
        Uint32 current_time = SDL_GetTicks();
        if (current_time - last_time >= 33) {
            last_time = current_time;

            game_update();
            game_draw();

            // Update previous button states for next frame
            memcpy(btn_prev, btn_state, sizeof(btn_prev));

            // Convert screen to pixel buffer
            for (int y = 0; y < SCREEN_HEIGHT; y++) {
                for (int x = 0; x < SCREEN_WIDTH; x++) {
                    pixel_buffer[y * SCREEN_WIDTH + x] = PICO8_PALETTE[screen[y][x] & 15];
                }
            }

            // Update texture and render
            SDL_UpdateTexture(texture, NULL, pixel_buffer, SCREEN_WIDTH * sizeof(uint32_t));
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }

        SDL_Delay(1);
    }

    // Cleanup
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
