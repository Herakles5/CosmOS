import struct, json
with open("/opt/meinos/model_10.glb", "rb") as f:
    magic, version, length = struct.unpack("<4sII", f.read(12))
    chunk_len, chunk_type = struct.unpack("<II", f.read(8))
    json_data = f.read(chunk_len).decode("utf-8")
    parsed = json.loads(json_data)
    
    bin_chunk_len, bin_chunk_type = struct.unpack("<II", f.read(8))
    bin_data = f.read(bin_chunk_len)
    
    # image 0 uses bufferView 9
    bv = parsed["bufferViews"][9]
    offset = bv.get("byteOffset", 0)
    size = bv["byteLength"]
    img_data = bin_data[offset:offset+size]
    
    with open("lion_texture.png", "wb") as out:
        out.write(img_data)
