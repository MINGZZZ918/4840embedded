#!/usr/bin/env python3
import os
from PIL import Image

def main():
    # 1. 图片文件夹路径
    script_dir = os.path.dirname(os.path.abspath(__file__))
    image_dir = os.path.join(script_dir, "image")
    
    # 2. 获取所有 PNG 文件并排序
    png_files = sorted([
        f for f in os.listdir(image_dir)
        if f.lower().endswith(".png")
    ])
    if not png_files:
        print("Error: 未在 'image' 文件夹中找到任何 PNG 文件。")
        return
    
    # 3. 加载并检查尺寸一致性
    images = []
    width = height = None
    for fname in png_files:
        path = os.path.join(image_dir, fname)
        img = Image.open(path).convert("RGBA")
        img = img.resize((16, 16), Image.NEAREST)
        if width is None:
            width, height = img.size
        else:
            if img.size != (width, height):
                raise ValueError(f"图像 '{fname}' 大小 {img.size} 与 {width}x{height} 不一致")
        images.append(img)
    
    # 4. 计算总像素数
    total_pixels = width * height * len(images)
    
    # 5. 写入 MIF 文件
    mif_path = os.path.join(script_dir, "images.mif")
    with open(mif_path, "w") as mif:
        mif.write(f"WIDTH=24;\n")
        mif.write(f"DEPTH={total_pixels};\n")
        mif.write("ADDRESS_RADIX=DEC;\n")
        mif.write("DATA_RADIX=HEX;\n")
        mif.write("CONTENT BEGIN\n")
        
        addr = 0
        for img in images:
            pixels = img.load()
            for y in range(height):
                for x in range(width):
                    r, g, b, a = pixels[x, y]
                    # 透明哨兵：alpha==0 输出全黑
                    if a == 0:
                        val = 0x000000
                    else:
                        val = (r << 16) | (g << 8) | b
                    mif.write(f"{addr} : {val:06X};\n")
                    addr += 1
        
        mif.write("END;\n")
    
    print(f"✅ 已生成 'images.mif' ({len(images)} 张 {width}x{height} 图像，深度 {total_pixels})")

if __name__ == "__main__":
    main()

