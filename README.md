# CalendarDisplay_E1002

Seeed Studio **reTerminal E1002**용 월간 Apple/iCloud Calendar 디스플레이 프로젝트입니다.

## v0.1 목표

- 7.3" 800×480 Spectra 6 컬러 E-Ink
- 한 달 달력 7열 × 최대 6주 표시
- 날짜 칸 안에 일정 최대 3개 표시
- 초과 일정은 `+N`으로 표시
- 최대 4개 iCloud 공개 ICS 캘린더
- 캘린더별 색상 구분
- 상단 사용자 메모
- 한국 시간 기준
- 24시간마다 자동 동기화
- GPIO4 버튼으로 즉시 수동 동기화
- 내용이 바뀌지 않았으면 E-Ink 전체 갱신 생략
- Deep Sleep

## 프로젝트 구조

```text
CalendarDisplay_E1002/
├─ platformio.ini
├─ .gitignore
└─ src/
   ├─ main.cpp
   ├─ firmware_core.inc
   ├─ firmware_ics.inc
   ├─ firmware_ui.inc
   ├─ firmware_app.inc
   └─ config.example.h
```

`main.cpp`가 기능별 파일을 include하는 구조입니다.

## 설정

`src/config.example.h`를 `src/config.h`로 복사한 후 수정합니다.

```cpp
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#define CALENDAR_1_NAME "개인"
#define CALENDAR_1_URL  "webcal://..."

#define CUSTOM_MEMO_TEXT "이번 달 메모"
#define AUTO_SYNC_HOURS 24
```

`src/config.h`는 Git에 올라가지 않도록 `.gitignore`에 포함되어 있습니다.

## PlatformIO

1. VS Code + PlatformIO 설치
2. 저장소 열기
3. `config.example.h` → `config.h` 복사
4. Wi-Fi / Calendar URL 입력
5. E1002 USB-C 연결
6. Upload
7. Serial Monitor 115200

## 현재 제한

v0.1은 기본 VEVENT의 `DTSTART`, `DTEND`, `SUMMARY`, 종일 일정과 ICS line folding을 처리합니다.

아직 **RRULE 반복 일정 / EXDATE / RECURRENCE-ID** 확장은 포함하지 않았습니다. 실제 iCloud Calendar 피드를 확인한 뒤 v0.2에서 추가할 예정입니다.

## 폰트

현재 실기기 펌웨어는 U8g2의 Gulim 한글 bitmap font를 사용합니다.

PC 미리보기에서 확인한 **Noto Sans KR 계열 스타일**이 더 깔끔하므로, E1002 도착 후 Flash/RAM 여유와 렌더링 품질을 확인해 Noto Sans KR 서브셋 폰트로 교체하는 것을 다음 단계로 잡습니다.

## E1002 도착 후 체크

- [ ] 기본 컬러 패널 출력
- [ ] GPIO4 버튼 wake
- [ ] Wi-Fi / NTP
- [ ] 실제 iCloud ICS
- [ ] 한글 일정명
- [ ] 반복 일정
- [ ] 24시간 Deep Sleep 소비전력
- [ ] Noto Sans KR 폰트 적용
