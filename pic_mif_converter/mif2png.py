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
    """
    解析 MIF 文件，返回一个 {address: value} 的字典
    """
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
                    data[addr] = int(val_hex, 16)
                except ValueError:
                    pass
    return data

def mif16_to_png(mif_path, output_path):
    """
    将 16×16 MIF 转为 PNG。
    默认图像尺寸为 16×16，超过部分用黑色填充（索引 0x0F）。
    """
    width = height = 16
    data = parse_mif(mif_path)

    img = Image.new('RGB', (width, height))
    pixels = img.load()

    for addr in range(width * height):
        idx = data.get(addr, 0x0F)            # 默认黑色
        rgb = color_palette.get(idx, (0,0,0))
        x = addr % width
        y = addr // width
        pixels[x, y] = rgb

    img.save(output_path)
    print(f"Saved 16×16 image to: {output_path}")

# 示例用法
if __name__ == "__main__":
    mif16_to_png("1_16x16.mif", "out_16x16.png")