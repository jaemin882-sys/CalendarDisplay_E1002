# 제3자 자료

## Display / font software

Firmware dependencies are pinned in platformio.ini. Their original licenses apply.

- Seeed GxEPD2 fork: https://github.com/Seeed-Projects/Seeed_GxEPD2/tree/1100ea37c16b910fd79152f4250c13d802b9c20b
- U8g2 for Adafruit GFX: https://github.com/olikraus/U8g2_for_Adafruit_GFX/tree/82d2b3eea866e7d40266672b41e5c8306ee97403
- Original font source used by the preview atlas: `src/u8g2_fonts.c` in that pinned U8g2 repository. `tests/font_atlas.cpp` contains the transformation source; font glyph pixels are not redesigned.

Unifont: Copyright (C) 1998–2019 Roman Czyborra, Paul Hardy, Qianqian Fang, Andrew Miller, Johnnie Weaver, David Corbett, et al. Licensed GPLv2 or later with the GNU Font Embedding Exception. See https://unifoundry.com/LICENSE.txt and https://www.gnu.org/licenses/old-licenses/gpl-2.0.html . These terms apply to the derived Unifont glyph data in tools/font_atlas.json and embedded in preview.html; the Font Embedding Exception concerns embedding fonts, not removal of the font's own license. Font source is available at the pinned upstream URL above.

Helvetica bitmap source carries: Copyright (c) 1984, 1987 Adobe Systems Incorporated. All Rights Reserved. Copyright (c) 1988, 1991 Digital Equipment Corporation. All Rights Reserved. Original X11 bitmap font terms accompany the upstream font source. See https://github.com/olikraus/u8g2/tree/master/tools/font/bdf .

U8g2 adapter software is copyright (c) 2018, olikraus@gmail.com, under the BSD 2-clause-style terms in its source headers. The host mock graphics adapter and tests are project code; upstream font decoder is used from the dependency, not copied into this repository.

## Trust roots

src/tls_roots.h contains 16 public root CA PEM certificates selected from the Mozilla CA distribution via certifi 2026.7.22: DigiCert, GTS, ISRG, USERTrust and Sectigo subject families. Source: https://github.com/certifi/python-certifi ; certifi uses the Mozilla Public License 2.0 (https://www.mozilla.org/MPL/2.0/). Root certificates are public trust material, not private keys. Selection is intentionally finite; future server CA changes require maintenance.

## Holiday feed

Google Calendar Korea official holiday public ICS is used at runtime. Live holiday data and private iCloud data are not committed. The offline screenshot/example contains a small illustrative September–October 2026 calendar; it is not an authoritative holiday dataset.
