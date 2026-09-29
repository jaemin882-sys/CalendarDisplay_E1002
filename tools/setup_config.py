from pathlib import Path
import getpass

ROOT = Path(__file__).resolve().parents[1]
target = ROOT / "src" / "config.h"

def esc(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')

print("CalendarDisplay E1002 설정 파일 생성")
print()

ssid = input("Wi-Fi 이름(SSID): ").strip()
password = getpass.getpass("Wi-Fi 비밀번호: ")
calendar_name = input("캘린더 이름 [개인]: ").strip() or "개인"
calendar_url = input("iCloud 공개 캘린더 webcal:// 주소: ").strip()
memo = input("상단 메모 [이번 달 메모]: ").strip() or "이번 달 메모"

content = f'''#pragma once

#define WIFI_SSID "{esc(ssid)}"
#define WIFI_PASSWORD "{esc(password)}"

#define CALENDAR_1_NAME "{esc(calendar_name)}"
#define CALENDAR_1_URL  "{esc(calendar_url)}"
#define CALENDAR_2_NAME ""
#define CALENDAR_2_URL  ""
#define CALENDAR_3_NAME ""
#define CALENDAR_3_URL  ""
#define CALENDAR_4_NAME ""
#define CALENDAR_4_URL  ""

#define CUSTOM_MEMO_TEXT "{esc(memo)}"
#define TZ_INFO "KST-9"
#define AUTO_SYNC_HOURS 24
'''

target.write_text(content, encoding="utf-8")
print()
print(f"생성 완료: {target}")
print("이 파일은 .gitignore에 포함되어 GitHub에 올라가지 않습니다.")
