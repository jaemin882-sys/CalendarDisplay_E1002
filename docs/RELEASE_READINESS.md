# v1.0.0 최종 점검 결과

검토 기준 원본: `fabbb252205ac4f0d262821f6fd7cca898c05f7c`. 실기기는 미도착 상태이며, 아래의 성공은 PC 빌드/테스트 결과입니다.

## 검증 결과

- PlatformIO **clean build 성공**, compiler warning/error 없음.
- PlatformIO 6.2.0 / espressif32 7.1.3 / Arduino ESP32 2.0.17 계열 framework package `4.20017.260907+sha.dcc1105b`.
- Seeed GxEPD2 `1100ea37c16b910fd79152f4250c13d802b9c20b`, U8g2 adapter `82d2b3eea866e7d40266672b41e5c8306ee97403` 고정.
- **회귀 테스트 14개 통과**: 실제 firmware C++ 파서 ASan/UBSan, Python 주요 반복 규칙, 실제 UI/U8g2와 Canvas 전체 384,000픽셀 비교.
- 이 실행 환경의 LeakSanitizer는 ptrace 제약으로 비활성화했습니다. 메모리 누수 검증을 완료했다고 주장하지 않습니다.
- 실제 Google 한국 official holiday ICS **67,344 bytes 다운로드 및 C++ 파싱 성공**. 2026-09-13~10-24 범위의 추석 연휴, 개천절/대체일, 한글날 6개 항목 확인. 소스의 일정 정확성을 정부 자료와 모두 대조한 것은 아닙니다.
- 사용자 실제 iCloud 링크는 제공되지 않았으므로 Apple 계정의 실제 피드와 ESP32 TLS 접속은 미검증입니다.
- GitHub Actions는 `.github/workflows/build.yml`에서 같은 빌드/테스트를 실행합니다. 원본 커밋 조회에는 workflow run이 없었으며, 수정본의 원격 실행 결과는 PR/Actions에서 별도 확인합니다.

## 주요 위험과 수정

| 우선순위 | 원본 문제 | 수정 |
|---|---|---|
| P0 | U8g2 폰트 변경 때 투명 모드 초기화 → 검은 글자 블록 | 모든 폰트 변경에서 투명 모드/흰 배경 재설정 |
| P0 | 오늘 날짜가 render hash에 없어 테두리가 멈춤 | 오늘/레이아웃 버전/공휴일 상태까지 해시에 포함 |
| P0 | HTTP 200 HTML/잘린 ICS도 성공으로 처리 | VCALENDAR 종료·VEVENT 구조·날짜·지원 범위 검증, 실패 시 피드 rollback |
| P0 | 공휴일 실패가 모든 개인 일정 갱신 차단 | 실패 분리 및 NVS 공휴일 캐시 |
| P0 | 오래된 비반복 일정이 160개 슬롯을 소진 | 표시 기간과 겹치는 occurrence만 저장, 512개로 확대 |
| P1 | RECURRENCE-ID 무시로 원본/수정본 중복 | 2-pass 예외 수집, 원래 occurrence 억제, 이동/취소 반영 |
| P1 | 격주 기준·ordinal BYDAY·EXDATE UTC·범위 직전 다일 일정 오류 | WKST와 날짜 필터/정확한 timestamp/구간 겹침 처리 |
| P1 | 2개×2줄 셀 밖 침범, 12개 초과 +N 오류 | 16px 본문 3줄 예산, 모든 일정 카운트, +N 공간 확보 |
| P1 | 매번 패널 초기화, refresh 중 Wi-Fi 활성 | 필요 시에만 초기화, 다운로드 후 Wi-Fi OFF |
| P1 | KEY0 누른 채 sleep 시 즉시 재기상 | 3초 release 대기, 계속 LOW면 timer-only sleep |
| P1 | setInsecure 및 무제한 String body 복사 | CA 검증, 크기 제한 PSRAM HTTP sink, 원문 재복사 제거 |
| P2 | 웹 폰트 프리뷰가 실제 펌웨어와 상이 | 실제 폰트 atlas와 공통 레이아웃 상수, 픽셀 회귀 비교 |
| P2 | 중복 Korean1/Korean2 폰트 | Korean2 단독으로 통합, 2,446개 본문 glyph 폭/bitmap 동일 검증 |

## 메모리 / 폰트

동일한 고정 라이브러리/툴체인을 적용해 원본과 수정본을 각각 빌드했습니다. 설정은 예시값이며 사용자가 입력하는 문자열 길이에 따라 소폭 달라질 수 있습니다.

