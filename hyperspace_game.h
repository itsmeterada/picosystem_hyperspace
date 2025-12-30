/*
 * Hyperspace Game Logic
 * Shared between PicoSystem and ThumbyColor ports
 *
 * This file expects the following to be defined before inclusion:
 * - SCREEN_WIDTH, SCREEN_HEIGHT
 * - FIX_SCREEN_CENTER, FIX_PROJ_CONST
 * - screen[][], spritesheet[][], map_memory[], palette_map[]
 * - cls(), pset(), pget(), sget(), line(), rectfill(), circfill()
 * - spr(), pal(), pal_reset(), clip_set(), clip_reset(), color()
 * - btn(), btnp(), dget(), dset(), sfx()
 * - rnd_state, cart_data_dirty
 * - PSET_FAST(), SGET_FAST() macros
 * - libfixmath functions
 */

#ifndef HYPERSPACE_GAME_H
#define HYPERSPACE_GAME_H

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

// Fixed-point random function: returns value in [0, max)
static fix16_t rnd_fix(fix16_t max) {
    rnd_state = rnd_state * 1103515245 + 12345;
    // Use upper 16 bits for better randomness, treat as 0.0 to 1.0 fraction
    uint16_t frac = (rnd_state >> 16) & 0xFFFF;
    // frac/65536 * max = (frac * max) >> 16
    // But max is already in 16.16 format, so we need to be careful
    // Result = max * (frac / 65536) = (max * frac) >> 16
    int64_t result = ((int64_t)max * frac) >> 16;
    return (fix16_t)result;
}

static int flr_fix(fix16_t x) {
    // floor: round towards negative infinity
    if (x >= 0) {
        return x >> 16;
    } else {
        // For negative numbers, we need proper floor behavior
        return (x >> 16) - ((x & 0xFFFF) ? 1 : 0);
    }
}

static fix16_t mid_fix(fix16_t a, fix16_t b, fix16_t c) {
    if (a > b) { fix16_t t = a; a = b; b = t; }
    if (b > c) { b = c; }
    if (a > b) { b = a; }
    return b;
}

