#!/usr/bin/env python3
"""Convert PICO-8 .p8 file graphics and map data to C header."""

import sys

def convert_p8_to_header(p8_file, output_file):
    with open(p8_file, 'r') as f:
        content = f.read()

    # Find sections
    gfx_start = content.find('__gfx__')
    map_start = content.find('__map__')
    label_start = content.find('__label__')
    sfx_start = content.find('__sfx__')

    if gfx_start == -1:
        print("Error: __gfx__ section not found")
        return
    if map_start == -1:
        print("Error: __map__ section not found")
        return

    # Extract GFX data (ends at __label__ or __map__)
    gfx_end = label_start if label_start != -1 and label_start < map_start else map_start
    gfx_section = content[gfx_start:gfx_end].split('\n')[1:]
    gfx_section = [line.strip() for line in gfx_section if line.strip() and not line.startswith('__')]

    # Extract MAP data (ends at __sfx__ or __gff__ or end)
    map_end = sfx_start if sfx_start != -1 else len(content)
    map_section = content[map_start:map_end].split('\n')[1:]
    map_section = [line.strip() for line in map_section if line.strip() and not line.startswith('__')]

    print(f"Found {len(gfx_section)} GFX lines")
    print(f"Found {len(map_section)} MAP lines")

    with open(output_file, 'w') as f:
        f.write("/*\n")
        f.write(" * Hyperspace Game Data - PicoSystem Port\n")
        f.write(" * Auto-generated from hyperspace.lua.p8\n")
        f.write(" */\n\n")
        f.write("#ifndef HYPERSPACE_DATA_H\n")
        f.write("#define HYPERSPACE_DATA_H\n\n")
        f.write("#include <stdint.h>\n\n")

        # Write spritesheet
        f.write("// Spritesheet data (128x128 pixels, 4-bit palette indices)\n")
        f.write("static const uint8_t hyperspace_spritesheet[128][128] = {\n")

        for row_idx, row in enumerate(gfx_section[:128]):
            row = row[:128]  # Limit to 128 chars
            pixels = []
            for c in row:
                try:
                    pixels.append(str(int(c, 16)))
                except ValueError:
                    pixels.append('0')
            # Pad to 128 if needed
            while len(pixels) < 128:
                pixels.append('0')

            f.write('    {' + ','.join(pixels) + '}')
            if row_idx < 127:
                f.write(',')
            f.write('\n')

        # Fill remaining rows if needed
        for row_idx in range(len(gfx_section), 128):
            zeros = ','.join(['0'] * 128)
            f.write('    {' + zeros + '}')
            if row_idx < 127:
                f.write(',')
            f.write('\n')

        f.write("};\n\n")

        # Write map data
        f.write("// Map memory data (4KB, contains mesh definitions)\n")
        f.write("static const uint8_t hyperspace_map[0x1000] = {\n")

        map_bytes = []
        for row in map_section:
            row = row.strip()
            for i in range(0, len(row), 2):
                if i + 1 < len(row):
                    try:
                        byte_val = int(row[i:i+2], 16)
                        map_bytes.append(byte_val)
                    except ValueError:
                        map_bytes.append(0)

        # Pad to 4KB
        while len(map_bytes) < 0x1000:
            map_bytes.append(0)

        print(f"Map bytes extracted: {len(map_bytes)} (first 16: {map_bytes[:16]})")

        for i in range(0, 0x1000, 16):
            bytes_str = ','.join(f'0x{b:02x}' for b in map_bytes[i:i+16])
            f.write('    ' + bytes_str)
            if i + 16 < 0x1000:
                f.write(',')
            f.write('\n')

        f.write("};\n\n")
        f.write("#endif // HYPERSPACE_DATA_H\n")

    print(f"Generated {output_file}")

if __name__ == '__main__':
    p8_file = '/home/terada/trees/itsmeterada/pico/hyperspace/hyperspace.lua.p8'
    output_file = '/home/terada/trees/itsmeterada/pico/picosystem_hyperspace/hyperspace_data.h'
    convert_p8_to_header(p8_file, output_file)
