//
//  Pimoroni PicoSystem hardware abstraction layer
//

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "picosystem_hardware.h"
#include "hardware/timer.h"


#ifdef PIXEL_DOUBLE
  #include "screen_double.pio.h"
#else
  #include "screen.pio.h"
#endif

volatile struct picosystem_hw pshw;
color_t _fb[PICOSYSTEM_SCREEN_WIDTH * PICOSYSTEM_SCREEN_HEIGHT] __attribute__ ((aligned (4))) = { };

buffer_t* picosystem_alloc_buffer(uint32_t w, uint32_t h, void *data)
{
  buffer_t *b = (buffer_t *)malloc(sizeof(buffer_t));
  b->w = w;
  b->h = h;
  if (data) {
    b->data = (color_t *)data;
    b->alloc = false;
  } else {
    b->data = (color_t *)malloc(w * h * sizeof(color_t));
    b->alloc = true;
  }
  return b;
}

void picosystem_init_inputs(uint32_t pin_mask)
{
  for (uint8_t i = 0; i < 32; i++) {
    uint32_t pin = 1U << i;
    if (pin & pin_mask) {
      gpio_set_function(pin, GPIO_FUNC_SIO);
      gpio_set_dir(pin, GPIO_IN);
      gpio_pull_up(pin);
    }
  }
}

void picosystem_init_outputs(uint32_t pin_mask) 
{
  for(uint8_t i = 0; i < 32; i++) {
    uint32_t pin = 1U << i;
    if(pin & pin_mask) {
      gpio_set_function(pin, GPIO_FUNC_SIO);
      gpio_set_dir(pin, GPIO_OUT);
      gpio_put(pin, 0);
    }
  }
}

bool picosystem_pressed(uint32_t b)
{
  return !(pshw.io & (1U << b)) && (pshw.lio & (1U << b));
}

bool picosystem_released(uint32_t b)
{
  return (pshw.io & (1U << b)) && !(pshw.lio & (1U << b));
}

bool picosystem_button(uint32_t b)
{
  return !(pshw.io & (1U << b));
} 

void picosytem_reset_to_dfu()
{
  reset_usb_boot(0, 0);
}

float picosystem_battery_voltage() 
{
  // convert adc reading to voltage
  adc_select_input(0);
  float v = ((float)adc_read() / (1 << 12)) * 3.3f;
  return v * 3.0f; // correct for voltage divider on board
}

uint32_t picosystem_time()
{
  return to_ms_since_boot(get_absolute_time());
}

uint32_t picosystem_time_us()
{
  return to_us_since_boot(get_absolute_time());
}

void picosystem_sleep(uint32_t ms)
{
  sleep_ms(ms);
}

void picosystem_wait_vsync()
{
  while(gpio_get(PICOSYSTEM_PIN_VSYNC)) {}   // if in vsync already wait for it to end
  while(!gpio_get(PICOSYSTEM_PIN_VSYNC)) {}  // now wait for vsync to occur
}

bool picosystem_is_flipping()
{
  return pshw.in_flip;
}

// in pixel doubling mode...
//
// scanline data is sent via dma to the pixel doubling pio program which then
// writes the data to the st7789 via an spi-like interface. the pio program
// doubles pixels horizontally, but we need to double them vertically by
// sending each scanline to the pio twice.
//
// to minimise the number of dma transfers we transmit the current scanline
// and the previous scanline in every transfer. the exceptions are the first
// and final scanlines which are sent on their own to start and complete the
// write.
//
// - transfer #1: scanline 0
// - transfer #2: scanline 0 + scanline 1
// - transfer #3: scanline 1 + scanline 2
// ...
// - transfer #n - 1: scanline (n - 1) + scanline n
// - transfer #n: scanline n

// sets up dma transfer for current and previous scanline (except for
// scanlines 0 and 120 which are sent on their own.)
void picosystem_transmit_scanline()
{
  // start of data to transmit
  uint32_t *s = (uint32_t *)&pshw.screen->data[((pshw.dma_scanline - 1) < 0 ? 0 : (pshw.dma_scanline - 1)) * 120];
  // number of 32-bit words to transmit
  uint16_t c = (pshw.dma_scanline == 0 || pshw.dma_scanline == 120) ? 60 : 120;

  dma_channel_transfer_from_buffer_now(pshw.dma_channel, s, c);
}