static fix16_t sgn_fix(fix16_t x) {
    if (x > 0) return fix16_one;
    if (x < 0) return -fix16_one;
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

static int32_t dget(int n) {
    if (n >= 0 && n < 64) return cart_data[n];
    return 0;
}

static void dset(int n, int32_t v) {
    if (n >= 0 && n < 64) {
        if (cart_data[n] != v) {
            cart_data[n] = v;
            cart_data_dirty = true;
        }
    }
}

// Sound enable flag (must be declared before sfx())
static int sound_enabled = 1;  // Sound on by default

// Platform-specific sfx implementation
// Define PLATFORM_SFX before including this header to use custom implementation
#ifdef PLATFORM_SFX
extern void platform_sfx(int n, int channel);
static void sfx(int n, int channel) {
    if (!sound_enabled) return;
    platform_sfx(n, channel);
}
#else
static void sfx(int n, int channel) {
    // Sound effects not implemented on this platform
    (void)n;
    (void)channel;
}
#endif

static fix16_t sym_random_fix(fix16_t f) {
    return f - rnd_fix(fix16_mul(f, FIX_TWO));
}

static int get_random_idx(int max) {
    return flr_fix(rnd_fix(fix16_from_int(max)));
}

// ============================================================================
// Game Data Types (Fixed-Point)
// ============================================================================

typedef struct {
    fix16_t x, y, z;
} Vec3;

typedef struct {
    fix16_t m[12];  // 3x4 matrix
} Mat34;

typedef struct {
    Vec3 pos;
    int tri[3];
    fix16_t uv[3][2];
    Vec3 normal;
    fix16_t z;  // for sorting
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
    fix16_t spd;
    int col;
} Trail;

typedef struct {
    Vec3 pos;
    fix16_t spd;
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
    fix16_t rot_x, rot_y;
    fix16_t rot_x_spd, rot_y_spd;
    Vec3 spd;
    Vec3 waypoint;
    fix16_t laser_t;
    fix16_t stop_laser_t;
    fix16_t next_laser_t;
    fix16_t laser_offset_x[2];
    fix16_t laser_offset_y[2];
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

static fix16_t nme_scale[4] = {F16(1.0), F16(2.5), F16(3.0), F16(5.0)};
static int nme_life[4] = {1, 3, 10, 80};
static int nme_score[4] = {1, 10, 10, 100};
static fix16_t nme_radius[4] = {F16(3.25), F16(6.0), F16(8.0), F16(16.0)};
static fix16_t nme_bounds[3] = {F16(-50.0), F16(-50.0), F16(-100.0)};
static fix16_t nme_rot[3] = {F16(0.18), F16(0.24), F16(0.06)};
static fix16_t nme_spd[3] = {F16(1.0), F16(0.5), F16(0.6)};

// Trails
#define MAX_TRAILS 32  // Reduced for PicoSystem memory
static Trail trails[MAX_TRAILS];
static int trail_color[5] = {7, 7, 6, 13, 1};

// Backgrounds
#define MAX_BGS 32  // Reduced for PicoSystem memory
static Background bgs[MAX_BGS];
static int bg_color[3] = {12, 13, 6};

// Lasers
#define MAX_LASERS 50  // Reduced for PicoSystem memory
static Laser lasers[MAX_LASERS];
static int num_lasers = 0;
static Laser nme_lasers[MAX_LASERS];
static int num_nme_lasers = 0;

// Enemies
#define MAX_ENEMIES 25  // Reduced for PicoSystem memory
static Enemy enemies[MAX_ENEMIES];
static int num_enemies = 0;
static int nb_nme_ship = 0;

// Camera
static Mat34 cam_mat;
static fix16_t cam_x = 0, cam_y = 0;
static fix16_t cam_angle_z = F16(-0.4);
static fix16_t cam_angle_x = 0;
static fix16_t cam_depth = F16(22.5);

// Ship state
static Mat34 ship_mat, inv_ship_mat, ship_pos_mat;
static fix16_t ship_x = 0, ship_y = 0;
static fix16_t ship_spd_x = 0, ship_spd_y = 0;
static fix16_t roll_angle = 0, roll_spd = 0;
static fix16_t pitch_angle = 0, pitch_spd = 0;
static fix16_t roll_f = 0, pitch_f = 0;
static fix16_t cur_noise_t = 0, tgt_noise_t = 0;
static fix16_t cur_noise_roll = 0, old_noise_roll = 0;
static fix16_t cur_noise_pitch = 0, old_noise_pitch = 0;

// Light
static Mat34 light_mat;
static Vec3 light_dir, ship_light_dir;

// Game state
static int cur_mode = 0;
static int life = 4;
static int score = 0;
static int best_score = 0;
static fix16_t global_t = 0;
static fix16_t game_spd = F16(1.0);
static int hit_t = -1;
static Vec3 hit_pos;
static fix16_t barrel_cur_t = F16(-1.0);
static int barrel_dir = 0;
static bool laser_on = false;
static bool laser_spawned = false;
static fix16_t aim_z = F16(-200.0);
static Vec3 aim_proj;
static Vec3* tgt_pos = NULL;
static Vec3 interp_tgt_pos;
static fix16_t aim_life_ratio = F16(-1.0);
static fix16_t cur_thrust = 0;
static fix16_t fade_ratio = F16(-1.0);
static int manual_fire = 1;  // Default to MANUAL mode (AUTO off)
static int non_inverted_y = 0;
static fix16_t cur_laser_t = 0;
static int cur_laser_side = -1;
static fix16_t cur_nme_t = 0;
static fix16_t asteroid_mul_t = F16(1.0);
static int cur_sequencer_x = 96;
static int cur_sequencer_y = 96;
static fix16_t next_sequencer_t = 0;
static bool waiting_nme_clear = false;
static bool spawn_asteroids = false;
static Vec3 star_proj;

// For camera interpolation
static fix16_t src_cam_angle_z, src_cam_angle_x;
static fix16_t src_cam_x, src_cam_y;
static fix16_t dst_cam_angle_z, dst_cam_angle_x;
static fix16_t dst_cam_x, dst_cam_y;
static fix16_t interpolation_ratio, interpolation_spd;

// Current texture for rendering
static Texture* cur_tex;
static Vec3* t_light_dir;

// Palette animation (engine glow effect)
// PICO-8 original: ngn_colors = {13,12,7,12}
static int ngn_colors[4] = {13, 12, 7, 12};  // indigo, blue, white, blue
// PICO-8 original: laser_ngn_colors = {3,11,7,11}
static int laser_ngn_colors[4] = {3, 11, 7, 11};  // dark green, green, white, green
static fix16_t ngn_col_idx = 0;
static fix16_t ngn_laser_col_idx = 0;

// Flare
static int flare_offset = 0;

// Explosion colors
static int explosion_color[4] = {9, 10, 15, 7};

// mem_pos for decoding
static int mem_pos = 0;

// ============================================================================
// Fixed-Point Math Functions
// ============================================================================

static void vec3_copy(Vec3* dst, const Vec3* src) {
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
}

static void vec3_set(Vec3* v, fix16_t x, fix16_t y, fix16_t z) {
    v->x = x;
    v->y = y;
    v->z = z;
}

static void vec3_mul(Vec3* v, fix16_t f) {
    v->x = fix16_mul(v->x, f);
    v->y = fix16_mul(v->y, f);
    v->z = fix16_mul(v->z, f);
}

static Vec3 vec3_minus(const Vec3* v0, const Vec3* v1) {
    Vec3 res = {v0->x - v1->x, v0->y - v1->y, v0->z - v1->z};
    return res;
}

static fix16_t vec3_dot(const Vec3* v0, const Vec3* v1) {
    return fix16_mul(v0->x, v1->x) + fix16_mul(v0->y, v1->y) + fix16_mul(v0->z, v1->z);
}

static fix16_t vec3_length(const Vec3* v) {
    return fix16_sqrt(vec3_dot(v, v));
}

static void vec3_normalize(Vec3* v) {
    vec3_mul(v, F16(0.1));
    fix16_t len = vec3_length(v);
    if (len > 0) {
        fix16_t invl = fix16_div(fix16_one, len);
        vec3_mul(v, invl);
    }
}

static void mat_rotx(Mat34* m, fix16_t a) {
    fix16_t angle = fix16_mul(a, FIX_TWO_PI);
    fix16_t cos_a = fix16_cos(angle);
    fix16_t sin_a = fix16_sin(angle);
    // PICO-8's sin is negative of standard sin, so we negate sin_a
    m->m[0] = fix16_one; m->m[1] = 0; m->m[2] = 0; m->m[3] = 0;
    m->m[4] = 0; m->m[5] = cos_a; m->m[6] = -sin_a; m->m[7] = 0;
    m->m[8] = 0; m->m[9] = sin_a; m->m[10] = cos_a; m->m[11] = 0;
}

static void mat_roty(Mat34* m, fix16_t a) {
    fix16_t angle = fix16_mul(a, FIX_TWO_PI);
    fix16_t cos_a = fix16_cos(angle);
    fix16_t sin_a = fix16_sin(angle);
    // PICO-8's sin is negative of standard sin, so we negate sin_a
    m->m[0] = cos_a; m->m[1] = 0; m->m[2] = -sin_a; m->m[3] = 0;
    m->m[4] = 0; m->m[5] = fix16_one; m->m[6] = 0; m->m[7] = 0;
    m->m[8] = sin_a; m->m[9] = 0; m->m[10] = cos_a; m->m[11] = 0;
}

static void mat_rotz(Mat34* m, fix16_t a) {
    fix16_t angle = fix16_mul(a, FIX_TWO_PI);
    fix16_t cos_a = fix16_cos(angle);
    fix16_t sin_a = fix16_sin(angle);
    // PICO-8's sin is negative of standard sin, so we negate sin_a
    m->m[0] = cos_a; m->m[1] = -sin_a; m->m[2] = 0; m->m[3] = 0;
    m->m[4] = sin_a; m->m[5] = cos_a; m->m[6] = 0; m->m[7] = 0;
    m->m[8] = 0; m->m[9] = 0; m->m[10] = fix16_one; m->m[11] = 0;
}

static void mat_translation(Mat34* m, fix16_t x, fix16_t y, fix16_t z) {
    m->m[0] = fix16_one; m->m[1] = 0; m->m[2] = 0; m->m[3] = x;
    m->m[4] = 0; m->m[5] = fix16_one; m->m[6] = 0; m->m[7] = y;
    m->m[8] = 0; m->m[9] = 0; m->m[10] = fix16_one; m->m[11] = z;
}

static void mat_mul(Mat34* res, const Mat34* m0, const Mat34* m1) {
    fix16_t r[12];
    r[0] = fix16_mul(m0->m[0], m1->m[0]) + fix16_mul(m0->m[1], m1->m[4]) + fix16_mul(m0->m[2], m1->m[8]);
    r[1] = fix16_mul(m0->m[0], m1->m[1]) + fix16_mul(m0->m[1], m1->m[5]) + fix16_mul(m0->m[2], m1->m[9]);
    r[2] = fix16_mul(m0->m[0], m1->m[2]) + fix16_mul(m0->m[1], m1->m[6]) + fix16_mul(m0->m[2], m1->m[10]);
    r[3] = fix16_mul(m0->m[0], m1->m[3]) + fix16_mul(m0->m[1], m1->m[7]) + fix16_mul(m0->m[2], m1->m[11]) + m0->m[3];

    r[4] = fix16_mul(m0->m[4], m1->m[0]) + fix16_mul(m0->m[5], m1->m[4]) + fix16_mul(m0->m[6], m1->m[8]);
    r[5] = fix16_mul(m0->m[4], m1->m[1]) + fix16_mul(m0->m[5], m1->m[5]) + fix16_mul(m0->m[6], m1->m[9]);
    r[6] = fix16_mul(m0->m[4], m1->m[2]) + fix16_mul(m0->m[5], m1->m[6]) + fix16_mul(m0->m[6], m1->m[10]);
    r[7] = fix16_mul(m0->m[4], m1->m[3]) + fix16_mul(m0->m[5], m1->m[7]) + fix16_mul(m0->m[6], m1->m[11]) + m0->m[7];

    r[8] = fix16_mul(m0->m[8], m1->m[0]) + fix16_mul(m0->m[9], m1->m[4]) + fix16_mul(m0->m[10], m1->m[8]);
    r[9] = fix16_mul(m0->m[8], m1->m[1]) + fix16_mul(m0->m[9], m1->m[5]) + fix16_mul(m0->m[10], m1->m[9]);
    r[10] = fix16_mul(m0->m[8], m1->m[2]) + fix16_mul(m0->m[9], m1->m[6]) + fix16_mul(m0->m[10], m1->m[10]);
    r[11] = fix16_mul(m0->m[8], m1->m[3]) + fix16_mul(m0->m[9], m1->m[7]) + fix16_mul(m0->m[10], m1->m[11]) + m0->m[11];

    memcpy(res->m, r, sizeof(r));
}

static void mat_mul_vec(Vec3* res, const Mat34* m, const Vec3* v) {
    res->x = fix16_mul(v->x, m->m[0]) + fix16_mul(v->y, m->m[1]) + fix16_mul(v->z, m->m[2]);
    res->y = fix16_mul(v->x, m->m[4]) + fix16_mul(v->y, m->m[5]) + fix16_mul(v->z, m->m[6]);
    res->z = fix16_mul(v->x, m->m[8]) + fix16_mul(v->y, m->m[9]) + fix16_mul(v->z, m->m[10]);
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

static fix16_t normalize_angle(fix16_t a) {
    a = fix16_mod(a, fix16_one);
    if (a > FIX_HALF) a -= fix16_one;
    if (a < -FIX_HALF) a += fix16_one;
    return a;
}

static fix16_t smoothstep(fix16_t ratio) {
    // ratio * ratio * (3 - 2 * ratio)
    fix16_t r2 = fix16_mul(ratio, ratio);
    fix16_t three_minus_2r = F16(3.0) - fix16_mul(FIX_TWO, ratio);
    return fix16_mul(r2, three_minus_2r);
}

// ============================================================================
// Mesh Decoding
// ============================================================================

// Read a raw byte from map memory and convert to signed value * 0.5
static fix16_t decode_byte(void) {
    int res = map_memory[mem_pos];
    mem_pos++;
    if (res >= 128) res = res - 256;  // Convert to signed (-128 to 127)
    // Multiply by 0.5: shift right by 1 in fixed point, but we need the fractional part
    return fix16_from_int(res) >> 1;  // Equivalent to * 0.5
}

// Read a raw byte as integer (for counts and indices)
static int decode_byte_int(void) {
    int res = map_memory[mem_pos];
    mem_pos++;
    if (res >= 128) res = res - 256;  // Convert to signed
    return res / 2;  // The original decode_byte multiplies by 0.5, so divide by 2
}

static void decode_mesh(Mesh* mesh, fix16_t scale) {
    int nb_vert = decode_byte_int();
    if (nb_vert < 0) nb_vert = 0;
    if (nb_vert > 256) nb_vert = 256;  // Sanity check
    mesh->num_vertices = nb_vert;
    mesh->vertices = (Vec3*)calloc(nb_vert > 0 ? nb_vert : 1, sizeof(Vec3));
    mesh->projected = (Vec3*)calloc(nb_vert > 0 ? nb_vert : 1, sizeof(Vec3));

    printf("Decoding mesh: %d vertices at mem_pos=%d\n", nb_vert, mem_pos);

    for (int i = 0; i < nb_vert; i++) {
        mesh->vertices[i].x = fix16_mul(decode_byte(), scale);
        mesh->vertices[i].y = fix16_mul(decode_byte(), scale);
        mesh->vertices[i].z = fix16_mul(decode_byte(), scale);
    }

    int nb_tri = decode_byte_int();
    if (nb_tri < 0) nb_tri = 0;
    if (nb_tri > 256) nb_tri = 256;  // Sanity check
    mesh->num_triangles = nb_tri;
    mesh->triangles = (Triangle*)calloc(nb_tri > 0 ? nb_tri : 1, sizeof(Triangle));

    printf("Decoding mesh: %d triangles\n", nb_tri);

    for (int i = 0; i < nb_tri; i++) {
        Triangle* tri = &mesh->triangles[i];

        // Vertex index (original is 1-based, convert to 0-based)
        tri->tri[0] = decode_byte_int() - 1;
        tri->normal.x = fix16_div(decode_byte(), F16(63.5));
        tri->uv[0][0] = decode_byte();
        tri->uv[0][1] = decode_byte();

        tri->tri[1] = decode_byte_int() - 1;
        tri->normal.y = fix16_div(decode_byte(), F16(63.5));
        tri->uv[1][0] = decode_byte();
        tri->uv[1][1] = decode_byte();

        tri->tri[2] = decode_byte_int() - 1;
        tri->normal.z = fix16_div(decode_byte(), F16(63.5));
        tri->uv[2][0] = decode_byte();
        tri->uv[2][1] = decode_byte();
    }
}

// ============================================================================
// Projection
// ============================================================================

static void transform_pos(Vec3* proj, const Mat34* mat, const Vec3* pos) {
    mat_mul_pos(proj, mat, pos);

    // c = -80 / z (for 128px screen) or -75 / z (for 120px screen)
    // When z is negative (in front of camera), c will be positive
    fix16_t c = fix16_div(FIX_PROJ_CONST, proj->z);

    proj->x = FIX_SCREEN_CENTER + fix16_mul(proj->x, c);
    proj->y = FIX_SCREEN_CENTER - fix16_mul(proj->y, c);

    if (c > 0 && c <= F16(10.0)) {
        proj->z = c;
    } else {
        proj->z = 0;
    }
}

// ============================================================================
// Rasterization (Simplified for PicoSystem)
// ============================================================================

static void rasterize_flat_tri(Vec3* v0, Vec3* v1, Vec3* v2,
                                fix16_t* uv0, fix16_t* uv1, fix16_t* uv2, fix16_t light) {
    fix16_t y0 = v0->y;
    fix16_t y1 = v1->y;

    fix16_t firstline, lastline;

    if (y0 < y1) {
        firstline = fix16_floor(y0 + FIX_HALF) + FIX_HALF;
        lastline = fix16_floor(y1 - FIX_HALF) + FIX_HALF;
    } else if (y0 == y1) {
        return;
    } else {
        firstline = fix16_floor(y1 + FIX_HALF) + FIX_HALF;
        lastline = fix16_floor(y0 - FIX_HALF) + FIX_HALF;
    }

    if (firstline < FIX_HALF) firstline = FIX_HALF;
    if (lastline > F16(SCREEN_HEIGHT - 0.5)) lastline = F16(SCREEN_HEIGHT - 0.5);

    fix16_t x0 = v0->x, z0 = v0->z;
    fix16_t x1 = v1->x, z1 = v1->z;
    fix16_t x2 = v2->x, y2 = v2->y, z2 = v2->z;

    fix16_t uv0x = uv0[0], uv0y = uv0[1];
    fix16_t uv1x = uv1[0], uv1y = uv1[1];
    fix16_t uv2x = uv2[0], uv2y = uv2[1];

    fix16_t cb0 = fix16_mul(x1, y2) - fix16_mul(x2, y1);
    fix16_t cb1 = fix16_mul(x2, y0) - fix16_mul(x0, y2);

    fix16_t d = cb0 + cb1 + fix16_mul(x0, y1) - fix16_mul(x1, y0);
    if (fix16_abs(d) < F16(0.001)) return;

    fix16_t dy = y1 - y0;
    if (fix16_abs(dy) < F16(0.001)) return;
    fix16_t invdy = fix16_div(fix16_one, dy);

    int tex_x = cur_tex->x;
    int tex_y = cur_tex->y;
    int tex_lit_x = cur_tex->light_x;

    for (fix16_t y = firstline; y <= lastline; y += fix16_one) {
        fix16_t coef = fix16_mul(y - y0, invdy);
        fix16_t xfirst = fix16_floor(x0 + fix16_mul(coef, x1 - x0) + F16(0.48)) + FIX_HALF;
        fix16_t xlast = fix16_floor(x0 + fix16_mul(coef, x2 - x0) - F16(0.48)) + FIX_HALF;

        if (xfirst < FIX_HALF) xfirst = FIX_HALF;
        if (xlast > F16(SCREEN_WIDTH - 0.5)) xlast = F16(SCREEN_WIDTH - 0.5);

        fix16_t x0y = fix16_mul(x0, y);
        fix16_t x1y = fix16_mul(x1, y);
        fix16_t x2y = fix16_mul(x2, y);

        // Pre-compute scanline gradients (avoid division in inner loop)
        fix16_t inv_d = fix16_div(fix16_one, d);
        fix16_t db0_dx = fix16_mul(y1 - y2, inv_d);
        fix16_t db1_dx = fix16_mul(y2 - y0, inv_d);

        fix16_t b0_base = fix16_mul(cb0 + fix16_mul(xfirst, y1) + x2y - fix16_mul(xfirst, y2) - x1y, inv_d);
        fix16_t b1_base = fix16_mul(cb1 + fix16_mul(xfirst, y2) + x0y - fix16_mul(xfirst, y0) - x2y, inv_d);

        int py = fix16_to_int(y);
        int dither_row = 56 + (py & 7);  // bitmask instead of modulo

        for (fix16_t x = xfirst; x <= xlast; x += fix16_one) {
            fix16_t b0 = b0_base;
            fix16_t b1 = b1_base;
            fix16_t b2 = fix16_one - b0 - b1;

            b0_base += db0_dx;
            b1_base += db1_dx;

            b0 = fix16_mul(b0, z0);
            b1 = fix16_mul(b1, z1);
            b2 = fix16_mul(b2, z2);

            fix16_t d2 = b0 + b1 + b2;
            if (fix16_abs(d2) < F16(0.001)) continue;

            fix16_t inv_d2 = fix16_div(fix16_one, d2);
            fix16_t uvx = fix16_mul(fix16_mul(b0, uv0x) + fix16_mul(b1, uv1x) + fix16_mul(b2, uv2x), inv_d2);
            fix16_t uvy = fix16_mul(fix16_mul(b0, uv0y) + fix16_mul(b1, uv1y) + fix16_mul(b2, uv2y), inv_d2);

            int px = fix16_to_int(x);
            int offset_x = tex_x;
            int dither_val = SGET_FAST(px & 7, dither_row);  // bitmask instead of modulo
            if (light <= F16(7.0) + fix16_mul(fix16_from_int(dither_val), F16(0.125))) {
                offset_x += tex_lit_x;
            }

            PSET_FAST(px, py, SGET_FAST(fix16_to_int(uvx) + offset_x, fix16_to_int(uvy) + tex_y));
        }
    }
}

static void rasterize_tri(int index, Triangle* tris, Vec3* projs) {
    Triangle* tri = &tris[index];

    if (tri->tri[0] < 0 || tri->tri[1] < 0 || tri->tri[2] < 0) return;
    if (cur_tex == NULL) return;

    Vec3* v0 = &projs[tri->tri[0]];
    Vec3* v1 = &projs[tri->tri[1]];
    Vec3* v2 = &projs[tri->tri[2]];

    // Early cull: all vertices behind camera
    if (v0->z <= 0 && v1->z <= 0 && v2->z <= 0) return;

    fix16_t x0 = v0->x, y0 = v0->y;
    fix16_t x1 = v1->x, y1 = v1->y;
    fix16_t x2 = v2->x, y2 = v2->y;

    // Early cull: completely off-screen
    fix16_t min_x = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    fix16_t max_x = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    fix16_t min_y = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    fix16_t max_y = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);

    if (max_x < 0 || min_x >= F16(SCREEN_WIDTH)) return;
    if (max_y < 0 || min_y >= F16(SCREEN_HEIGHT)) return;

    // Backface cull
    fix16_t nz = fix16_mul(x1 - x0, y2 - y0) - fix16_mul(y1 - y0, x2 - x0);
    if (nz < 0) return;

    fix16_t* uv0 = tri->uv[0];
    fix16_t* uv1 = tri->uv[1];
    fix16_t* uv2 = tri->uv[2];

    // Sort by Y
    Vec3 *tv0 = v0, *tv1 = v1, *tv2 = v2;
    fix16_t *tuv0 = uv0, *tuv1 = uv1, *tuv2 = uv2;

    if (tv1->y < tv0->y) { Vec3* t = tv1; tv1 = tv0; tv0 = t; fix16_t* tu = tuv1; tuv1 = tuv0; tuv0 = tu; }
    if (tv2->y < tv0->y) { Vec3* t = tv2; tv2 = tv0; tv0 = t; fix16_t* tu = tuv2; tuv2 = tuv0; tuv0 = tu; }
    if (tv2->y < tv1->y) { Vec3* t = tv2; tv2 = tv1; tv1 = t; fix16_t* tu = tuv2; tuv2 = tuv1; tuv1 = tu; }

    y0 = tv0->y; y1 = tv1->y; y2 = tv2->y;
    x0 = tv0->x;
    fix16_t z0 = tv0->z, z2 = tv2->z;

    if (y0 == y2) return;

    fix16_t light = fix16_mul(F16(15.0), vec3_dot(t_light_dir, &tri->normal));

    fix16_t c = fix16_div(y1 - y0, y2 - y0);
    Vec3 v3 = {x0 + fix16_mul(c, tv2->x - x0), y1, z0 + fix16_mul(c, z2 - z0)};

    fix16_t b0 = fix16_mul(fix16_one - c, z0);
    fix16_t b1 = fix16_mul(c, z2);
    fix16_t sum = b0 + b1;
    fix16_t invd = (sum > F16(0.001)) ? fix16_div(fix16_one, sum) : 0;

    fix16_t uv3[2] = {
        fix16_mul(fix16_mul(b0, tuv0[0]) + fix16_mul(b1, tuv2[0]), invd),
        fix16_mul(fix16_mul(b0, tuv0[1]) + fix16_mul(b1, tuv2[1]), invd)
    };

    if (tv1->x <= v3.x) {
        rasterize_flat_tri(tv0, tv1, &v3, tuv0, tuv1, uv3, light);
        rasterize_flat_tri(tv2, tv1, &v3, tuv2, tuv1, uv3, light);
    } else {
        rasterize_flat_tri(tv0, &v3, tv1, tuv0, uv3, tuv1, light);
        rasterize_flat_tri(tv2, &v3, tv1, tuv2, uv3, tuv1, light);
    }
}

// Insertion sort - faster than qsort for small arrays (typical mesh has <20 tris)
static void sort_tris(Triangle* tris, int num, Vec3* projs) {
    // First pass: compute z for each triangle
    for (int i = 0; i < num; i++) {
        Triangle* tri = &tris[i];
        tri->z = projs[tri->tri[0]].z + projs[tri->tri[1]].z + projs[tri->tri[2]].z;
    }
    // Insertion sort by z (ascending = back to front)
    for (int i = 1; i < num; i++) {
        Triangle temp = tris[i];
        int j = i - 1;
        while (j >= 0 && tris[j].z > temp.z) {
            tris[j + 1] = tris[j];
            j--;
        }
        tris[j + 1] = temp;
    }
}

// ============================================================================
// Game Initialization
// ============================================================================

static void init_ship(void) {
    mem_pos = 0;
    decode_mesh(&ship_mesh, fix16_one);

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

static void init_single_trail(Trail* trail, fix16_t z) {
    vec3_set(&trail->pos0, sym_random_fix(F16(100.0)) + ship_x, sym_random_fix(F16(100.0)) + ship_y, z);
    trail->spd = fix16_mul(F16(2.5) + rnd_fix(F16(5.0)), game_spd);
    trail->col = flr_fix(rnd_fix(F16(4.0))) + 1;
}

static void init_trail(void) {
    for (int i = 0; i < MAX_TRAILS; i++) {
        init_single_trail(&trails[i], sym_random_fix(F16(150.0)));
    }
}

static void init_single_bg(Background* bg, fix16_t z) {
    fix16_t a = rnd_fix(fix16_one);
    fix16_t r = F16(150.0) + rnd_fix(F16(150.0));
    fix16_t angle = fix16_mul(a, FIX_TWO_PI);
    // PICO-8's sin is negative of standard sin
    vec3_set(&bg->pos, fix16_mul(r, fix16_cos(angle)), fix16_mul(r, -fix16_sin(angle)), z);
    bg->spd = F16(0.05) + rnd_fix(F16(0.05));
    if (flr_fix(rnd_fix(F16(6.0))) == 0) {
        bg->index = 8 + (int)rnd_fix(F16(8.0));
    } else {
        bg->index = -bg_color[get_random_idx(3)];
    }
}

static void init_bg(void) {
    for (int i = 0; i < MAX_BGS; i++) {
        init_single_bg(&bgs[i], sym_random_fix(F16(400.0)));
    }
}

static void init_main(void) {
    // Save persistent data to flash when returning to title
    save_cart_data();

    cur_mode = 0;
    cam_angle_z = F16(-0.4);
    cam_angle_x = fix16_mul(fix16_from_int(flr_fix(rnd_fix(FIX_TWO)) * 2 - 1), F16(0.03) + rnd_fix(F16(0.1)));

    ship_x = 0;
    ship_y = 0;
    cam_x = 0;
    cam_y = 0;
    ship_spd_x = 0;
    ship_spd_y = 0;
    life = 4;
    barrel_cur_t = F16(-1.0);
    num_enemies = 0;
    num_lasers = 0;
    num_nme_lasers = 0;
    hit_t = -1;
    laser_on = false;
    nb_nme_ship = 0;
    aim_z = F16(-200.0);
    cur_thrust = 0;
    roll_f = 0;
    pitch_f = 0;
    global_t = 0;
    asteroid_mul_t = fix16_one;
    cur_sequencer_x = 96;
    cur_sequencer_y = 96;
    next_sequencer_t = 0;
    waiting_nme_clear = false;
    spawn_asteroids = false;
    game_spd = fix16_one;
    cam_depth = F16(22.5);
    cur_nme_t = 0;
    best_score = dget(0);
}

// ============================================================================
// Laser Management
// ============================================================================

static Laser* spawn_laser(Laser* lasers_arr, int* count, Vec3 pos) {
    if (*count >= MAX_LASERS) return NULL;
    Laser* laser = &lasers_arr[*count];
    vec3_copy(&laser->pos0, &pos);
    (*count)++;
    return laser;
}

static void remove_laser(Laser* lasers_arr, int* count, int idx) {
    if (idx < *count - 1) {
        lasers_arr[idx] = lasers_arr[*count - 1];
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
    next_sequencer_t = global_t + F16(0.25);
    fix16_t desc_bounds = fix16_mul(nme_bounds[type - 2], FIX_TWO);
    Vec3 pos = {
        mid_fix(F16(-100.0), sym_random_fix(F16(50.0)) + ship_x, F16(100.0)),
        mid_fix(F16(-100.0), sym_random_fix(F16(50.0)) + ship_y, F16(100.0)),
        desc_bounds - F16(200.0)
    };
    Enemy* nme = spawn_nme(type, pos);
    if (nme) {
        vec3_set(&nme->spd, 0, 0, F16(8.0));
        vec3_copy(&nme->waypoint, &nme->pos);
        nme->waypoint.z = desc_bounds;
    }
}

// ============================================================================
// Collision
// ============================================================================

static void hit_ship(Vec3* pos, fix16_t sqr_size) {
    if (hit_t == -1 && barrel_cur_t < 0) {
        fix16_t dx = fix16_mul(pos->x - ship_x, F16(0.2));
        fix16_t dy = fix16_mul(pos->y - ship_y, F16(0.2));
        fix16_t sqrd = fix16_mul(dx, dx) + fix16_mul(dy, dy);
        if (sqrd < sqr_size) {
            fix16_t n = fix16_div(fix16_one, fix16_sqrt(sqrd + F16(0.001)));
            dx = fix16_mul(dx, n);
            dy = fix16_mul(dy, n);
            roll_f += fix16_mul(dx, F16(0.05));
            pitch_f -= fix16_mul(dy, F16(0.02));
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
        nme->pos.x += fix16_mul(nme->spd.x, game_spd);
        nme->pos.y += fix16_mul(nme->spd.y, game_spd);
        nme->pos.z += fix16_mul(nme->spd.z, game_spd);
        nme->rot_x += nme->rot_x_spd;
        nme->rot_y += nme->rot_y_spd;

        int type = nme->type;

        if (type > 1) {
            int sub_type = type - 1;
            if (sub_type >= 1 && sub_type <= 3) {
                fix16_t desc_bounds = nme_bounds[sub_type - 1];
                fix16_t desc_spd = nme_spd[sub_type - 1];

                Vec3 dir = vec3_minus(&nme->waypoint, &nme->pos);
                vec3_mul(&dir, F16(0.1));
                fix16_t dist = vec3_dot(&dir, &dir);

                if (dist < fix16_mul(game_spd, game_spd) || nme->hit_t == 0) {
                    vec3_set(&nme->waypoint, sym_random_fix(F16(100.0)), sym_random_fix(F16(100.0)),
                             desc_bounds - rnd_fix(-desc_bounds));
                }

                vec3_normalize(&dir);
                nme->spd.x += fix16_mul(fix16_mul(dir.x, desc_spd), F16(0.1));
                nme->spd.y += fix16_mul(fix16_mul(dir.y, desc_spd), F16(0.1));
                nme->spd.z += fix16_mul(fix16_mul(dir.z, desc_spd), F16(0.1));

                if (nme->pos.z >= fix16_mul(desc_bounds, FIX_TWO)) {
                    fix16_t spd_len = vec3_length(&nme->spd);
                    if (spd_len > desc_spd) {
                        vec3_mul(&nme->spd, fix16_div(desc_spd, spd_len));
                    }
                    nme->rot_x = fix16_mul(F16(-0.08), nme->spd.y);
                    nme->rot_y = fix16_mul(-nme_rot[sub_type - 1], nme->spd.x);

                    int nb_lasers = type - 2;

                    if ((type == 4 || nme->hit_t == 0) && nme->laser_t < 0) {
                        nme->laser_t = 0;
                    }

                    nme->laser_t += fix16_one;

                    if (nme->laser_t > nme->stop_laser_t) {
                        nme->laser_t = -fix16_div(F16(60.0) + rnd_fix(F16(60.0)), game_spd);
                        nme->stop_laser_t = F16(60.0) + rnd_fix(F16(60.0));
                        fix16_t c = fix16_mul(F16(-0.5), fix16_div(nme->pos.z, game_spd));
                        for (int j = 0; j < nb_lasers && j < 2; j++) {
                            nme->laser_offset_x[j] = sym_random_fix(F16(30.0)) + fix16_mul(ship_spd_x, c);
                            nme->laser_offset_y[j] = sym_random_fix(F16(30.0)) + fix16_mul(ship_spd_y, c);
                        }
                    }

                    fix16_t laser_t_val = nme->laser_t;
                    fix16_t t = fix16_div(F16(6.0), game_spd);
                    if (laser_t_val > 0) {
                        nme->next_laser_t += fix16_one;

                        if (nme->next_laser_t >= t) {
                            nme->next_laser_t -= t;

                            if (type != 2) {
                                fix16_t angle = fix16_mul(fix16_div(laser_t_val, F16(120.0)), FIX_TWO_PI);
                                fix16_t ratio = fix16_cos(angle);

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
                                            ship_x + fix16_mul(nme->laser_offset_x[j], ratio) + sym_random_fix(F16(5.0)),
                                            ship_y + fix16_mul(nme->laser_offset_y[j], ratio) + sym_random_fix(F16(5.0)),
                                            0
                                        };
                                        Vec3 ldir = vec3_minus(&target, &laser_pos);
                                        vec3_mul(&ldir, F16(0.1));
                                        fix16_t len = vec3_length(&ldir);
                                        fix16_t v = (len > F16(0.001)) ? fix16_div(fix16_mul(FIX_TWO, game_spd), len) : fix16_mul(FIX_TWO, game_spd);
                                        laser->spd.x = fix16_mul(ldir.x, v);
                                        laser->spd.y = fix16_mul(ldir.y, v);
                                        laser->spd.z = fix16_mul(ldir.z, v);
                                    }
                                }
                            } else {
                                Vec3 laser_pos = {nme->pos.x, nme->pos.y, nme->pos.z + F16(12.0)};
                                Laser* laser = spawn_laser(nme_lasers, &num_nme_lasers, laser_pos);
                                if (laser) {
                                    laser->spd.x = sym_random_fix(F16(0.05));
                                    laser->spd.y = sym_random_fix(F16(0.05));
                                    laser->spd.z = fix16_mul(FIX_TWO, game_spd);
                                }
                            }
                        }
                    }
                }
            }
        }

        bool del = false;
        if (nme->pos.z > 0) {
            hit_ship(&nme->pos, F16(2.5));
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
            if (nme->type > 1) nb_nme_ship--;
            if (nme->proj) free(nme->proj);
            enemies[i] = enemies[num_enemies - 1];
            num_enemies--;
            i--;
        }
    }

    cur_nme_t -= fix16_one;
    if (spawn_asteroids && cur_nme_t <= 0) {
        cur_nme_t = fix16_div(fix16_mul(F16(30.0) + rnd_fix(F16(60.0)), asteroid_mul_t), game_spd);
        fix16_t posx = mid_fix(F16(-100.0), fix16_mul(F16(10.0), ship_spd_x) + ship_x + sym_random_fix(F16(30.0)), F16(100.0));
        fix16_t posy = mid_fix(F16(-100.0), fix16_mul(F16(10.0), ship_spd_y) + ship_y + sym_random_fix(F16(30.0)), F16(100.0));
        Vec3 pos = {posx, posy, F16(-50.0)};
        Enemy* nme = spawn_nme(1, pos);
        if (nme) {
            vec3_set(&nme->spd,
                mid_fix(fix16_mul(F16(-100.0) - posx, F16(0.005)), sym_random_fix(F16(0.25)), fix16_mul(F16(100.0) - posx, F16(0.005))),
                mid_fix(fix16_mul(F16(-100.0) - posy, F16(0.005)), sym_random_fix(F16(0.25)), fix16_mul(F16(100.0) - posy, F16(0.005))),
                F16(0.25));
            nme->rot_x_spd = sym_random_fix(F16(0.015));
            nme->rot_y_spd = sym_random_fix(F16(0.015));
        }
    }

    // Sort enemies by distance
    for (int i = 1; i < num_enemies; i++) {
        Enemy nme_tmp = enemies[i];
        int j = i - 1;
        while (j >= 0 && nme_tmp.pos.z > enemies[j].pos.z) {
            enemies[j + 1] = enemies[j];
            j--;
        }
        enemies[j + 1] = nme_tmp;
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
            hit_ship(&laser->pos0, F16(1.5));
            hit_ship(&laser->pos1, F16(1.5));
            remove_laser(nme_lasers, &num_nme_lasers, i);
            i--;
        }
    }
}

static void update_lasers(void) {
    cur_laser_t += fix16_one;
    laser_spawned = false;

    if (laser_on && cur_laser_t > FIX_TWO) {
        cur_laser_t = 0;
        laser_spawned = true;
        Vec3 pos = {fix16_from_int(cur_laser_side), F16(-1.5), F16(-8.0)};
        Vec3 world_pos;
        mat_mul_pos(&world_pos, &ship_mat, &pos);
        spawn_laser(lasers, &num_lasers, world_pos);
        cur_laser_side = -cur_laser_side;
    }

    for (int i = 0; i < num_lasers; i++) {
        Laser* laser = &lasers[i];
        vec3_copy(&laser->pos1, &laser->pos0);
        laser->pos0.z -= F16(5.0);

        if (laser->pos0.z <= F16(-200.0)) {
            remove_laser(lasers, &num_lasers, i);
            i--;
        }
    }
}

static void update_trail(void) {
    for (int i = 0; i < MAX_TRAILS; i++) {
        Trail* trail = &trails[i];
        if (trail->pos0.z >= F16(150.0)) {
            init_single_trail(trail, F16(-150.0));
        }
        vec3_copy(&trail->pos1, &trail->pos0);
        trail->pos0.z += trail->spd;
    }

    for (int i = 0; i < MAX_BGS; i++) {
        Background* bg = &bgs[i];
        bg->pos.z += fix16_mul(bg->spd, game_spd);
        if (bg->pos.z >= F16(400.0)) {
            init_single_bg(bg, F16(-400.0));
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
        fix16_t nme_z = nme->pos.z;

        if (nme_z > laser_pos1->z) {
            laser_idx++;
        } else {
            if (nme->life > 0 && nme_z >= laser_pos0->z) {
                fix16_t dx = fix16_mul(laser_pos0->x - nme->pos.x, F16(0.2));
                fix16_t dy = fix16_mul(laser_pos0->y - nme->pos.y, F16(0.2));

                fix16_t radius = nme_radius[nme->type - 1];
                if (fix16_mul(dx, dx) + fix16_mul(dy, dy) <= fix16_mul(fix16_mul(radius, radius), F16(0.04))) {
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
    fix16_t dx = 0, dy = 0;
    if (btn(0)) dx -= fix16_one;
    if (btn(1)) dx += fix16_one;
    if (btn(2)) dy -= fix16_one;
    if (btn(3)) dy += fix16_one;

    if (cur_mode == 2) {
        global_t += F16(0.033);
        game_spd = fix16_one + fix16_mul(global_t, F16(0.002));

        if (dx == 0 && dy == 0) cur_thrust = 0;
        else cur_thrust = fix16_min(FIX_HALF, cur_thrust + F16(0.1));
        fix16_t mul_spd = cur_thrust;

        if (non_inverted_y != 0) dy = -dy;

        if (barrel_cur_t > F16(-1.0) || life <= 0) {
            dx = 0;
            dy = 0;
        }

        if (btn(5) && dx != 0 && barrel_cur_t < 0) {
            sfx(1, 0);
            barrel_cur_t = 0;
            barrel_dir = dx > 0 ? 1 : -1;
        }

        if (barrel_cur_t >= 0) {
            barrel_cur_t += fix16_one;
            if (barrel_cur_t >= 0) {
                dx = fix16_from_int(barrel_dir * 9);
                dy = 0;
                mul_spd = F16(0.1);
                if (barrel_cur_t > F16(5.0)) {
                    barrel_cur_t = F16(-20.0);
                }
            }
        }

        if (fix16_abs(ship_x) > F16(100.0)) dx = fix16_mul(-sgn_fix(ship_x), F16(0.4));
        if (fix16_abs(ship_y) > F16(100.0)) dy = fix16_mul(-sgn_fix(ship_y), F16(0.4));

        ship_spd_x += fix16_mul(dx, mul_spd);
        ship_spd_y += fix16_mul(dy, mul_spd);

        roll_f -= fix16_mul(F16(0.003), dx);
        pitch_f += fix16_mul(F16(0.0008), dy);

        ship_spd_x = fix16_mul(ship_spd_x, F16(0.85));
        ship_spd_y = fix16_mul(ship_spd_y, F16(0.85));

        ship_x += ship_spd_x;
        ship_y += ship_spd_y;

        cam_x = fix16_mul(F16(1.05), ship_x);
        cam_y = ship_y + F16(11.5);

        if (hit_t != -1) {
            cam_x += sym_random_fix(FIX_TWO);
            cam_y += sym_random_fix(FIX_TWO);
        } else if (life <= 0) {
            hit_t = 0;
            vec3_set(&hit_pos, ship_x, ship_y, 0);
            sfx(2, 1);
        }

        cam_angle_z = fix16_mul(cam_x, F16(0.0005));
        cam_angle_x = fix16_mul(cam_y, F16(0.0003));

        // Sequencer
        if (waiting_nme_clear) {
            if (nb_nme_ship == 0) {
                next_sequencer_t = 0;
                waiting_nme_clear = false;
            } else {
                next_sequencer_t = F16(32767.0);
            }
        }

        if (global_t >= next_sequencer_t) {
            int value = sget(cur_sequencer_x, cur_sequencer_y);
            cur_sequencer_x++;
            if (cur_sequencer_x > 127) {
                cur_sequencer_x = 96;
                cur_sequencer_y++;
            }

            if (value == 1) spawn_nme_ship(3);
            else if (value == 13) { spawn_nme_ship(4); sfx(6, 2); }
            else if (value == 2) spawn_nme_ship(2);
            else if (value == 6) { spawn_asteroids = true; asteroid_mul_t = fix16_one; }
            else if (value == 7) { spawn_asteroids = true; asteroid_mul_t = FIX_HALF; }
            else if (value == 5) spawn_asteroids = false;
            else if (value == 10) next_sequencer_t = global_t + fix16_one;
            else if (value == 9) next_sequencer_t = global_t + F16(10.0);
            else if (value == 11) waiting_nme_clear = true;
            else { cur_sequencer_x = 96; cur_sequencer_y = 96; }
        }

    } else if (cur_mode == 0) {
        if (dx == 0 && dy == 0) dx = F16(-0.25);
        cam_angle_z += fix16_mul(dx, F16(0.007));
        cam_angle_x -= fix16_mul(dy, F16(0.007));

        if (btnp(5)) {
            cur_mode = 3;
            manual_fire = dget(1);
            non_inverted_y = dget(2);
            sound_enabled = dget(3);
            if (sound_enabled == 0 && dget(3) == 0) sound_enabled = 1;
        }
    } else if (cur_mode == 3) {
        cam_angle_z -= F16(0.00175);

        if (btnp(0) || btnp(1)) {
            manual_fire = 1 - manual_fire;
            dset(1, manual_fire);
        }
        if (btnp(2) || btnp(3)) {
            non_inverted_y = 1 - non_inverted_y;
            dset(2, non_inverted_y);
        }
        if (btnp(4)) {
            sound_enabled = 1 - sound_enabled;
            dset(3, sound_enabled);
        }

        if (btnp(5)) {
            src_cam_angle_z = normalize_angle(cam_angle_z);
            src_cam_angle_x = normalize_angle(cam_angle_x);
            src_cam_x = cam_x;
            src_cam_y = cam_y;

            dst_cam_x = fix16_mul(F16(1.05), ship_x);
            dst_cam_y = ship_y + F16(11.5);
            dst_cam_angle_z = fix16_mul(dst_cam_x, F16(0.0005));
            dst_cam_angle_x = fix16_mul(dst_cam_y, F16(0.0003));

            Vec3 src = {src_cam_x, src_cam_y, F16(26.0)};
            Vec3 dst = {dst_cam_x, dst_cam_y, F16(22.5)};
            Vec3 diff = vec3_minus(&src, &dst);
            fix16_t len = vec3_length(&diff);
            interpolation_spd = (len > F16(0.01)) ? fix16_div(F16(0.25), len) : fix16_one;
            interpolation_ratio = 0;
            cur_mode = 1;
        }
    } else {
        interpolation_ratio += interpolation_spd;

        if (interpolation_ratio >= fix16_one) {
            cur_mode = 2;
            score = 0;
        } else {
            fix16_t smoothed_ratio = smoothstep(interpolation_ratio);
            cam_x = src_cam_x + fix16_mul(smoothed_ratio, dst_cam_x - src_cam_x);
            cam_y = src_cam_y + fix16_mul(smoothed_ratio, dst_cam_y - src_cam_y);
            cam_depth = F16(22.5) + fix16_mul(smoothed_ratio, F16(3.5));
            cam_angle_z = src_cam_angle_z + fix16_mul(smoothed_ratio, dst_cam_angle_z - src_cam_angle_z);
            cam_angle_x = src_cam_angle_x + fix16_mul(smoothed_ratio, dst_cam_angle_x - src_cam_angle_x);
        }
    }

    bool old_laser_on = laser_on;
    laser_on = (cur_mode != 2 && btn(4)) ||
               ((btn(4) || (manual_fire != 1 && tgt_pos)) && barrel_cur_t < 0 && hit_t == -1);

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
    cur_noise_t += fix16_one;
    fix16_t noise_attenuation = fix16_cos(fix16_mul(mid_fix(F16(-0.25), fix16_mul(roll_angle, F16(1.2)), F16(0.25)), FIX_TWO_PI));

    if (cur_noise_t > tgt_noise_t) {
        old_noise_roll = cur_noise_roll;
        old_noise_pitch = cur_noise_pitch;
        cur_noise_t = 0;

        fix16_t new_roll_sign = -sgn_fix(cur_noise_roll);
        if (new_roll_sign == 0) new_roll_sign = fix16_one;

        cur_noise_roll = fix16_mul(new_roll_sign, F16(0.01) + rnd_fix(F16(0.03)));
        tgt_noise_t = fix16_mul(fix16_mul(fix16_mul(F16(60.0) + rnd_fix(F16(40.0)), noise_attenuation), fix16_abs(cur_noise_roll - old_noise_roll)), F16(10.0));
        cur_noise_pitch = sym_random_fix(F16(0.01));
    }

    fix16_t noise_ratio = (tgt_noise_t > 0) ? smoothstep(fix16_div(cur_noise_t, tgt_noise_t)) : 0;

    roll_f -= fix16_mul(roll_angle, F16(0.02));
    roll_spd = fix16_mul(roll_spd, F16(0.8)) + roll_f;
    roll_angle += roll_spd;

    pitch_f -= fix16_mul(pitch_angle, F16(0.02));
    pitch_spd = fix16_mul(pitch_spd, F16(0.8)) + pitch_f;
    pitch_angle += pitch_spd;

    roll_f = 0;
    pitch_f = 0;

    fix16_t noise_roll = fix16_mul(noise_attenuation, old_noise_roll + fix16_mul(noise_ratio, cur_noise_roll - old_noise_roll));
    fix16_t noise_pitch = fix16_mul(noise_attenuation, old_noise_pitch + fix16_mul(noise_ratio, cur_noise_pitch - old_noise_pitch));

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

    mat_rotx(&light_mat, F16(0.14));
    mat_roty(&rot, F16(0.34) + fix16_mul(global_t, F16(0.003)));
    mat_mul(&light_mat, &light_mat, &rot);
    Vec3 light_src = {0, 0, -fix16_one};
    mat_mul_vec(&light_dir, &light_mat, &light_src);

    mat_mul_vec(&ship_light_dir, &inv_ship_mat, &light_dir);

    if (fade_ratio >= 0) {
        if (cur_mode == 2) fade_ratio += FIX_TWO;
        else fade_ratio -= FIX_TWO;
        if (fade_ratio >= F16(100.0)) {
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

    Vec3 aim_pos = {ship_x, ship_y - F16(1.5), aim_z};
    transform_pos(&aim_proj, &cam_mat, &aim_pos);

    fix16_t auto_aim_dist = F16(30.0);
    tgt_pos = NULL;
    aim_life_ratio = F16(-1.0);

    if (cur_mode == 2) {
        fix16_t aim_x = aim_proj.x;
        fix16_t aim_y = aim_proj.y;

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
                fix16_t ddx = fix16_mul(nme->proj[0].x - aim_x, F16(0.1));
                fix16_t ddy = fix16_mul(nme->proj[0].y - aim_y, F16(0.1));
                fix16_t sqr_dist = fix16_mul(ddx, ddx) + fix16_mul(ddy, ddy);
                if (sqr_dist < auto_aim_dist) {
                    auto_aim_dist = sqr_dist;
                    tgt_pos = &nme->pos;
                    fix16_t laser_t = fix16_mul(-game_spd, fix16_div(nme->pos.z, F16(5.0)));
                    interp_tgt_pos.x = nme->pos.x + fix16_mul(nme->spd.x, laser_t);
                    interp_tgt_pos.y = nme->pos.y + fix16_mul(nme->spd.y, laser_t);
                    interp_tgt_pos.z = fix16_min(0, nme->pos.z + fix16_mul(nme->spd.z, laser_t));
                    if (nme->type != 1) {
                        aim_life_ratio = fix16_div(fix16_from_int(nme->life), fix16_from_int(nme_life[nme->type - 1]));
                    }
                }
            }
        }
    }

    fix16_t tgt_z = F16(-200.0);
    if (tgt_pos) tgt_z = tgt_pos->z;
    aim_z += fix16_mul(tgt_z - aim_z, F16(0.2));

    Vec3 star_pos = {fix16_mul(light_mat.m[2], F16(100.0)), fix16_mul(light_mat.m[6], F16(100.0)), fix16_mul(light_mat.m[10], F16(100.0))};
    transform_pos(&star_proj, &ship_pos_mat, &star_pos);
}

static void draw_explosion(Vec3* proj, fix16_t size) {
    fix16_t invz = proj->z;
    int col = explosion_color[get_random_idx(4)];
    circfill(fix16_to_int(proj->x + fix16_mul(sym_random_fix(fix16_mul(size, FIX_HALF)), invz)),
             fix16_to_int(proj->y + fix16_mul(sym_random_fix(fix16_mul(size, FIX_HALF)), invz)),
             fix16_to_int(fix16_mul(invz, size + rnd_fix(size))), col);
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
            line(fix16_to_int(p0.x), fix16_to_int(p0.y), fix16_to_int(p1.x), fix16_to_int(p1.y), col);
        }
    }
}

static void set_ngn_pal(void) {
    // Fast wrap-around without division
    ngn_col_idx += fix16_one;
    if (ngn_col_idx >= F16(4.0)) ngn_col_idx -= F16(4.0);
    ngn_laser_col_idx += F16(0.2);
    if (ngn_laser_col_idx >= F16(4.0)) ngn_laser_col_idx -= F16(4.0);

    pal(12, ngn_colors[fix16_to_int(ngn_col_idx)]);

    int index = fix16_to_int(ngn_laser_col_idx);
    pal(8, laser_ngn_colors[index]);
    pal(14, laser_ngn_colors[(index + 1) & 3]);
    pal(15, laser_ngn_colors[(index + 2) & 3]);
}

static void draw_lens_flare(void) {
    int sx = fix16_to_int(star_proj.x);
    int sy = fix16_to_int(star_proj.y);
    if (sx < 0 || sx >= SCREEN_WIDTH || sy < 0 || sy >= SCREEN_HEIGHT) return;
    if (pget(sx, sy) != 7) return;

    fix16_t vx = FIX_SCREEN_CENTER - star_proj.x;
    fix16_t vy = FIX_SCREEN_CENTER - star_proj.y;

    fix16_t factors[] = {F16(-0.3), F16(0.4), F16(0.5), F16(0.9), F16(1.0)};
    // Sprite indices for each flare element (swapped 0 and 1 to match original)
    int sprite_map[] = {1, 0, 2, 3, 2};

    // Use flare_offset to alternate between sprite sets 40-43 and 44-47
    int base_sprite = 40 + flare_offset * 4;

    for (int i = 0; i < 5; i++) {
        int px = fix16_to_int(FIX_SCREEN_CENTER + fix16_mul(vx, factors[i]));
        int py = fix16_to_int(FIX_SCREEN_CENTER + fix16_mul(vy, factors[i]));
        // Center the 8x8 sprite by offsetting -4
        spr(base_sprite + sprite_map[i], px - 4, py - 4, 1, 1);
    }

    flare_offset = 1 - flare_offset;
}

static bool star_visible;

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
                spr(index + 16 * flr_fix(rnd_fix(FIX_TWO)), fix16_to_int(p0.x), fix16_to_int(p0.y), 1, 1);
            } else {
                int col = 7;
                if (rnd_fix(fix16_one) > FIX_HALF) col = -index;
                pset(fix16_to_int(p0.x), fix16_to_int(p0.y), col);
            }
        }
    }

    // Draw sun
    star_visible = star_proj.z > 0 && star_proj.x >= 0 && star_proj.x < F16(SCREEN_WIDTH) &&
                   star_proj.y >= 0 && star_proj.y < F16(SCREEN_HEIGHT);
    if (star_visible) {
        int index = 32 + flr_fix(rnd_fix(F16(4.0))) * 2;
        spr(index, fix16_to_int(star_proj.x) - 7, fix16_to_int(star_proj.y) - 7, 2, 2);
    }

    // Draw trails
    fix16_t trail_color_coef = F16(2.25);  // 0.45 * 5
    for (int i = 0; i < MAX_TRAILS; i++) {
        Trail* trail = &trails[i];
        transform_pos(&p0, &cam_mat, &trail->pos0);
        transform_pos(&p1, &cam_mat, &trail->pos1);

        if (p0.z > 0 && p1.z > 0) {
            int index = fix16_to_int(mid_fix(fix16_from_int(trail->col), fix16_div(trail_color_coef, p0.z) + fix16_one, F16(5.0))) - 1;
            if (index < 0) index = 0;
            if (index > 4) index = 4;
            line(fix16_to_int(p0.x), fix16_to_int(p0.y), fix16_to_int(p1.x), fix16_to_int(p1.y), trail_color[index]);
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
                    fix16_t ratio = FIX_HALF + fix16_div(F16(15.0) + fix16_from_int(nme->life), F16(30.0));
                    fix16_t size = fix16_mul(fix16_mul(ratio, nme_radius[nme->type - 1]), F16(0.8));
                    if (((-nme->life) & 1) == 0) cur_tex = &nme_tex_hit;
                    for (int j = 0; j < 3; j++) {
                        int idx = get_random_idx(mesh->num_vertices);
                        draw_explosion(&nme->proj[idx], size);
                    }
                } else {
                    fix16_t ratio = FIX_HALF + fix16_div(F16(6.0) - fix16_from_int(nme->hit_t), F16(12.0));
                    fix16_t size = fix16_mul(ratio, F16(3.0));
                    if ((nme->hit_t & 1) == 0) cur_tex = &nme_tex_hit;
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

            int x = fix16_to_int(p0.x) - 2;
            int y = fix16_to_int(p0.y) - 4;

            spr(113, x - 1, y + 1, 1, 1);
            spr(114, fix16_to_int(p1.x) - 3, fix16_to_int(p1.y) - 3, 1, 1);

            if (aim_life_ratio >= 0) {
                rectfill(x, y, x + 4, y, 3);
                rectfill(x, y, x + fix16_to_int(fix16_mul(aim_life_ratio, F16(4.0))), y, 11);
            }
        }
        spr(idx, fix16_to_int(aim_proj.x) - 3, fix16_to_int(aim_proj.y) - 3, 1, 1);
    }

    // Draw ship
    if (laser_spawned) cur_tex = &ship_tex_laser_lit;
    else cur_tex = &ship_tex;

    sort_tris(ship_mesh.triangles, ship_mesh.num_triangles, ship_mesh.projected);

    if (hit_t != -1) {
        transform_pos(&p0, &cam_mat, &hit_pos);
        draw_explosion(&p0, F16(3.0));

        if ((hit_t & 1) == 0) {
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

        spr(16, 59, 1, 8, 1);  // Adjusted for 120px
        clip_set(59, 1, life * 15, 7);  // Adjusted
        spr(0, 59, 1, 8, 1);
        clip_reset();
    } else if (cur_mode != 1) {
        print_3d("HYPERSPACE by J-Fry", 1, 1);
        print_3d("PicoSystem Port by itsmeterada", 1, 8);
        if (cur_mode == 0) {
            print_3d("PRESS X TO START", 30, 95);
            if (score > 0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "LAST %d", score);
                print_3d(buf, 1, 105);
            }
            char buf[32];
            snprintf(buf, sizeof(buf), "BEST %d", best_score);
            print_3d(buf, 1, 112);
        } else {
            print_3d("PRESS X TO START", 30, 50);
            print_3d("ARROWS:OPT", 30, 60);
            const char* option_str[] = {"AUTO", "MANUAL", "INV Y", "NORM Y", "SND OFF", "SND ON"};
            spr(99, 1, 98, 1, 2);
            print_3d(option_str[manual_fire], 9, 98);
            print_3d(option_str[non_inverted_y + 2], 9, 105);
            print_3d(option_str[sound_enabled + 4], 9, 112);
        }
    }

    // Fade effect
    if (fade_ratio > 0) {
        Vec3 center = {FIX_SCREEN_CENTER, FIX_SCREEN_CENTER, fix16_one};
        draw_explosion(&center, fade_ratio);
    }
}

// ============================================================================
// Sprite Data (embedded)
// ============================================================================

// This data would normally be loaded from a file, but for PicoSystem
// we embed it directly. The spritesheet and map data should be
// pre-converted and included here.

#include "hyperspace_data.h"

static void load_embedded_data(void) {
    // Copy embedded spritesheet data
    memcpy(spritesheet, hyperspace_spritesheet, sizeof(spritesheet));
    // Copy embedded map data (mesh definitions)
    memcpy(map_memory, hyperspace_map, sizeof(map_memory));
}

// ============================================================================
// Main
// ============================================================================

// Note: update_buttons() should be implemented by the platform
// Note: Platform should set rnd_state before calling game_init()

static void game_init(void) {
    pal_reset();

    // Load persistent data from flash
    load_cart_data();

    init_main();
    init_ship();
    init_nme();
    init_trail();
    init_bg();
}

#endif // HYPERSPACE_GAME_H
