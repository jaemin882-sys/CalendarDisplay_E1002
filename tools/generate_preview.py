"""Regenerate the offline illustrative calendar after renderer/layout changes."""
import json
import re
from pathlib import Path
from preview import layout_payload
ROOT=Path(__file__).resolve().parents[1]

def offline_html(payload):
    fonts=(ROOT/'tools/font_atlas.json').read_text(encoding='utf8')
    renderer=(ROOT/'tools/preview_canvas.js').read_text(encoding='utf8')
    return ('<!doctype html><html lang="ko"><meta charset="utf-8"><title>E1002 미리보기</title>'
            '<style>body{background:#eee;font:14px sans-serif;padding:24px}main{width:800px;margin:auto}'
            'canvas{background:white;image-rendering:pixelated}p{color:#444}</style><main>'
            '<p>800 × 480 · 고정 예시 (일정·공휴일·날씨 모두 테스트 데이터)</p>'
            '<canvas width="800" height="480"></canvas><p id="message"></p>'
            '<p>날씨: Open-Meteo · 과거 값은 저장 당시 예보이며 실제 관측값이 아닙니다.</p></main>'
            '<script>const p='+json.dumps(payload,ensure_ascii=False,separators=(',',':'))+
            ',fonts='+fonts+',layout='+json.dumps(layout_payload())+';\n'+renderer+'</script>')

if __name__=='__main__':
    path=ROOT/'preview.html'
    payload=json.loads(re.search(r'const p=(.*?),fonts=',path.read_text(encoding='utf8')).group(1))
    # Illustrative fixtures only; never read the user's config or location.
    payload['weather']={
        '2026-09-27':{'low':80,'high':170,'code':0},
        '2026-09-28':{'low':100,'high':210,'code':2},
        '2026-09-29':{'low':80,'high':170,'code':3},
        '2026-09-30':{'low':-120,'high':-20,'code':71},
        '2026-10-01':{'low':-30,'high':110,'code':61},
        '2026-10-02':{'low':100,'high':230,'code':95},
        '2026-10-03':{'low':80,'high':170,'code':0},
        '2026-10-09':{'low':60,'high':170,'code':3}}
    path.write_text(offline_html(payload),encoding='utf8')
