from PIL import Image
import math

# Verilog 中颜色表（编号 -> RGB）
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

def closest_color_index(rgb):
    """返回最接近的颜色索引（0~15）"""
    r, g, b = rgb
    min_dist = float('inf')
    best_index = 0
    for idx, (pr, pg, pb) in color_palette.items():
        dist = (r - pr) ** 2 + (g - pg) ** 2 + (b - pb) ** 2
        if dist < min_dist:
            min_dist = dist
            best_index = idx
    return best_index

def image_to_mif(image_path, output_mif, width=8, depth=4096):
    img = Image.open(image_path).convert('RGB')
    img = img.resize((32, 32))
    pixels = list(img.getdata())

    with open(output_mif, 'w') as f:
        f.write(f"WIDTH={width};\n")
        f.write(f"DEPTH={depth};\n")
        f.write("ADDRESS_RADIX=DEC;\n")
        f.write("DATA_RADIX=HEX;\n")
        f.write("CONTENT BEGIN\n")

        for addr in range(depth):
            if addr < len(pixels):
                val = closest_color_index(pixels[addr])
            else:
                val = 0x0F  # 空白像素默认填黑色
            f.write(f"{addr} : {val:X};\n")

        f.write("END;\n")

# 示例用法
image_to_mif("1.png", "1.mif")
