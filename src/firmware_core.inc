#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <vector>
#include <algorithm>
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_sntp.h"
#include <SPI.h>
#include <Preferences.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include <GxEPD2_7C.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "config.h"
static_assert(AUTO_SYNC_HOURS>=1 && AUTO_SYNC_HOURS<=168,"AUTO_SYNC_HOURS must be 1..168");

#define EPD_SCK_PIN 7
#define EPD_MOSI_PIN 9
#define EPD_CS_PIN 10
#define EPD_DC_PIN 11
#define EPD_RES_PIN 12
#define EPD_BUSY_PIN 13
#define SYNC_BUTTON_PIN 3  // E1002 KEY0: right-side green wake button

SPIClass hspi(HSPI);

#define MAX_DISPLAY_BUFFER_SIZE 16000u
#define MAX_HEIGHT(EPD) \
  (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2) ? \
   EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2))

GxEPD2_7C<GxEPD2_730c_GDEP073E01, MAX_HEIGHT(GxEPD2_730c_GDEP073E01)>
  display(GxEPD2_730c_GDEP073E01(EPD_CS_PIN, EPD_DC_PIN, EPD_RES_PIN, EPD_BUSY_PIN));

U8G2_FOR_ADAFRUIT_GFX u8g2;
Preferences prefs;

static const uint16_t C_BLACK=GxEPD_BLACK;
static const uint16_t C_WHITE=GxEPD_WHITE;
static const uint16_t C_RED=GxEPD_RED;
static const uint16_t C_GREEN=GxEPD_GREEN;
static const uint16_t C_BLUE=GxEPD_BLUE;
static const uint16_t C_YELLOW=GxEPD_YELLOW;

struct CalendarFeed {
  const char* name;
  const char* url;
  uint16_t color;
};

static constexpr uint8_t HOLIDAY_FEED_INDEX=4;
#ifndef HOLIDAY_ICS_URL
#define HOLIDAY_ICS_URL \
  "https://calendar.google.com/calendar/ical/" \
  "ko.south_korea.official%23holiday%40group.v.calendar.google.com/public/basic.ics"
#endif
static const char* KOREA_HOLIDAY_ICS=HOLIDAY_ICS_URL;

CalendarFeed feeds[] = {
  {CALENDAR_1_NAME,CALENDAR_1_URL,C_BLUE},
  {CALENDAR_2_NAME,CALENDAR_2_URL,C_GREEN},
  {CALENDAR_3_NAME,CALENDAR_3_URL,C_RED},
  {CALENDAR_4_NAME,CALENDAR_4_URL,C_YELLOW},
  {"공휴일",KOREA_HOLIDAY_ICS,C_RED},
};

struct CalendarEvent {
  time_t start=0;
  time_t end=0;
  bool allDay=false;
  String summary;
  uint16_t color=C_BLUE;
  uint8_t calendarIndex=0;
};

static constexpr int MAX_EVENTS=512;
CalendarEvent events[MAX_EVENTS];
int eventCount=0;

String normalizeIcsUrl(String url){
  url.trim();
  if(url.startsWith("webcal://")) url="https://"+url.substring(9);
  return url;
}

#include "firmware_datetime.inc"

bool lineStartsWithProperty(const String& line,const char* prop){
  int colon=line.indexOf(':');
  if(colon<0) return false;
  String left=line.substring(0,colon);
  int semi=left.indexOf(';');
  if(semi>=0) left=left.substring(0,semi);
  return left==prop;
}

String propertyValue(const String& line){
  int colon=line.indexOf(':');
  return colon>=0 ? line.substring(colon+1) : String();
}

bool connectWiFi(uint32_t timeoutMs=18000){
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  uint32_t start=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-start<timeoutMs) delay(250);
  return WiFi.status()==WL_CONNECTED;
}

static volatile bool clockUpdated=false;
void onClockSync(struct timeval*){clockUpdated=true;}
bool syncClock(){
  clockUpdated=false;
  sntp_set_time_sync_notification_cb(onClockSync);
  configTzTime(TZ_INFO,"pool.ntp.org","time.google.com","time.cloudflare.com");
  uint32_t started=millis();
  while(!clockUpdated && millis()-started<15000) delay(100);
  bool retained=(esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_TIMER ||
                 esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_EXT1) && time(nullptr)>1704067200;
  Serial.println(clockUpdated?"Clock: fresh NTP":(retained?"Clock: NTP timeout, using retained RTC":"Clock: unavailable"));
  return clockUpdated || retained;
}

bool configurationLooksValid(){
  String ssid=WIFI_SSID;
  bool hasCalendar=false;
  for(uint8_t i=0;i<HOLIDAY_FEED_INDEX;++i){
    String url=normalizeIcsUrl(feeds[i].url);
    if(url.isEmpty()) continue;
    if(!url.startsWith("https://") || url.indexOf("EXAMPLE")>=0) return false;
    hasCalendar=true;
  }
  ssid.trim();


  if(ssid.isEmpty() || ssid=="YOUR_WIFI_NAME") return false;
  if(!hasCalendar) return false;
  return true;
}

void displayWindow(time_t& windowStart,time_t& windowEnd){
  struct tm now={};
  getLocalTime(&now,100);
  now.tm_hour=0;
  now.tm_min=0;
  now.tm_sec=0;
  now.tm_isdst=-1;

  time_t todayMidnight=mktime(&now);
  int daysSinceSunday=now.tm_wday;
  windowStart=todayMidnight-(time_t)(daysSinceSunday+14)*86400;
  windowEnd=windowStart+(time_t)42*86400;
}
