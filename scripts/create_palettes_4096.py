import os
import argparse
import struct

def create_palettes_4096(filename):
    """
    Create a binary file with some discrete palette color values.

    :param filename: The name of the file to be created.
    """

    palettes_bytes = bytearray()
    discrete_values = (0x0, 0x4, 0x7, 0x9, 0xB, 0xD, 0xF)
    total = 0
    exit_loops = False
    for i in discrete_values:
        for j in discrete_values:
            for k in discrete_values:
                total += 1
                first_byte = (0 << 4) | i
                second_byte = (j << 4) | k
                palettes_bytes += struct.pack('BB', second_byte, first_byte)
                if total > 256:
                    exit_loops = True
                    break
            if exit_loops:
                break
        if exit_loops:
            break;

    with open(filename, 'wb') as file:
        file.write(palettes_bytes)

    print(f"File '{filename}' created with 4x4x4 palette values.")

def main():
    parser = argparse.ArgumentParser(description='Generate a file with some discrete palette color values, using the mask FF0F: ')
    parser.add_argument('filename', type=str, help='Name of the file to create.')

    args = parser.parse_args()
    create_palettes_4096(args.filename)

if __name__ == '__main__':
    main()