// once the dma transfer of the scanline is complete we move to the
// next scanline (or quit if we're finished)
void __isr picosystem_dma_complete() {
  if(dma_channel_get_irq0_status(pshw.dma_channel)) {
    dma_channel_acknowledge_irq0(pshw.dma_channel); // clear irq flag

    #ifdef PIXEL_DOUBLE
      if(++pshw.dma_scanline > 120) {
        // all scanlines done. reset counter and exit
        pshw.dma_scanline = -1;
        pshw.in_flip = false;
        return;
      }
      picosystem_transmit_scanline();
    #else
      pshw.in_flip = false;
    #endif
  }
}

void picosystem_flip() {
  if(!picosystem_is_flipping()) {
    pshw.in_flip = true;
    #ifdef PIXEL_DOUBLE
      // start the dma transfer of scanline data
      pshw.dma_scanline = 0;
      picosystem_transmit_scanline();
    #else
      // if dma transfer already in process then skip
      uint32_t c = pshw.screen->w * pshw.screen->h / 2;
      dma_channel_transfer_from_buffer_now(pshw.dma_channel, pshw.screen->data, c);
    #endif
  }
}

void picosystem_screen_program_init(PIO pio, uint sm) {
  #ifdef PIXEL_DOUBLE
    uint offset = pio_add_program(pshw.screen_pio, &screen_double_program);
    pio_sm_config c = screen_double_program_get_default_config(offset);
  #else
    uint offset = pio_add_program(screen_pio, &screen_program);
    pio_sm_config c = screen_program_get_default_config(offset);
  #endif

  pio_sm_set_consecutive_pindirs(pio, sm, PICOSYSTEM_PIN_MOSI, 2, true);

  #ifndef NO_OVERCLOCK
    // dividing the clock by two ensures we keep the spi transfer to
    // around 62.5mhz as per the st7789 datasheet when overclocking
    sm_config_set_clkdiv_int_frac(&c, 2, 1);
  #endif

  // osr shifts left, autopull off, autopull threshold 32
  sm_config_set_out_shift(&c, false, false, 32);

  // configure out, set, and sideset pins
  sm_config_set_out_pins(&c, PICOSYSTEM_PIN_MOSI, 1);
  sm_config_set_sideset_pins(&c, PICOSYSTEM_PIN_SCK);

  pio_sm_set_pins_with_mask(
    pio, sm, 0, (1u << PICOSYSTEM_PIN_SCK) | (1u << PICOSYSTEM_PIN_MOSI));

  pio_sm_set_pindirs_with_mask(
    pio, sm, (1u << PICOSYSTEM_PIN_SCK) | (1u << PICOSYSTEM_PIN_MOSI), (1u << PICOSYSTEM_PIN_SCK) | (1u << PICOSYSTEM_PIN_MOSI));

  // join fifos as only tx needed (gives 8 deep fifo instead of 4)
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

  pio_gpio_init(pshw.screen_pio, PICOSYSTEM_PIN_MOSI);
  pio_gpio_init(pshw.screen_pio, PICOSYSTEM_PIN_SCK);

  pio_sm_init(pio, sm, offset, &c);
  pio_sm_set_enabled(pio, sm, true);
}

uint16_t picosystem_gamma_correct(uint8_t v) {
  float gamma = 2.8;
  return (uint16_t)(pow((float)(v) / 100.0f, gamma) * 65535.0f + 0.5f);
}

void picosystem_backlight(uint8_t b) {
  pwm_set_gpio_level(PICOSYSTEM_PIN_BACKLIGHT, picosystem_gamma_correct(b));
}

void picosystem_play_note(uint32_t f, uint32_t v) {
  // adjust the clock divider to achieve this desired frequency
  #ifndef NO_OVERCLOCK
    float clock = 250000000.0f;
  #else
    float clock = 125000000.0f;
  #endif

  // float pwm_divider = clock / _audio_pwm_wrap / f;
  // pwm_set_clkdiv(pwm_gpio_to_slice_num(AUDIO), pwm_divider);
  // pwm_set_wrap(pwm_gpio_to_slice_num(AUDIO), _audio_pwm_wrap);

  // work out usable range of volumes at this frequency. the piezo speaker
  // isn't driven in a way that can control volume easily however if we're
  // clever with the duty cycle we can ensure that the ceramic doesn't have
  // time to fully deflect - effectively reducing the volume.
  //
  // through experiment it seems that constraining the deflection period of
  // the piezo to between 0 and 1/10000th of a second gives reasonable control
  // over the volume. the relationship is non linear so we also apply a
  // correction curve which is tuned so that the result sounds reasonable.
  // uint32_t max_count = (f * _audio_pwm_wrap) / 10000;

  // the change in volume isn't linear - we correct for this here
  // float curve = 1.8f;
  // uint32_t level = (pow((float)(v) / 100.0f, curve) * max_count);
  // pwm_set_gpio_level(AUDIO, level);
}

