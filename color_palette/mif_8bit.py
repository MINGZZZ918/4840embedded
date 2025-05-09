#!/usr/bin/env python3
import os
from PIL import Image

# --------------------
# 配置区域
# --------------------
PNG_DIR = "output_images"          # 输入 PNG 文件夹
WIDTH = 16                         # 每张图宽度
HEIGHT = 16                        # 每张图高度
MIF_FILE = "8bit_sprites.mif"      # 输出 MIF 文件名

def build_color_palette():
    """构造 Verilog 使用的 256 色调色板"""
    palette = []
    for r in range(6):
        for g in range(6):
            for b in range(6):
                palette.append((r * 51, g * 51, b * 51))
    for i in range(256 - len(palette)):
        palette.append(((i * 47) % 256, (i * 91) % 256, (i * 137) % 256))
    return palette

def rgb_to_index(rgb, palette):
    """返回最接近颜色的调色板索引"""
    min_dist = float('inf')
    best_idx = 0
    for i, p in enumerate(palette):
        dist = sum((a - b) ** 2 for a, b in zip(rgb, p))
        if dist < min_dist:
            min_dist = dist
            best_idx = i
    return best_idx

def pngs_to_indexed_mif(png_dir, width, height, mif_file):
    palette = build_color_palette()
    entries = []
    addr = 0

    # 按名字排序
    png_files = sorted([f for f in os.listdir(png_dir) if f.lower().endswith(".png")])
    if not png_files:
        print("❌ 没有找到 PNG 图片")
        return

    for filename in png_files:
        path = os.path.join(png_dir, filename)
        img = Image.open(path).convert('RGB')
        if img.size != (width, height):
            raise ValueError(f"{filename} 大小不是 {width}x{height}")
        for y in range(height):
            for x in range(width):
                rgb = img.getpixel((x, y))
                index = rgb_to_index(rgb, palette)
                entries.append(f"    {addr:04X} : {index:02X};")
                addr += 1

    depth = len(entries)
    with open(mif_file, "w") as f:
        f.write(f"""-- 8-bit palette indexed sprites (.mif) generated from PNGs
WIDTH=8;
DEPTH={depth};

ADDRESS_RADIX=HEX;
DATA_RADIX=HEX;

CONTENT BEGIN
""")
        f.write("\n".join(entries))
        f.write("\nEND;\n")

    print(f"✅ 已保存 MIF 文件: {mif_file}")
    print(f"📦 总共转换了 {len(png_files)} 张图片，共 {depth} 个像素 ({depth // (width * height)} 张 sprite)")

if __name__ == "__main__":
    pngs_to_indexed_mif(PNG_DIR, WIDTH, HEIGHT, MIF_FILE)
