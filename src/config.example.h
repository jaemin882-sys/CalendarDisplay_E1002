#pragma once

// Copy this file to config.h and edit before upload.

#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Public iCloud calendar ICS feed(s).
// Apple webcal:// URLs are automatically converted to https://.
// Keep unused feeds as empty strings.
#define CALENDAR_1_NAME "개인"
#define CALENDAR_1_URL  "webcal://pXX-caldav.icloud.com/published/2/EXAMPLE"
#define CALENDAR_2_NAME "가족"
#define CALENDAR_2_URL  ""
#define CALENDAR_3_NAME ""
#define CALENDAR_3_URL  ""
#define CALENDAR_4_NAME ""
#define CALENDAR_4_URL  ""

// Text shown in the memo area above the monthly calendar.
#define CUSTOM_MEMO_TEXT "이번 달 메모를 여기에 적으세요"

// Korea Standard Time. Change if needed.
#define TZ_INFO "KST-9"

// Automatic refresh interval while sleeping.
#define AUTO_SYNC_HOURS 24
