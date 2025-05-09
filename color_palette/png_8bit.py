#!/usr/bin/env python3
import os
import re
from PIL import Image

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

def read_indexed_mif(mif_path):
    """读取8-bit索引MIF内容，返回索引值列表"""
    values = []
    in_content = False
    with open(mif_path, "r") as f:
        for line in f:
            line = line.strip()
            if not in_content:
                if line.upper().startswith("CONTENT BEGIN"):
                    in_content = True
                continue
            if line.upper().startswith("END"):
                break
            if ':' in line:
                match = re.match(r"(\w+)\s*:\s*([0-9A-Fa-f]+);", line)
                if match:
                    idx = int(match.group(2), 16)
                    values.append(idx)
    return values

def write_pngs_from_indices(indices, width, height, output_dir):
    os.makedirs(output_dir, exist_ok=True)
    palette = build_color_palette()
    pixels_per_image = width * height
    num_images = len(indices) // pixels_per_image

    for i in range(num_images):
        img = Image.new("RGB", (width, height))
        for idx in range(pixels_per_image):
            addr = i * pixels_per_image + idx
            color_idx = indices[addr]
            rgb = palette[color_idx]
            x, y = idx % width, idx // width
            img.putpixel((x, y), rgb)
        img.save(os.path.join(output_dir, f"sprite_{i}.png"))
    print(f"✅ 共生成 {num_images} 张 PNG，保存至 '{output_dir}'")

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    mif_path = os.path.join(script_dir, "8bit_sprites.mif")
    output_dir = os.path.join(script_dir, "output_images")

    width, height = 16, 16
    indices = read_indexed_mif(mif_path)
    write_pngs_from_indices(indices, width, height, output_dir)

if __name__ == "__main__":
    main()