| 항목 | 원본 | 최종 수정본 | 변화 |
|---|---:|---:|---:|
| 정적 RAM | 66,976 B | 78,272 B (23.9%) | +11,296 B |
| Flash 프로그램 | 994,861 B | 1,024,573 B (30.7%) | +29,712 B |
| 한글 font 배열 | 85,124 B | 70,709 B | −14,415 B |
| 이벤트 슬롯 | 160 | 512 | 표시 기간 내부만 저장 |

Flash 프로그램 파티션은 3,342,336 B, 보고된 내부 RAM 기준은 327,680 B입니다. 정적 RAM 수치는 TLS·String·PSRAM 등 실행 중 peak를 포함하지 않습니다. 현재 display 객체는 16,132 B, events 배열은 16,384 B, CA PEM 문자열은 21,027 B(Flash)입니다. 폰트 배열은 Flash에 남고 전체가 RAM으로 복사되지 않습니다.

- 본문/공휴일/집 이름: Unifont Korean2, 16px 비트맵. 표준 한글 2,350자와 ASCII 등을 포함한 2,446 glyph. 한글 전체 11,172자를 지원하는 것은 아닙니다.
- 제목: Helvetica Bold 14pt, 날짜/요일: Bold 10pt, +N: 8pt. 한글 본문을 줄여 정보량을 늘리지 않았습니다.
- 미지원 한글·한자·이모지는 `?`. UTF-8 continuation/overlong/surrogate를 확인하여 잘린 바이트로 줄바꿈하지 않습니다.
- Noto Sans KR 전체 삽입은 적용하지 않았습니다. 실제 패널에서 두께/획 뭉침을 평가하지 못했고, 동적 일정에는 현재 일정 문자만 추린 subset이 다음 일정에서 누락될 위험이 있습니다.
- Noto의 현실적 후속안은 **표준 한글 2,350자+ASCII+기호의 16~18px 1-bit subset** 또는 Noto 소형 subset+Unifont fallback입니다. 16×16 고정 1-bit 기준 2,350자 bitmap만 75,200 B(압축 전), 전체 11,172자는 357,504 B이고 인덱스/metrics는 별도입니다. 이는 산술적 크기 예시이며 실제 Noto 빌드 측정치가 아닙니다.
- 18px를 쓰면 셀당 3줄 유지가 어려워집니다. 현 단계에서는 서체 교체보다 투명 모드·16px 행간·정확한 줄바꿈의 개선을 채택했습니다.

## ICS 지원 범위

| 기능 | 범위 |
|---|---|
| DTSTART / DTEND | DATE / DATE-TIME, 유효 날짜 검증. 종료 시각은 제외 |
| DTEND 없음 | 종일 1일, 시간 일정은 해당 시각의 순간 일정(표시용 1초) |
| 시간대 | floating=한국시간, Asia/Seoul, Asia/Tokyo, UTC/Etc/UTC/GMT, UTC Z |
| Folding / escaping | 공백·탭 folding, \\n/\\N/\\,/\\;/\\\\. VALARM은 일정 필드와 분리 |
| 반복 | DAILY, WEEKLY, MONTHLY, YEARLY, INTERVAL, COUNT, UNTIL |
| 필터 | BYDAY, BYMONTHDAY(음수 포함), BYMONTH, WKST |
| ordinal BYDAY | MONTHLY의 2MO/-1FR 등. YEARLY는 BYMONTH가 있을 때 월 안의 ordinal만 |
| EXDATE | 다중 행/쉼표, DATE 또는 시간대가 지정된 timestamp |
| RECURRENCE-ID | UID+원래 시작 시각 기준 단일 instance 수정·이동·취소. 파일 순서 무관 |
| 다일 일정 | 화면 시작 전부터 이어지는 일정 및 DTEND 제외 처리 |
| 취소 | STATUS:CANCELLED master/instance |

**미지원**: BYSETPOS/BYYEARDAY/BYWEEKNO/BYHOUR/BYMINUTE/BYSECOND, RDATE/EXRULE/DURATION, RANGE=THISANDFUTURE, 임의 VTIMEZONE/DST 시간대. 지원하지 않는 속성은 조용히 무시하지 않고 해당 개인 피드를 실패 처리하여 이전 화면을 유지합니다. 오래된 일정의 미지원 규칙도 피드를 실패시킬 수 있습니다. 중복 UID master의 SEQUENCE/LAST-MODIFIED 병합은 하지 않습니다. 일반 iCloud 피드에서 master는 한 UID당 하나여야 합니다.

