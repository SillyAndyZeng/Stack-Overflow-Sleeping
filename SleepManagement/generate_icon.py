#!/usr/bin/env python3
"""
generate_icon.py
================
生成月亮主题的应用图标文件 (app_icon.ico + app_icon.png)。

使用纯 Python 标准库，无需 Pillow 等外部依赖。
ICO 容器内嵌 32-bit BGRA BMP DIB 位图数据。

兼容性：Windows .ico 格式，32×32 像素，32 位色深。
"""

import struct
import os
import zlib
import struct

SIZE = 32  # 图标尺寸 32×32


def create_moon_pixels(size: int) -> list[tuple[int, int, int, int]]:
    """
    生成月亮图标的 BGRA 像素数据。

    绘制元素：
    - 深蓝色圆角方形背景 (#1a1a3e)
    - 金色月牙 (使用两个圆形相减形成)
    - 三颗白色小星星

    返回: [(B, G, R, A), ...] 共 size*size 个像素，行优先。
    """
    pixels = []

    cx, cy = size * 0.38, size * 0.38  # 月亮中心
    r = size * 0.34  # 月亮半径
    mask_cx = cx + r * 0.35  # 遮罩中心偏移
    mask_cy = cy - r * 0.3
    mask_r = r * 0.78  # 遮罩半径

    # 星星位置
    stars = [
        (size * 0.70, size * 0.28, size * 0.055),
        (size * 0.78, size * 0.52, size * 0.045),
        (size * 0.60, size * 0.72, size * 0.045),
        (size * 0.30, size * 0.72, size * 0.04),
    ]

    # 圆角矩形判定
    corner_r = size * 0.2

    for py in range(size):
        for px in range(size):
            # 圆角矩形背景判定
            # 计算到四个角的距离
            corners = [
                (corner_r, corner_r),  # 左上
                (size - 1 - corner_r, corner_r),  # 右上
                (corner_r, size - 1 - corner_r),  # 左下
                (size - 1 - corner_r, size - 1 - corner_r),  # 右下
            ]
            inside_bg = True
            for ccx, ccy in corners:
                if px < corner_r and py < corner_r and ccx == corner_r and ccy == corner_r:
                    if (px - ccx) ** 2 + (py - ccy) ** 2 > corner_r ** 2:
                        inside_bg = False
                elif px > size - 1 - corner_r and py < corner_r and ccx == size - 1 - corner_r and ccy == corner_r:
                    if (px - ccx) ** 2 + (py - ccy) ** 2 > corner_r ** 2:
                        inside_bg = False
                elif px < corner_r and py > size - 1 - corner_r and ccx == corner_r and ccy == size - 1 - corner_r:
                    if (px - ccx) ** 2 + (py - ccy) ** 2 > corner_r ** 2:
                        inside_bg = False
                elif px > size - 1 - corner_r and py > size - 1 - corner_r and ccx == size - 1 - corner_r and ccy == size - 1 - corner_r:
                    if (px - ccx) ** 2 + (py - ccy) ** 2 > corner_r ** 2:
                        inside_bg = False

            if not inside_bg or px < 1 or py < 1 or px >= size - 1 or py >= size - 1:
                # 完全透明（圆角外部）
                pixels.append((0, 0, 0, 0))
                continue

            # 背景深蓝色
            bg_b, bg_g, bg_r, bg_a = 0x2C, 0x3E, 0x6B, 0xFF

            # 月亮：金色
            moon_r, moon_g, moon_b = 0x00, 0xD7, 0xFF  # 金色 #FFD700 → BGRA: (0x00, 0xD7, 0xFF, FF)
            # 实际上 #FFD700 的 BGRA 是 B=0x00, G=0xD7, R=0xFF, A=0xFF

            dx = px - cx
            dy = py - cy
            dist = (dx * dx + dy * dy) ** 0.5

            # 星星：白色
            is_star = False

            if dist <= r:
                # 在月亮圆内，检查遮罩
                mdx = px - mask_cx
                mdy = py - mask_cy
                mask_dist = (mdx * mdx + mdy * mdy) ** 0.5
                if mask_dist <= mask_r:
                    # 遮罩区域：透明（显示背景）
                    pass  # 继续走背景色逻辑
                else:
                    # 月亮可见区域
                    pixels.append((0x00, 0xD7, 0xFF, 0xFF))  # 金色 BGRA
                    continue
            else:
                # 检查星星
                for sx, sy, sr in stars:
                    sdx = px - sx
                    sdy = py - sy
                    if (sdx * sdx + sdy * sdy) ** 0.5 <= sr:
                        pixels.append((0xFF, 0xFF, 0xFF, 0xFF))  # 白色 BGRA
                        is_star = True
                        break

            if is_star:
                continue

            # 背景色
            pixels.append((bg_b, bg_g, bg_r, bg_a))

    return pixels


