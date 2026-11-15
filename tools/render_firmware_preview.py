"""Developer QA: render the actual firmware UI and U8g2 fonts on a host PC.
Requires g++ and installed PlatformIO dependencies. No device, network or config.h.
python tools/render_firmware_preview.py sample.ics /tmp/calendar.ppm [holidays.ics]
"""
import subprocess,sys,tempfile
from build_host import build_host
with tempfile.TemporaryDirectory(prefix='e1002-render-') as build:
    subprocess.run([str(build_host(build)),*sys.argv[1:]],check=True)
