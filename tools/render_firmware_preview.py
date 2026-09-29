"""Developer QA: render the actual firmware UI and U8g2 fonts on a host PC.
Requires g++ and installed PlatformIO dependencies. No device, network or config.h.
python tools/render_firmware_preview.py sample.ics /tmp/calendar.ppm [holidays.ics]
"""
from pathlib import Path
import subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parents[1]
lib=ROOT/'.pio/libdeps/seeed_xiao_esp32s3/U8g2_for_Adafruit_GFX/src'
build=Path(tempfile.mkdtemp(prefix='e1002-render-'))
# Keep only linked font sections in the executable.
subprocess.run(['gcc','-c','-O2','-ffunction-sections','-fdata-sections',str(lib/'u8g2_fonts.c'),'-o',str(build/'fonts.o')],check=True)
subprocess.run(['g++','-std=c++17','-O2','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(ROOT/'tests/host_gfx'),'-I'+str(lib),str(ROOT/'tests/host_render.cpp'),str(lib/'U8g2_for_Adafruit_GFX.cpp'),str(build/'fonts.o'),'-o',str(build/'render')],check=True)
subprocess.run([str(build/'render'),*sys.argv[1:]],check=True)