void picosystem_led(uint8_t r, uint8_t g, uint8_t b) {
  pwm_set_gpio_level(PICOSYSTEM_PIN_RED,   picosystem_gamma_correct(r));
  pwm_set_gpio_level(PICOSYSTEM_PIN_GREEN, picosystem_gamma_correct(g));
  pwm_set_gpio_level(PICOSYSTEM_PIN_BLUE,  picosystem_gamma_correct(b));
}

void picosystem_screen_command(uint8_t c, size_t len, const char *data) {
  gpio_put(PICOSYSTEM_PIN_CS, 0);
  gpio_put(PICOSYSTEM_PIN_DC, 0); // command mode
  spi_write_blocking(spi0, &c, 1);
  if(data) {
    gpio_put(PICOSYSTEM_PIN_DC, 1); // data mode
    spi_write_blocking(spi0, (const uint8_t*)data, len);
  }
  gpio_put(PICOSYSTEM_PIN_CS, 1);
}

uint32_t picosystem_gpio_get() {
  return gpio_get_all();
}

void picosystem_init_hardware() {
  // configure backlight pwm and disable backlight while setting up
  pwm_config cfg = pwm_get_default_config();
  pwm_set_wrap(pwm_gpio_to_slice_num(PICOSYSTEM_PIN_BACKLIGHT), 65535);
  pwm_init(pwm_gpio_to_slice_num(PICOSYSTEM_PIN_BACKLIGHT), &cfg, true);
  gpio_set_function(PICOSYSTEM_PIN_BACKLIGHT, GPIO_FUNC_PWM);
  picosystem_backlight(0);

  #ifdef PICOSYSTEM_OVERCLOCK
    // Apply a modest overvolt, default is 1.10v.
    // this is required for a stable 250MHz on some RP2040s
    vreg_set_voltage(VREG_VOLTAGE_1_20);
   sleep_ms(10);
    // overclock the rp2040 to 250mhz
    set_sys_clock_khz(250000, true);
  #endif

  // configure control io pins
  picosystem_init_inputs(
    PICOSYSTEM_INPUT_A | 
    PICOSYSTEM_INPUT_B | 
    PICOSYSTEM_INPUT_X | 
    PICOSYSTEM_INPUT_Y | 
    PICOSYSTEM_INPUT_UP | 
    PICOSYSTEM_INPUT_DOWN | 
    PICOSYSTEM_INPUT_LEFT | 
    PICOSYSTEM_INPUT_RIGHT);
  picosystem_init_outputs(PICOSYSTEM_PIN_CHARGE_LED);

  // configure adc channel used to monitor battery charge
  adc_init(); adc_gpio_init(PICOSYSTEM_PIN_BATTERY_LEVEL);

  // configure pwm channels for red, green, blue led channels
  pwm_set_wrap(pwm_gpio_to_slice_num(PICOSYSTEM_PIN_RED), 65535);
  pwm_init(pwm_gpio_to_slice_num(PICOSYSTEM_PIN_RED), &cfg, true);
  gpio_set_function(PICOSYSTEM_PIN_RED, GPIO_FUNC_PWM);

  pwm_set_wrap(pwm_gpio_to_slice_num(PICOSYSTEM_PIN_GREEN), 65535);
  pwm_init(pwm_gpio_to_slice_num(PICOSYSTEM_PIN_GREEN), &cfg, true);
  gpio_set_function(PICOSYSTEM_PIN_GREEN, GPIO_FUNC_PWM);

  pwm_set_wrap(pwm_gpio_to_slice_num(PICOSYSTEM_PIN_BLUE), 65535);
  pwm_init(pwm_gpio_to_slice_num(PICOSYSTEM_PIN_BLUE), &cfg, true);
  gpio_set_function(PICOSYSTEM_PIN_BLUE, GPIO_FUNC_PWM);

  // configure the spi interface used to initialise the screen
  spi_init(spi0, 8000000);

  // reset cycle the screen before initialising
  gpio_set_function(PICOSYSTEM_PIN_LCD_RESET, GPIO_FUNC_SIO);
  gpio_set_dir(PICOSYSTEM_PIN_LCD_RESET, GPIO_OUT);
  gpio_put(PICOSYSTEM_PIN_LCD_RESET, 0); sleep_ms(100); gpio_put(PICOSYSTEM_PIN_LCD_RESET, 1);

  // configure screen io pins
  gpio_set_function(PICOSYSTEM_PIN_DC, GPIO_FUNC_SIO); gpio_set_dir(PICOSYSTEM_PIN_DC, GPIO_OUT);
  gpio_set_function(PICOSYSTEM_PIN_CS, GPIO_FUNC_SIO); gpio_set_dir(PICOSYSTEM_PIN_CS, GPIO_OUT);
  gpio_set_function(PICOSYSTEM_PIN_SCK, GPIO_FUNC_SPI);
  gpio_set_function(PICOSYSTEM_PIN_MOSI, GPIO_FUNC_SPI);

  // setup the st7789 screen driver
  gpio_put(PICOSYSTEM_PIN_CS, 1);

  // initialise the screen configuring it as 12-bits per pixel in RGB order
  enum st7789 {
    SWRESET   = 0x01, TEON      = 0x35, MADCTL    = 0x36, COLMOD    = 0x3A,
    GCTRL     = 0xB7, VCOMS     = 0xBB, LCMCTRL   = 0xC0, VDVVRHEN  = 0xC2,
    VRHS      = 0xC3, VDVS      = 0xC4, FRCTRL2   = 0xC6, PWRCTRL1  = 0xD0,
    FRMCTR1   = 0xB1, FRMCTR2   = 0xB2, GMCTRP1   = 0xE0, GMCTRN1   = 0xE1,
    INVOFF    = 0x20, SLPOUT    = 0x11, DISPON    = 0x29, GAMSET    = 0x26,
    DISPOFF   = 0x28, RAMWR     = 0x2C, INVON     = 0x21, CASET     = 0x2A,
    RASET     = 0x2B, STE       = 0x44, DGMEN     = 0xBA,
  };

  picosystem_screen_command(SWRESET, 0, NULL);
  sleep_ms(5);
  picosystem_screen_command(MADCTL,    1, "\x04");
  picosystem_screen_command(TEON,      1, "\x00");
  picosystem_screen_command(FRMCTR2,   5, "\x0C\x0C\x00\x33\x33");
  picosystem_screen_command(COLMOD,    1, "\x03");
  picosystem_screen_command(GAMSET,    1, "\x01");

  picosystem_screen_command(GCTRL,     1, "\x14");
  picosystem_screen_command(VCOMS,     1, "\x25");
  picosystem_screen_command(LCMCTRL,   1, "\x2C");
  picosystem_screen_command(VDVVRHEN,  1, "\x01");
  picosystem_screen_command(VRHS,      1, "\x12");
  picosystem_screen_command(VDVS,      1, "\x20");
  picosystem_screen_command(PWRCTRL1,  2, "\xA4\xA1");
  picosystem_screen_command(FRCTRL2,   1, "\x1E");
  picosystem_screen_command(GMCTRP1,  14, "\xD0\x04\x0D\x11\x13\x2B\x3F\x54\x4C\x18\x0D\x0B\x1F\x23");
  picosystem_screen_command(GMCTRN1,  14, "\xD0\x04\x0C\x11\x13\x2C\x3F\x44\x51\x2F\x1F\x1F\x20\x23");
  picosystem_screen_command(INVON, 0, NULL);
  sleep_ms(115);
  picosystem_screen_command(SLPOUT, 0, NULL);
  picosystem_screen_command(DISPON, 0, NULL);
  picosystem_screen_command(CASET,     4, "\x00\x00\x00\xef");
  picosystem_screen_command(RASET,     4, "\x00\x00\x00\xef");
  picosystem_screen_command(RAMWR, 0, NULL);

  // switch st7789 into data mode so that we can start transmitting frame
  // data - no need to issue any more commands
  gpio_put(PICOSYSTEM_PIN_CS, 0);
  gpio_put(PICOSYSTEM_PIN_DC, 1);

  // at this stage the screen is configured and expecting to receive
  // pixel data. each time we write a screen worth of data the
  // st7789 resets the data pointer back to the start meaning that
  // we can now just leave the screen in data writing mode and
  // reassign the spi pins to our pixel doubling pio. so long as
  // we always write the entire screen we'll never get out of sync.

  // enable vsync input pin, we use this to synchronise screen updates
  // ensuring no tearing
  gpio_init(PICOSYSTEM_PIN_VSYNC);
  gpio_set_dir(PICOSYSTEM_PIN_VSYNC, GPIO_IN);

  // setup the screen updating pio program
  picosystem_screen_program_init(pshw.screen_pio, pshw.screen_sm);

  // initialise dma channel for transmitting pixel data to screen
  // via the screen updating pio program
  pshw.dma_channel = 0;
  dma_channel_config config = dma_channel_get_default_config(pshw.dma_channel);
  channel_config_set_bswap(&config, true);
  channel_config_set_dreq(&config, pio_get_dreq(pshw.screen_pio, pshw.screen_sm, true));
  dma_channel_configure(
    pshw.dma_channel, &config, &pshw.screen_pio->txf[pshw.screen_sm], NULL, 0, false);
  dma_channel_set_irq0_enabled(pshw.dma_channel, true);
  irq_set_enabled(pio_get_dreq(pshw.screen_pio, pshw.screen_sm, true), true);

  irq_set_exclusive_handler(DMA_IRQ_0, picosystem_dma_complete);
  irq_set_enabled(DMA_IRQ_0, true);

  // initialise audio pwm pin
  int audio_pwm_slice_number = pwm_gpio_to_slice_num(PICOSYSTEM_PIN_AUDIO);
  pwm_config audio_pwm_cfg = pwm_get_default_config();
  pwm_init(audio_pwm_slice_number, &audio_pwm_cfg, true);
  gpio_set_function(PICOSYSTEM_PIN_AUDIO, GPIO_FUNC_PWM);
  pwm_set_gpio_level(PICOSYSTEM_PIN_AUDIO, 0);
}

