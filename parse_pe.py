import struct

with open("rufus-4.exe", "rb") as f:
    data = f.read()

if data[0:2] != b'MZ':
    print("Not MZ")
    exit(1)

pe_offset = struct.unpack("<I", data[0x3C:0x40])[0]
print(f"PE Offset: {pe_offset}")

if data[pe_offset:pe_offset+4] != b'PE\0\0':
    print("Not PE")
    exit(1)

num_sections = struct.unpack("<H", data[pe_offset+6:pe_offset+8])[0]
size_opt_header = struct.unpack("<H", data[pe_offset+20:pe_offset+22])[0]
magic = struct.unpack("<H", data[pe_offset+24:pe_offset+26])[0]
print(f"Num Sections: {num_sections}")
print(f"Size of Opt Header: {size_opt_header}")
print(f"Magic: {hex(magic)}")

size_of_image = struct.unpack("<I", data[pe_offset+80:pe_offset+84])[0]
print(f"SizeOfImage: {size_of_image}")