def write_ico(filepath: str, size: int, pixels: list):
    """
    将像素数据写入 .ico 文件（BMP DIB 格式）。

    ICO 文件结构:
    - ICONDIR (6 bytes)
    - ICONDIRENTRY (16 bytes)
    - BITMAPINFOHEADER (40 bytes)
    - XOR mask (pixel data, bottom-up)
    - AND mask (1bpp transparency mask)
    """
    bpp = 32
    row_size = ((size * bpp + 31) // 32) * 4  # 每行对齐到 4 字节
    xor_size = row_size * size
    and_row_size = ((size + 31) // 32) * 4
    and_size = and_row_size * size
    data_size = 40 + xor_size + and_size  # BITMAPINFOHEADER + XOR + AND
    offset = 6 + 16  # ICONDIR + ICONDIRENTRY

    with open(filepath, 'wb') as f:
        # --- ICONDIR ---
        f.write(struct.pack('<HHH', 0, 1, 1))  # reserved, type=icon, count=1

        # --- ICONDIRENTRY ---
        # bWidth, bHeight, bColorCount, bReserved
        f.write(struct.pack('<BBBB', size if size < 256 else 0,
                            size if size < 256 else 0, 0, 0))
        # wPlanes, wBitCount
        f.write(struct.pack('<HH', 1, bpp))
        # dwBytesInRes, dwImageOffset
        f.write(struct.pack('<II', data_size, offset))

        # --- BITMAPINFOHEADER ---
        f.write(struct.pack('<I', 40))        # biSize
        f.write(struct.pack('<i', size))       # biWidth
        f.write(struct.pack('<i', size * 2))   # biHeight (double for ICO)
        f.write(struct.pack('<HH', 1, bpp))    # biPlanes, biBitCount
        f.write(struct.pack('<I', 0))          # biCompression (BI_RGB)
        f.write(struct.pack('<I', xor_size + and_size))  # biSizeImage
        f.write(struct.pack('<i', 0))          # biXPelsPerMeter
        f.write(struct.pack('<i', 0))          # biYPelsPerMeter
        f.write(struct.pack('<I', 0))          # biClrUsed
        f.write(struct.pack('<I', 0))          # biClrImportant

        # --- XOR mask (bottom-up BGRA) ---
        # 像素是行优先的，ICO 要求 bottom-up
        for y in range(size - 1, -1, -1):
            row_start = y * size
            for x in range(size):
                idx = row_start + x
                if idx < len(pixels):
                    b, g, r, a = pixels[idx]
                    f.write(struct.pack('BBBB', b, g, r, a))
                else:
                    f.write(struct.pack('BBBB', 0, 0, 0, 0))
            # 行补齐
            padding = row_size - size * 4
            if padding > 0:
                f.write(b'\x00' * padding)

        # --- AND mask (1bpp, transparent pixels = 1) ---
        for y in range(size - 1, -1, -1):
            row_start = y * size
            for x in range(0, size, 8):
                byte_val = 0
                for bit in range(8):
                    px_idx = row_start + x + bit
                    if px_idx >= len(pixels):
                        byte_val |= (1 << (7 - bit))
                    else:
                        _, _, _, a = pixels[px_idx]
                        if a < 128:
                            byte_val |= (1 << (7 - bit))
                f.write(struct.pack('B', byte_val))
            # AND 行补齐
            padding = and_row_size - ((size + 7) // 8)
            if padding > 0:
                f.write(b'\x00' * padding)

    print(f'✅ 已生成: {filepath} ({size}×{size}, {bpp}bit)')


def write_png(filepath: str, size: int, pixels: list):
    """
    将像素数据保存为 PNG 文件（纯标准库实现）。
    """
    import struct
    import zlib

    def write_chunk(chunk_type: bytes, data: bytes):
        chunk = chunk_type + data
        f.write(struct.pack('>I', len(data)))
        f.write(chunk)
        f.write(struct.pack('>I', zlib.crc32(chunk) & 0xFFFFFFFF))

    # RGBA 像素（从 BGRA 转换）
    raw_data = bytearray()
    for y in range(size):
        raw_data.append(0)  # filter byte (None)
        for x in range(size):
            idx = y * size + x
            if idx < len(pixels):
                b, g, r, a = pixels[idx]
                raw_data.extend([r, g, b, a])
            else:
                raw_data.extend([0, 0, 0, 0])

    with open(filepath, 'wb') as f:
        # PNG signature
        f.write(b'\x89PNG\r\n\x1a\n')
        # IHDR
        ihdr_data = struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)
        write_chunk(b'IHDR', ihdr_data)
        # IDAT
        compressed = zlib.compress(bytes(raw_data))
        write_chunk(b'IDAT', compressed)
        # IEND
        write_chunk(b'IEND', b'')

    print(f'✅ 已生成: {filepath} ({size}×{size}, RGBA PNG)')


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))

    print('🔨 生成月亮应用图标...')
    pixels = create_moon_pixels(SIZE)

    ico_path = os.path.join(script_dir, 'app_icon.ico')
    write_ico(ico_path, SIZE, pixels)

    png_path = os.path.join(script_dir, 'app_icon.png')
    write_png(png_path, SIZE, pixels)

    print('\n✨ 完成！将 app_icon.ico 添加到项目中。')
    print('   同时生成 app_icon.png 供 Qt 资源文件使用。')


if __name__ == '__main__':
    main()