void picosystem_init()
{
  pshw.screen_pio = pio0;
  pshw.screen_sm = 0;
  pshw.dma_channel = dma_claim_unused_channel(true);
  pshw.dma_scanline = -1;

  pshw.screen = picosystem_alloc_buffer(120, 120, _fb);
  pshw.cx = 0;
  pshw.cy = 0;
  pshw.cw = 120;
  pshw.ch = 120;

  pshw.io = 0;
  pshw.lio = 0;

  pshw.in_flip = false;

  picosystem_init_hardware();
}


void picosystem_update(uint32_t tick);

void picosystem_draw(uint32_t tick);

// =============================================================================
// Audio System (PICO-8 Compatible)
// =============================================================================

#define AUDIO_SAMPLE_RATE 22050
#define AUDIO_PWM_WRAP    255
#define AUDIO_NUM_CHANNELS 4

// PICO-8 frequency table (C-0 to D#-5, 64 notes)
static const uint16_t p8_freq_table[64] = {
    65, 69, 73, 78, 82, 87, 92, 98,
    104, 110, 117, 123, 131, 139, 147, 156,
    165, 175, 185, 196, 208, 220, 233, 247,
    262, 277, 294, 311, 330, 349, 370, 392,
    415, 440, 466, 494, 523, 554, 587, 622,
    659, 698, 740, 784, 831, 880, 932, 988,
    1047, 1109, 1175, 1245, 1319, 1397, 1480, 1568,
    1661, 1760, 1865, 1976, 2093, 2217, 2349, 2489
};

