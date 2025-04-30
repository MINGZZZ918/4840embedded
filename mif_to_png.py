#!/usr/bin/env python3
import os
import re
from PIL import Image

# --------------------
# 配置区域：根据实际情况修改
# --------------------
MIF_FILE = "images.mif"   # 输入的 .mif 文件名
WIDTH    = 16             # 每张 sprite 的宽度（像素）
HEIGHT   = 16             # 每张 sprite 的高度（像素）
OUTPUT_DIR = "output_images"  # 输出 PNG 的文件夹

def read_mif_values(mif_path):
    """从 MIF 文件中解析出所有的 RRGGBB HEX 值列表"""
    values = []
    in_content = False
    with open(mif_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not in_content:
                if line.upper().startswith("CONTENT BEGIN"):
                    in_content = True
                continue
            if line.upper().startswith("END"):
                break
            if ':' in line:
                # 格式: 地址 : RRGGBB;
                parts = re.split(r'\s*:\s*|\s*;\s*', line)
                if len(parts) >= 2 and parts[1]:
                    values.append(parts[1])
    return values

def mif_to_pngs(mif_file, width, height, output_dir):
    """将 MIF 转换成多张 PNG"""
    vals = read_mif_values(mif_file)
    total_pixels = len(vals)
    per_image = width * height
    img_count = total_pixels // per_image

    os.makedirs(output_dir, exist_ok=True)

    for i in range(img_count):
        img = Image.new('RGB', (width, height))
        for idx in range(per_image):
            hexval = vals[i * per_image + idx]
            r = int(hexval[0:2], 16)
            g = int(hexval[2:4], 16)
            b = int(hexval[4:6], 16)
            x, y = idx % width, idx // width
            img.putpixel((x, y), (r, g, b))
        out_path = os.path.join(output_dir, f"sprite_{i}.png")
        img.save(out_path)

    print(f"✅ 已保存 {img_count} 张 PNG 到文件夹 '{output_dir}'")

if __name__ == "__main__":
    mif_to_pngs(MIF_FILE, WIDTH, HEIGHT, OUTPUT_DIR)
