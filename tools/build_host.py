"""Build the actual firmware renderer or font atlas with pinned U8g2 sources."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / '.pio/libdeps/seeed_xiao_esp32s3/U8g2_for_Adafruit_GFX/src'

def build_host(build, source='host_render.cpp', sanitize=False):
    build = Path(build)
    build.mkdir(parents=True, exist_ok=True)
    fonts = build / 'fonts.o'
    subprocess.run(['gcc', '-c', '-O2', '-DU8G2_USE_LARGE_FONTS', '-ffunction-sections', '-fdata-sections',
                    str(LIB / 'u8g2_fonts.c'), '-o', str(fonts)], check=True)
    binary = build / (Path(source).stem + ('.exe' if os.name == 'nt' else ''))
    flags = ['-fsanitize=address,undefined', '-g'] if sanitize and os.name != 'nt' else []
    subprocess.run(['g++', '-std=c++17', '-O2', *flags, '-ffunction-sections',
                    '-fdata-sections', '-Wl,--gc-sections', '-I' + str(ROOT / 'tests/host_gfx'),
                    '-I' + str(LIB), '-I' + str(build),
                    '-I' + str(ROOT / '.pio/libdeps/seeed_xiao_esp32s3/ArduinoJson/src'),
                    str(ROOT / 'tests' / source), str(LIB / 'U8g2_for_Adafruit_GFX.cpp'),
                    str(fonts), '-o', str(binary)], check=True)
    return binary