// PICO-8 SFX data for Hyperspace
typedef struct {
    uint8_t speed;
    uint8_t loop_start;
    uint8_t loop_end;
    uint8_t notes[32][4];  // pitch, waveform, volume, effect
} P8SFX;

static const P8SFX hyperspace_sfx[] = {
    // SFX 0: Laser fire (descending saw wave)
    {1, 0, 13, {
        {50, 2, 3, 0}, {51, 2, 3, 0}, {51, 2, 3, 0}, {49, 2, 1, 0},
        {46, 2, 3, 0}, {41, 2, 3, 0}, {36, 2, 4, 0}, {34, 2, 3, 0},
        {32, 2, 3, 0}, {29, 2, 3, 0}, {28, 2, 3, 0}, {28, 2, 2, 0},
        {28, 2, 1, 0}, {28, 2, 0, 0}, {28, 0, 0, 0}, {0, 0, 0, 0},
        {50, 4, 0, 0}, {52, 4, 0, 0}, {52, 4, 0, 0}, {49, 4, 0, 0},
        {46, 4, 0, 0}, {41, 4, 0, 0}, {36, 4, 0, 0}, {34, 4, 0, 0},
        {32, 4, 0, 0}, {29, 4, 0, 0}, {28, 4, 0, 0}, {28, 4, 0, 0},
        {28, 4, 0, 0}, {1, 4, 0, 0}, {1, 4, 0, 0}, {1, 4, 0, 0}
    }},
    // SFX 1: Player damage / barrel roll
    {5, 0, 0, {
        {36, 6, 7, 0}, {36, 6, 7, 0}, {39, 6, 7, 0}, {42, 6, 7, 0},
        {49, 6, 7, 0}, {56, 6, 7, 0}, {63, 6, 7, 0}, {63, 6, 7, 0},
        {48, 6, 7, 0}, {41, 6, 7, 0}, {36, 6, 7, 0}, {32, 6, 7, 0},
        {30, 6, 6, 0}, {28, 6, 6, 0}, {27, 6, 5, 0}, {26, 6, 5, 0},
        {25, 6, 4, 0}, {25, 6, 4, 0}, {24, 6, 3, 0}, {25, 6, 3, 0},
        {26, 6, 2, 0}, {28, 6, 2, 0}, {32, 6, 1, 0}, {35, 6, 1, 0},
        {10, 6, 0, 0}, {11, 6, 0, 0}, {13, 6, 0, 0}, {16, 6, 0, 0},
        {18, 6, 0, 0}, {20, 6, 0, 0}, {23, 6, 0, 0}, {24, 6, 0, 0}
    }},
    // SFX 2: Hit enemy / explosion
    {3, 0, 0, {
        {45, 6, 7, 0}, {41, 4, 7, 0}, {36, 4, 7, 0}, {25, 6, 7, 0},
        {30, 4, 7, 0}, {32, 6, 7, 0}, {29, 6, 7, 0}, {13, 6, 7, 0},
        {22, 6, 7, 0}, {20, 4, 7, 0}, {16, 4, 7, 0}, {15, 4, 7, 0},
        {19, 6, 7, 0}, {11, 4, 7, 0}, {9, 4, 7, 0}, {7, 6, 6, 0},
        {7, 4, 5, 0}, {5, 4, 4, 0}, {8, 6, 3, 0}, {2, 4, 2, 0},
        {1, 4, 1, 0}, {12, 6, 0, 0}, {5, 6, 0, 0}, {1, 6, 0, 0},
        {1, 6, 0, 0}, {1, 6, 0, 0}, {3, 6, 0, 0}, {1, 6, 0, 0},
        {2, 6, 0, 0}, {1, 6, 0, 0}, {1, 6, 0, 0}, {0, 0, 0, 0}
    }},
    // SFX 3: (unused placeholder)
    {1, 0, 0, {
        {60, 3, 7, 0}, {60, 0, 7, 0}, {55, 1, 7, 0}, {57, 0, 7, 0},
        {54, 0, 7, 0}, {51, 0, 7, 0}, {47, 1, 7, 0}, {48, 0, 7, 0},
        {41, 0, 7, 0}, {34, 0, 7, 0}, {32, 0, 7, 0}, {27, 0, 7, 0},
        {23, 0, 7, 0}, {29, 1, 7, 0}, {20, 0, 7, 0}, {19, 0, 7, 0},
        {18, 0, 7, 0}, {18, 0, 7, 0}, {19, 0, 7, 0}, {21, 0, 7, 0},
        {18, 1, 7, 0}, {23, 0, 7, 0}, {18, 1, 7, 0}, {30, 0, 7, 0},
        {39, 0, 7, 0}, {44, 0, 7, 0}, {53, 0, 7, 0}, {54, 0, 7, 0},
        {28, 1, 7, 0}, {33, 1, 7, 0}, {46, 1, 7, 0}, {0, 0, 0, 0}
    }},
    // SFX 4: (unused placeholder)
    {1, 0, 13, {
        {44, 4, 4, 0}, {18, 0, 4, 0}, {1, 0, 2, 0}, {16, 0, 0, 0},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}
    }},
    // SFX 5: Bonus pickup
    {1, 0, 0, {
        {44, 4, 7, 0}, {40, 4, 7, 0}, {35, 4, 7, 0}, {32, 4, 7, 0},
        {28, 4, 7, 0}, {26, 4, 7, 0}, {23, 4, 6, 0}, {21, 4, 4, 0},
        {21, 4, 2, 0}, {20, 4, 0, 0}, {22, 4, 0, 0}, {0, 0, 0, 0},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}
    }},
    // SFX 6: Boss spawn
    {24, 0, 0, {
        {0, 0, 0, 0}, {7, 3, 6, 0}, {20, 1, 4, 0}, {7, 3, 6, 0},
        {20, 1, 4, 0}, {26, 3, 7, 0}, {20, 1, 4, 0}, {27, 3, 7, 0},
        {1, 4, 4, 0}, {23, 3, 7, 0}, {23, 3, 7, 0}, {23, 3, 7, 0},
        {23, 3, 7, 0}, {23, 3, 6, 0}, {23, 3, 5, 0}, {23, 3, 0, 0},
        {1, 4, 0, 0}, {1, 4, 0, 0}, {23, 3, 0, 0}, {11, 4, 0, 0},
        {23, 0, 0, 0}, {23, 0, 0, 0}, {23, 0, 0, 0}, {23, 0, 0, 0},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}
    }},
    // SFX 7: Boss damage
    {32, 0, 0, {
        {13, 2, 7, 0}, {13, 2, 7, 0}, {8, 2, 7, 0}, {8, 2, 7, 0},
        {4, 2, 7, 0}, {4, 2, 7, 0}, {1, 2, 7, 0}, {1, 2, 7, 0},
        {1, 2, 7, 0}, {1, 2, 7, 0}, {1, 2, 7, 0}, {1, 2, 7, 0},
        {18, 0, 0, 0}, {18, 0, 0, 0}, {18, 0, 0, 0}, {18, 0, 0, 0},
        {19, 0, 0, 0}, {20, 0, 0, 0}, {50, 0, 2, 0}, {20, 0, 0, 0},
        {20, 0, 0, 0}, {52, 0, 4, 0}, {68, 0, 4, 0}, {82, 0, 4, 0},
        {118, 0, 5, 0}, {82, 0, 4, 0}, {102, 0, 4, 0}, {82, 0, 4, 0},
        {82, 0, 4, 0}, {82, 0, 4, 0}, {1, 0, 4, 0}, {0, 0, 0, 0}
    }}
};