한국 달력 전용이며 TZ_INFO를 임의 DST 지역으로 바꾸면 day 계산은 지원하지 않습니다. 날짜 허용 범위는 1970–2099, 반복 스캔 상한은 80,000일입니다. 입력 RRULE이 RFC에 맞게 작성된 정상 피드라는 전제가 있으며 모든 악성/비표준 RFC 변형에 대한 완전한 파서는 아닙니다.

## 네트워크와 용량

- Wi-Fi 최대 18초, NTP 최대 15초. NTP 실패 시 timer/KEY0 wake에서 유지된 유효 RTC만 fallback.
- HTTP 연결/읽기 timeout 12초, TLS handshake 15초, redirect 최대 4회. 본문 sink 총 경과 45초 제한(마지막 읽기 timeout까지 추가될 수 있음).
- HTTPS만 허용, 선택한 공개 루트 CA 16개 사용, 인증 실패 시 insecure fallback 없음. redirect의 최종 인증서도 TLS client가 검증.
- ICS 최대 768KiB(PSRAM), PSRAM 없으면 96KiB. 원문 전체를 한 번만 저장하고 논리행 단위로 2회 읽음. chunked decoding은 HTTPClient 담당.
- 논리행 4KiB, 예외 128개, 이벤트당 EXDATE 256개, 표시 occurrence 총 512개. 상한 초과 시 실패/rollback. String과 vector는 유한 크기이나 동적 할당을 없애지는 않았으므로 실기기 peak heap 기록이 필요.
- 2-pass는 RECURRENCE-ID가 master 뒤에 나와도 처리하기 위한 최소 구조 변경입니다.
- 매 wake마다 deep sleep reboot로 transient heap은 초기화. NVS에는 화면 hash와 공휴일 cache만 저장하며 공휴일 내용이 같으면 다시 쓰지 않음.
- 공휴일 캐시는 이전에 수집한 표시 기간만 보유합니다. 장애가 길어져 새 주가 추가되면 그 주의 공휴일은 없을 수 있습니다. 오래된 개인 일정 전체를 캐시해 재배치하는 기능은 없습니다.

## 실기기 잔여 리스크

1. **패널/전원/SPI/BUSY**: 모델과 2MHz 핀맵은 공식 자료와 일치하지만 실제 갱신·방향·전원 안정성은 미검증. driver API는 완전한 성공 여부를 반환하지 않으므로 BUSY HIGH만으로 화면 갱신을 보장할 수 없습니다.
2. **실제 iCloud 내용과 TLS CA 체인**: 공개 링크가 없어서 검증하지 못함. 국외 TZID 또는 지원 밖 규칙이 있으면 피드 전체가 거절됨.
3. **KEY0 EXT1 / pull-up / USB CDC 재연결**: compile 확인만 완료. RTC_PERIPH ON을 유지하며 sleep 전 주변장치 출력을 설정하지만 실제 sleep 누설전류/SD 전원 상태는 실측 필요.
4. **배터리 수명**: 측정 전 추정 기간을 보장하지 않음. 24시간은 sleep 진입 기준이며 활성 시간이 더해짐.
5. **NVS/전원 중단**: 갱신 도중 전원이 끊기면 패널이 부분 갱신 상태일 수 있음. NVS hash와 물리 패널의 원자적 일치 보장은 불가능. KEY0로 강제 갱신.
6. **외부 공휴일 소스**: 최신성·서비스 지속성 보장 없음. 개인 일정과 분리하여 실패 영향 제한.

실제 수행 순서는 [HARDWARE_CHECKLIST.md](HARDWARE_CHECKLIST.md)를 따릅니다.

## 공식 참고 자료

- [Seeed E-series Arduino display cookbook](https://wiki.seeedstudio.com/reterminal_e10xx_with_arduino/): E1002 GDEP073E01/ED2208, 800×480, SCK7/MOSI9/CS10/DC11/RST12/BUSY13, SPI 2MHz.
- [Seeed onboard peripherals](https://wiki.seeedstudio.com/reterminal_e10xx_with_arduino_peripherals/): KEY0 GPIO3, LED GPIO6(active LOW), SD CS14/EN16, battery monitor enable21.
- [Espressif ESP32-S3 sleep API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/sleep_modes.html): EXT1 low wake and RTC pad considerations. 실제 사용 심볼은 고정 SDK 헤더와 build로 확인.
