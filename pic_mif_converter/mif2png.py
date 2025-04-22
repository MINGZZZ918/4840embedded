from PIL import Image

# 与 Verilog 中一致的颜色查找表（Color Lookup Table）
color_palette = {
    0x00: (0xf0, 0xf0, 0xf0),
    0x01: (0xb0, 0xa0, 0xa0),
    0x02: (0xa0, 0xa0, 0xb0),
    0x03: (0xa0, 0xa0, 0xa0),
    0x04: (0xb0, 0x30, 0x20),
    0x05: (0xb0, 0x20, 0x20),
    0x06: (0xe0, 0xe0, 0x90),
    0x07: (0xe0, 0x90, 0x20),
    0x08: (0xe0, 0x90, 0x10),
    0x09: (0x90, 0x40, 0x00),
    0x0a: (0x60, 0x60, 0x60),
    0x0b: (0x60, 0x60, 0x00),
    0x0c: (0x60, 0x00, 0x00),
    0x0d: (0x50, 0x00, 0x70),
    0x0e: (0x00, 0x40, 0x40),
    0x0f: (0x00, 0x00, 0x00)
}

def parse_mif(filepath):
    in_content = False
    data = {}

    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith("CONTENT BEGIN"):
                in_content = True
                continue
            if line.startswith("END;"):
                break
            if in_content and ':' in line:
                try:
                    addr_part, val_part = line.split(':')
                    addr = int(addr_part.strip())
                    val_hex = val_part.strip().strip(';')
                    val = int(val_hex, 16)
                    data[addr] = val
                except:
                    pass
    return data

def mif_to_image_color(mif_path, output_path, width=32, height=32):
    data = parse_mif(mif_path)

    img = Image.new('RGB', (width, height))
    pixels = img.load()

    for i in range(width * height):
        color_idx = data.get(i, 0x0F)
        rgb = color_palette.get(color_idx, (0, 0, 0))
        x = i % width
        y = i // width
        if y < height:
            pixels[x, y] = rgb

    img.save(output_path)
    print(f"Saved: {output_path}")

# 示例用法
mif_to_image_color("1.mif", "2.png")