#define NUM_PICOSYSTEM_SFX (sizeof(hyperspace_sfx) / sizeof(hyperspace_sfx[0]))

// Audio channel state
typedef struct {
    const P8SFX *sfx;
    int note_index;
    int sample_count;
    int samples_per_note;
    uint32_t phase;
    uint32_t phase_inc;
    uint8_t volume;
    uint8_t waveform;
    bool active;
    bool looping;
} PicoAudioChannel;

static PicoAudioChannel ps_audio_channels[AUDIO_NUM_CHANNELS];
static uint8_t ps_master_volume = 255;
static uint32_t ps_lfsr = 0xACE1;
static uint ps_audio_pwm_slice;

// For piezo buzzer: generate square wave directly at audio frequency
// This is MUCH louder than PWM DAC approach

static inline uint8_t ps_gen_noise(void) {
    uint32_t bit = ((ps_lfsr >> 0) ^ (ps_lfsr >> 2) ^ (ps_lfsr >> 3) ^ (ps_lfsr >> 5)) & 1;
    ps_lfsr = (ps_lfsr >> 1) | (bit << 15);
    return (ps_lfsr & 0xFF);
}

// Update PWM frequency directly for piezo buzzer
static void ps_update_piezo_frequency(void) {
    // Find the highest priority active channel
    PicoAudioChannel *active = NULL;
    for (int ch = 0; ch < AUDIO_NUM_CHANNELS; ch++) {
        PicoAudioChannel *c = &ps_audio_channels[ch];
        if (c->active && c->volume > 0) {
            if (active == NULL || c->volume > active->volume) {
                active = c;
            }
        }
    }

    if (active == NULL || active->phase_inc == 0) {
        // No sound - set to silence (50% duty = no movement)
        pwm_set_gpio_level(PICOSYSTEM_PIN_AUDIO, 0);
        return;
    }

    // Calculate frequency from phase increment
    // phase_inc = (freq * 65536) / AUDIO_SAMPLE_RATE
    // freq = phase_inc * AUDIO_SAMPLE_RATE / 65536
    uint32_t freq = (active->phase_inc * AUDIO_SAMPLE_RATE) >> 16;
    if (freq < 20) freq = 20;
    if (freq > 20000) freq = 20000;

    // Set PWM to generate this frequency directly
    // PicoSystem runs at 250MHz (overclock) or 125MHz
    #ifndef NO_OVERCLOCK
        uint32_t clock = 250000000;
    #else
        uint32_t clock = 125000000;
    #endif

    // Calculate wrap value for desired frequency
    // freq = clock / (wrap + 1) / divider
    // Using divider = 1, wrap = clock / freq - 1
    uint32_t wrap = clock / freq - 1;
    if (wrap > 65535) wrap = 65535;
    if (wrap < 100) wrap = 100;

    pwm_set_wrap(ps_audio_pwm_slice, wrap);

    // 50% duty cycle for maximum volume on piezo
    // Scale by volume (0-7)
    uint32_t level = (wrap / 2) * active->volume / 7;
    pwm_set_gpio_level(PICOSYSTEM_PIN_AUDIO, level);
}

