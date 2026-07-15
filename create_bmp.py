import struct

width = 256
height = 256

file_size = 54 + 3 * width * height
header = struct.pack('<ccIIIIIIHH', b'B', b'M', file_size, 0, 54, 40, width, height, 1, 24)
header += struct.pack('<IIIIII', 0, 3 * width * height, 0, 0, 0, 0)

with open('image.bmp', 'wb') as f:
    f.write(header)
    for y in range(height):
        for x in range(width):
            r = int(x / width * 255)
            g = int(y / height * 255)
            b = 128
            f.write(struct.pack('<BBB', b, g, r))
