# CalendarDisplay_E1002

Seeed Studio **reTerminal E1002**를 냉장고용 월간 Apple/iCloud Calendar로 사용하는 프로젝트입니다.

## 현재 목표 사양

- 7.3" 800×480 Spectra 6 컬러 E-Ink
- 월간 달력 7열 × 최대 6주
- 날짜 칸 안에 일정 최대 3개, 초과 일정은 `+N`
- 최대 4개 iCloud 공개 ICS 캘린더
- 캘린더별 색상 구분
- 상단 사용자 메모
- 한국 시간 기준
- 24시간마다 자동 동기화
- **E1002 오른쪽 KEY0(GPIO3)** 버튼으로 즉시 수동 동기화
- 동기화 결과가 같으면 자동 갱신 생략
- Wi-Fi/iCloud 일시 오류 시 **마지막 정상 E-Ink 화면 유지**
- Deep Sleep
- 기본 반복 일정 RRULE 지원

## Apple 반복 일정

현재 다음 RRULE을 현재 월 화면에 확장합니다.

- `FREQ=DAILY`
- `FREQ=WEEKLY` + `BYDAY`
- `FREQ=MONTHLY`
- `FREQ=YEARLY`
- `INTERVAL`
- `COUNT`
- `UNTIL`
- `EXDATE`

복잡한 규칙(예: 매월 두 번째 월요일처럼 ordinal BYDAY)과 일부 `RECURRENCE-ID` 수정 예외는 실제 iCloud 피드로 추가 검증할 예정입니다.

## 가장 쉬운 설정 방법

저장소를 받은 뒤 PC에서:

```bash
python tools/setup_config.py
```

를 실행하면 아래 내용을 차례로 물어보고 `src/config.h`를 자동 생성합니다.

1. 집 Wi-Fi 이름
2. Wi-Fi 비밀번호
3. 캘린더 이름
4. iCloud 공개 캘린더 `webcal://` 주소
5. 화면 상단 메모

`src/config.h`는 `.gitignore` 처리되어 GitHub에 업로드되지 않습니다.

## iCloud Calendar 주소 준비

Apple Calendar에서 표시하고 싶은 iCloud 캘린더의 **공개 캘린더 링크**를 복사합니다. 보통 `webcal://...` 형태이며 펌웨어가 자동으로 `https://`로 바꿉니다.

공개 캘린더 링크는 링크를 아는 사람이 접근할 수 있으므로 민감한 일정에는 주의하세요. 추후 비공개 CalDAV 방식으로 확장할 수 있습니다.

## E1002 도착 당일

### 1. 개발 환경

권장: VS Code + PlatformIO

### 2. 저장소 받기

```bash
git clone https://github.com/jaemin882-sys/CalendarDisplay_E1002.git
cd CalendarDisplay_E1002
```

### 3. 설정 생성

```bash
python tools/setup_config.py
```

### 4. E1002 연결

USB-C **데이터 케이블**로 PC에 연결하고 본체 전원을 켭니다.

### 5. 업로드

```bash
pio run -t upload
```

또는 VS Code PlatformIO의 **Upload** 버튼을 누릅니다.

### 6. 정상 동작

첫 정상 동기화 시:

```text
전원/버튼 Wake
→ Wi-Fi
→ NTP 시간 동기화
→ iCloud ICS 다운로드
→ 월간 달력 생성
→ E-Ink 갱신
→ Deep Sleep
```

이후에는 24시간마다 자동 확인합니다.

당장 반영하고 싶은 일정이 있으면 **오른쪽 KEY0 버튼**을 한 번 눌러 수동 동기화합니다.

## 실패 시 동작

- 설정값이 비어 있음 → 화면에 설정 필요 안내
- 첫 Wi-Fi/iCloud 연결 실패 → 화면에 오류 안내
- 이미 정상 달력이 표시된 뒤 일시적 네트워크 오류 → **기존 화면 그대로 유지**
- 다음 24시간 자동 동기화 또는 KEY0 버튼에서 다시 시도

## 프로젝트 구조

```text
CalendarDisplay_E1002/
├─ .github/workflows/build.yml
├─ platformio.ini
├─ .gitignore
├─ tools/
│  └─ setup_config.py
└─ src/
   ├─ main.cpp
   ├─ firmware_core.inc
   ├─ firmware_ics.inc
   ├─ firmware_ui.inc
   ├─ firmware_app.inc
   └─ config.example.h
```

## PlatformIO 설정

- Board: Seeed XIAO ESP32-S3
- Upload: 115200
- PSRAM: OPI
- Display: GDEP073E01 / Spectra 6
- E-Ink SPI: SCK 7 / MOSI 9 / CS 10 / DC 11 / RST 12 / BUSY 13
- Manual wake: E1002 KEY0 / GPIO3

## 폰트

실기기 첫 버전은 안정성을 위해 U8g2 한글 Gulim bitmap font를 사용합니다.

PC 미리보기에서는 Noto Sans KR 계열이 더 깔끔했기 때문에, E1002에서 기본 기능을 확인한 후 **Noto Sans KR 서브셋 폰트**로 교체할 예정입니다. 기능 검증보다 폰트 변경을 먼저 하지는 않습니다.

## 자동 빌드

GitHub Actions에서 PlatformIO 빌드를 수행하도록 `.github/workflows/build.yml`을 추가했습니다. 기기 없이도 기본 컴파일 오류를 먼저 확인하기 위한 용도입니다.

## 실기기에서 최종 확인할 것

- [ ] Spectra 6 실제 색감/방향
- [ ] KEY0(GPIO3) wake
- [ ] Wi-Fi 연결
- [ ] 실제 iCloud ICS
- [ ] 한글 일정명
- [ ] 실제 반복 일정 / 예외 일정
- [ ] 화면 갱신 시간
- [ ] 24시간 Deep Sleep 배터리 소비
- [ ] Noto Sans KR 폰트 적용 여부