static struct repeating_timer ps_audio_timer;

static bool ps_audio_timer_callback(struct repeating_timer *t) {
    (void)t;
    ps_update_piezo_frequency();
    return true;
}

void picosystem_audio_init(void) {
    ps_audio_pwm_slice = pwm_gpio_to_slice_num(PICOSYSTEM_PIN_AUDIO);

    // Configure PWM for direct frequency generation
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 1.0f);  // No clock division
    pwm_init(ps_audio_pwm_slice, &config, true);
    gpio_set_function(PICOSYSTEM_PIN_AUDIO, GPIO_FUNC_PWM);

    pwm_set_wrap(ps_audio_pwm_slice, 1000);
    pwm_set_gpio_level(PICOSYSTEM_PIN_AUDIO, 0);

    for (int i = 0; i < AUDIO_NUM_CHANNELS; i++) {
        ps_audio_channels[i].active = false;
        ps_audio_channels[i].sfx = NULL;
    }

    // Timer for note progression (60Hz is enough for note changes)
    add_repeating_timer_ms(-16, ps_audio_timer_callback, NULL, &ps_audio_timer);
}

void picosystem_sfx(int n, int channel) {
    if (channel < 0 || channel >= AUDIO_NUM_CHANNELS) return;

    PicoAudioChannel *c = &ps_audio_channels[channel];

    if (n == -1) {
        c->active = false;
        return;
    }

    if (n == -2) {
        for (int i = 0; i < AUDIO_NUM_CHANNELS; i++) {
            ps_audio_channels[i].active = false;
        }
        return;
    }

    if (n < 0 || n >= (int)NUM_PICOSYSTEM_SFX) return;

    c->sfx = &hyperspace_sfx[n];
    c->note_index = 0;
    c->sample_count = 0;
    c->phase = 0;

    c->samples_per_note = c->sfx->speed * 183;
    if (c->samples_per_note < 183) c->samples_per_note = 183;

    uint8_t pitch = c->sfx->notes[0][0];
    c->waveform = c->sfx->notes[0][1];
    c->volume = c->sfx->notes[0][2];

    if (pitch < 64 && c->volume > 0) {
        uint16_t freq = p8_freq_table[pitch];
        c->phase_inc = (freq * 65536) / AUDIO_SAMPLE_RATE;
        c->active = true;
    } else {
        c->active = false;
    }

    c->looping = (c->sfx->loop_end > c->sfx->loop_start);
}

void picosystem_audio_update(void) {
    for (int ch = 0; ch < AUDIO_NUM_CHANNELS; ch++) {
        PicoAudioChannel *c = &ps_audio_channels[ch];
        if (!c->active || !c->sfx) continue;

        c->sample_count += AUDIO_SAMPLE_RATE / 60;

        if (c->sample_count >= c->samples_per_note) {
            c->sample_count = 0;
            c->note_index++;

            if (c->looping && c->note_index >= c->sfx->loop_end) {
                c->note_index = c->sfx->loop_start;
            } else if (c->note_index >= 32) {
                c->active = false;
                continue;
            }

            uint8_t pitch = c->sfx->notes[c->note_index][0];
            c->waveform = c->sfx->notes[c->note_index][1];
            c->volume = c->sfx->notes[c->note_index][2];

            if (pitch < 64 && c->volume > 0) {
                uint16_t freq = p8_freq_table[pitch];
                c->phase_inc = (freq * 65536) / AUDIO_SAMPLE_RATE;
            } else if (c->volume == 0) {
                c->phase_inc = 0;
            } else {
                c->active = false;
            }
        }
    }
}

void picosystem_set_volume(uint8_t volume) {
    ps_master_volume = volume;
}