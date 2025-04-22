from PIL import Image

def image_to_mif(image_path, output_mif, width=8, depth=4096):
    img = Image.open(image_path).convert('L')
    img = img.resize((32, 32))
    pixels = list(img.getdata())

    with open(output_mif, 'w') as f:
        f.write(f"WIDTH={width};\n")
        f.write(f"DEPTH={depth};\n")
        f.write("ADDRESS_RADIX=DEC;\n")
        f.write("DATA_RADIX=HEX;\n")
        f.write("CONTENT BEGIN\n")

        for addr in range(depth):
            val = pixels[addr] >> 4 if addr < len(pixels) else 0xF
            f.write(f"{addr} : {val:X};\n")

        f.write("END;\n")

# Usage
image_to_mif("1.png", "1.mif")
