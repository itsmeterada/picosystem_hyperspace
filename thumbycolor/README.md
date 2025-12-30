# Hyperspace for Thumby Color

A port of the PICO-8 game "Hyperspace" by J-Fry to the TinyCircuits Thumby Color handheld console.

## Screenshot

![Screenshot](screenshot.jpg)

## Features

- Full 3D software rasterization with texture mapping
- Fixed-point arithmetic optimized for RP2350
- Native PICO-8 resolution (128x128) - no scaling needed!
- 4 enemy types: asteroids, small ships, medium ships, and boss
- Barrel roll maneuver for dodging
- Auto-fire and manual fire modes
- Lens flare effects with temporal dithering
- Persistent high score via flash storage
- **Supports both ARM (Cortex-M33) and RISC-V (Hazard3) builds**

## Hardware

| Component | Specification |
|-----------|---------------|
| MCU | RP2350 (Dual ARM Cortex-M33 or Hazard3 RISC-V @ 150MHz) |
| Display | GC9107 128x128 0.85" IPS LCD |
| Color | RGB565 (65,536 colors) |
| Display Interface | SPI @ 80MHz with DMA |
| Audio | Magnetic Buzzer (PWM driven, 22kHz 8-bit) |
| Haptics | DC Vibration Motor |

## Building

### Requirements

