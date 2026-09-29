"""Windows-friendly local configuration; never prints credentials or public URLs."""
from pathlib import Path
import getpass
from urllib.parse import urlparse
ROOT=Path(__file__).resolve().parents[1]
TARGET=ROOT/'src/config.h'
def esc(value):
    return value.replace('\\','\\\\').replace('"','\\"').replace('\r','\\r').replace('\n','\\n').replace('\t','\\t')
def valid_url(url):
    p=urlparse(url.replace('webcal://','https://',1))
    return p.scheme=='https' and bool(p.hostname) and not p.username and 'EXAMPLE' not in url

def main():
    print('CalendarDisplay E1002 설정 — 설정값은 이 PC에만 저장됩니다.')
    if TARGET.exists() and input('기존 config.h를 덮어쓸까요? [y/N]: ').lower()!='y':
        print('기존 설정을 유지합니다.');return
    ssid=input('2.4GHz Wi-Fi 이름(SSID): ')
    if not ssid or ssid=='YOUR_WIFI_NAME':raise ValueError('Wi-Fi 이름이 필요합니다.')
    password=getpass.getpass('Wi-Fi 비밀번호 (화면에 표시되지 않음): ')
    feeds=[]
    for i in range(1,5):
        url=input(f'캘린더 {i} 공개 webcal:// 또는 https:// 주소'+(' (추가 없으면 엔터)' if i>1 else '')+': ').strip()
        if not url and i>1:break
        if not valid_url(url):raise ValueError('올바른 HTTPS/webcal 공개 캘린더 주소가 필요합니다.')
        name=input(f'캘린더 {i} 이름 [가족]: ').strip() or '가족'
        feeds.append((name,url))
    title=input('상단 제목 [온이네집]: ').strip() or '온이네집'
    lines=['#pragma once','',f'#define WIFI_SSID "{esc(ssid)}"',f'#define WIFI_PASSWORD "{esc(password)}"','']
    for i in range(4):
        name,url=feeds[i] if i<len(feeds) else ('','')
        lines.extend([f'#define CALENDAR_{i+1}_NAME "{esc(name)}"',f'#define CALENDAR_{i+1}_URL "{esc(url)}"'])
    lines.extend(['',f'#define CUSTOM_MEMO_TEXT "{esc(title)}"','#define TZ_INFO "KST-9"','#define AUTO_SYNC_HOURS 24',''])
    TARGET.write_text('\n'.join(lines),encoding='utf-8')
    print('src/config.h 생성 완료. 이제 PlatformIO Upload를 실행하세요.')
if __name__=='__main__':
    try:main()
    except (ValueError,EOFError,KeyboardInterrupt) as exc:
        print(f'설정 취소: {exc}');raise SystemExit(1)
