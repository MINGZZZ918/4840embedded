from PIL import Image

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
        dist = (r - pr)**2 + (g - pg)**2 + (b - pb)**2
        if dist < min_dist:
            min_dist = dist
            best_index = idx
    return best_index

def image_to_mif(image_path, output_mif, size=16, width=4, depth=None):
    """
    将 PNG 转为 size×size 的 MIF 文件。
    参数:
      image_path – 输入 PNG 文件路径
      output_mif  – 输出 MIF 文件路径
      size        – 输出像素宽高（默认 16）
      width       – 数据位宽（4 位，支持 16 色）
      depth       – 地址深度（若为 None，则自动设为 size*size）
    """
    img = Image.open(image_path).convert('RGB')
    img = img.resize((size, size))
    pixels = list(img.getdata())

    if depth is None:
        depth = size * size

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
                val = 0x0F  # 超出部分填黑
            f.write(f"{addr} : {val:X};\n")
        f.write("END;\n")

# 示例用法
if __name__ == "__main__":
    image_to_mif("1.png", "1_16x16.mif")