- [Pico SDK](https://github.com/raspberrypi/pico-sdk) (v2.0.0 or later for RP2350)
- CMake 3.13+
- ARM GCC toolchain (`arm-none-eabi-gcc`)
- (Optional) RISC-V toolchain for RISC-V builds

### Environment Setup

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
```

### Quick Build (using build script)

```bash
cd thumbycolor

# ARM build (default)
./build.sh

# Or explicitly:
./build.sh arm

# RISC-V build
./build.sh riscv

# Clean build directory
./build.sh clean
```

### Manual Build with CMake

#### ARM Build (Cortex-M33)

```bash
cd thumbycolor
mkdir build && cd build
cmake ..
make -j$(nproc)
```

#### RISC-V Build (Hazard3)

```bash
cd thumbycolor
mkdir build && cd build
cmake -DRISCV=ON ..
make -j$(nproc)
```

### RISC-V Toolchain Installation

RP2350's RISC-V cores are 32-bit Hazard3 (RV32IMAC). The Pico SDK looks for `riscv32-corev-elf-gcc` or `riscv32-unknown-elf-gcc`.

**Recommended: CORE-V Toolchain (no symlinks needed!)**

The CORE-V toolchain from Embecosm is recommended as it provides `riscv32-corev-elf-gcc` which Pico SDK recognizes directly.

1. Download from: https://buildbot.embecosm.com/job/corev-gcc-ubuntu2204/
   - Choose the latest successful build
   - Download `corev-openhw-gcc-ubuntu2204-YYYYMMDD.tar.gz`

2. Extract and set environment:
```bash
# Extract
tar xvf corev-openhw-gcc-ubuntu2204-YYYYMMDD.tar.gz

# Set environment variable
export PICO_TOOLCHAIN_PATH=/path/to/corev-openhw-gcc-ubuntu2204-YYYYMMDD
```

3. Add to your `.bashrc` for persistence:
```bash
echo 'export PICO_TOOLCHAIN_PATH=/path/to/corev-openhw-gcc-ubuntu2204-YYYYMMDD' >> ~/.bashrc
```

Reference: [CNX Software - Using RISC-V cores on the Raspberry Pi Pico 2](https://www.cnx-software.com/2024/08/31/using-risc-v-cores-on-the-raspberry-pi-pico-2-board-and-rp2350-mcu-from-blinking-an-led-to-building-linux/)

> **Note**: `gcc-riscv64-unknown-elf` (64-bit) does NOT work. RP2350 requires a 32-bit RISC-V toolchain.

### Output

The output `hyperspace_thumbycolor.uf2` can be copied to the Thumby Color in bootloader mode (hold BOOTSEL while connecting USB).

## Controls

| Button | Action |
|--------|--------|
| D-Pad | Move ship |
| A | Fire laser / Confirm |
| B | Barrel roll |
| Menu | Start game |

## GPIO Pin Mapping

Based on TinyCircuits Thumby Color hardware:

### Buttons (active-low with internal pull-ups)
| Button | GPIO |
|--------|------|
| D-Pad Up | 1 |
| D-Pad Down | 3 |
| D-Pad Left | 0 |
| D-Pad Right | 2 |
| A | 21 |
| B | 25 |
| Left Bumper | 6 |
| Right Bumper | 22 |
| Menu | 26 |

### Display (SPI0)
| Signal | GPIO |
|--------|------|
| MOSI (SDA) | 19 |
| SCK | 18 |
| CS | 17 |
| DC | 16 |
| RST | 4 |
| Backlight | 7 |

### Other Peripherals
| Function | GPIO |
|----------|------|
| Audio PWM | 23 |
| Audio Enable | 20 |
| LED Red | 11 |
| LED Green | 10 |
| LED Blue | 12 |
| Rumble | 5 |
| Battery ADC | 29 |
| Charge Status | 24 |

## Technical Details

### CPU Architecture Options

| Architecture | Core | ISA | Notes |
|--------------|------|-----|-------|
| ARM (default) | Cortex-M33 | ARMv8-M | Better toolchain support |
| RISC-V | Hazard3 | RV32IMAC + extensions | Experimental |

RISC-V extensions: Zicsr, Zifencei, Zba, Zbb, Zbs, Zbkb

### Display Driver (GC9107)

- SPI0 at 80MHz for pixel data (16-bit transfers)
- DMA for efficient framebuffer transfer
- PWM backlight brightness control
- Display inversion enabled for correct colors
- Custom gamma curves for improved brightness

### Color Format

- Internal: 8-bit palette indices (PICO-8 16-color palette)
- Display: RGB565 (16-bit, R and B channels swapped for GC9107)
- Palette animation supported via `pal()` function

### Audio System

Thumby Color uses a **magnetic buzzer** for audio output, driven by a PICO-8 compatible software synthesizer.

#### Hardware

| Component | Specification |
|-----------|---------------|
| Output Device | Magnetic Buzzer |
| Output Method | PWM (Pulse Width Modulation) |
| PWM GPIO | 23 |
| Enable GPIO | 20 |

#### PWM Audio

PWM audio works by rapidly switching a digital output ON/OFF. The duty cycle (ratio of ON time) controls the average voltage, which the buzzer smooths into an analog-like waveform.

```
Audio Sample (0-255) → PWM Duty Cycle → Buzzer → Sound Wave
     128 (50%)       → ████░░░░        → ~~~    → Silence
     255 (100%)      → ████████        → ───    → Peak
       0 (0%)        → ░░░░░░░░        → ___    → Trough
```

#### Audio Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| Sample Rate | 22,050 Hz | Updated via timer interrupt (~45μs) |
| Resolution | 8-bit | 256 volume levels |
| PWM Frequency | ~586 kHz | 150MHz / 256 (inaudible carrier) |
| Channels | 4 | PICO-8 compatible polyphony |

#### PICO-8 Compatible Waveforms

| ID | Waveform | Description |
|----|----------|-------------|
| 0 | Triangle | Smooth, mellow tone |
| 1 | Tilted Saw | Asymmetric sawtooth |
| 2 | Sawtooth | Bright, buzzy tone |
| 3 | Square (50%) | Classic chiptune sound |
| 4 | Pulse (25%) | Narrower pulse, hollow sound |
| 5 | Organ | Square + octave harmonic |
| 6 | Noise | LFSR pseudo-random (explosions, etc.) |
| 7 | Phaser | Two detuned sawtooths |

#### Sound Effects

| SFX ID | Usage |
|--------|-------|
| 0 | Laser fire (descending saw wave) |
| 1 | Player damage / Barrel roll |
| 2 | Enemy hit / Explosion |
| 5 | Bonus pickup |
| 6 | Boss spawn (eerie triangle wave) |
| 7 | Boss damage |

#### Implementation Details

**Phase Accumulator Oscillator:**
```c
// Frequency to phase increment
phase_inc = (frequency * 65536) / AUDIO_SAMPLE_RATE;

// Sample generation (called at 22kHz)
phase += phase_inc;
sample = waveform_generator(phase);
```

**Channel Mixing:**
```c
// All active channels are averaged
mix = sum_of_channels / active_channel_count;

// Master volume applied (default: 200/255)
output = 128 + (mix * master_volume / 255);
```

**LFSR Noise Generator:**
```c
// 16-bit Linear Feedback Shift Register
bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
lfsr = (lfsr >> 1) | (bit << 15);
```

#### Why Magnetic Buzzer + PWM Works Well

| Advantage | Explanation |
|-----------|-------------|
| Natural low-pass filter | Buzzer mechanics smooth out PWM carrier frequency |
| Wide frequency response | Better bass than piezo speakers |
| Simple interface | Single GPIO + enable pin |
| Low CPU overhead | Timer interrupt handles sample output |

### Rendering Pipeline

1. **Mesh Loading**: Meshes decoded from embedded map memory
2. **Matrix Transformations**: 3x4 matrices for rotation and translation
3. **Projection**: Perspective projection with 128px screen center
4. **Triangle Rasterization**: Scanline-based with barycentric interpolation
5. **Texture Mapping**: UV coordinates with perspective correction
6. **Lighting**: Per-triangle lighting with dithering

### Memory Usage

| Section | Description |
|---------|-------------|
| Spritesheet | 16KB (128x128 4-bit pixels) |
| Map Memory | 4KB (mesh definitions) |
| Screen Buffer | 16KB (128x128 8-bit palette) |
| Framebuffer | 32KB (128x128 RGB565) |

### Advantages over PicoSystem Version

| Feature | Thumby Color | PicoSystem |
|---------|--------------|------------|
| Resolution | 128x128 (native PICO-8) | 120x120 |
| Color Depth | RGB565 (65K) | RGBA4444 (4K) |
| SPI Speed | 80MHz | ~62MHz |
| MCU | RP2350 | RP2040 |

## Project Structure

```
thumbycolor/
├── main_thumbycolor.c    # Platform-specific main code
├── thumbycolor_hw.c      # Hardware abstraction layer
├── thumbycolor_hw.h      # HAL header
├── CMakeLists.txt        # Build configuration (ARM/RISC-V)
├── build.sh              # Build script
├── README.md             # This file
└── build/                # Build output directory
    └── hyperspace_thumbycolor.uf2

../
├── hyperspace_game.h     # Shared game logic (all ports)
├── hyperspace_data.h     # Shared sprite/mesh data
└── libfixmath/           # Fixed-point math library
```

## Code Architecture

The game logic is shared across multiple ports via `hyperspace_game.h`:

- **hyperspace_game.h**: Platform-independent game logic, rendering, and state
- **main_thumbycolor.c**: Thumby Color specific code (display, input, main loop)
- **thumbycolor_hw.c/h**: Hardware abstraction (SPI, GPIO, audio)

This allows the same game logic to run on PicoSystem, Thumby Color, GBA, and SDL2.

## Related Projects

- [Hyperspace for PicoSystem](https://github.com/itsmeterada/picosystem_hyperspace) - Port for Pimoroni PicoSystem
- [Hyperspace for GBA](https://github.com/itsmeterada/hyperspace_gba) - Port for Game Boy Advance
- [Hyperspace SDL2](https://github.com/itsmeterada/hyperspace) - SDL2 port for desktop platforms

## Technical Information

- [thumby color](https://tinycircuits.com/products/thumby-color) - Thumby Color Specs
- [thumby color documents](https://color.thumby.us/pages/documentation-and-examples/documentation-and-examples/) - Thumby Color Documents and Examples
- [GC9107 spec sheet](https://cdn.hackaday.io/files/1881838051221472/GC9107%20DataSheet%20V1.2.pdf) - LCD controller

## Credits

- **Original Game**: [Hyperspace](https://www.lexaloffle.com/bbs/?tid=41663) by J-Fry (PICO-8)
- **Thumby Color Port**: itsmeterada
- **libfixmath**: [PetteriAimworthy/libfixmath](https://github.com/PetteriAimworthy/libfixmath)

## License

This port is provided for educational and personal use. Please respect the original author's rights.
