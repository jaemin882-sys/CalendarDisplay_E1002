#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <SPI.h>
#include <Preferences.h>
#include <GxEPD2_7C.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "config.h"

#define EPD_SCK_PIN 7
#define EPD_MOSI_PIN 9
#define EPD_CS_PIN 10
#define EPD_DC_PIN 11
#define EPD_RES_PIN 12
#define EPD_BUSY_PIN 13
#define SYNC_BUTTON_PIN 4

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

CalendarFeed feeds[] = {
  {CALENDAR_1_NAME,CALENDAR_1_URL,C_BLUE},
  {CALENDAR_2_NAME,CALENDAR_2_URL,C_GREEN},
  {CALENDAR_3_NAME,CALENDAR_3_URL,C_RED},
  {CALENDAR_4_NAME,CALENDAR_4_URL,C_YELLOW},
};

struct CalendarEvent {
  time_t start=0;
  time_t end=0;
  bool allDay=false;
  String summary;
  uint16_t color=C_BLUE;
  uint8_t calendarIndex=0;
};

static constexpr int MAX_EVENTS=160;
CalendarEvent events[MAX_EVENTS];
int eventCount=0;

String normalizeIcsUrl(String url){
  url.trim();
  if(url.startsWith("webcal://")) url="https://"+url.substring(9);
  return url;
}

String icsUnescape(String s){
  s.replace("\\n"," ");
  s.replace("\\N"," ");
  s.replace("\\,",",");
  s.replace("\\;",";");
  s.replace("\\\\","\\");
  return s;
}

time_t parseIcsDateTime(const String& value,bool* isAllDay){
  String v=value;
  v.trim();
  *isAllDay=(v.length()==8);
  if(v.length()<8) return 0;

  struct tm t={};
  t.tm_year=v.substring(0,4).toInt()-1900;
  t.tm_mon=v.substring(4,6).toInt()-1;
  t.tm_mday=v.substring(6,8).toInt();
  t.tm_isdst=-1;

  if(v.length()>=15 && v.charAt(8)=='T'){
    t.tm_hour=v.substring(9,11).toInt();
    t.tm_min=v.substring(11,13).toInt();
    t.tm_sec=v.substring(13,15).toInt();
  }
  return v.endsWith("Z") ? timegm(&t) : mktime(&t);
}

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
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  uint32_t start=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-start<timeoutMs) delay(250);
  return WiFi.status()==WL_CONNECTED;
}

bool syncClock(){
  configTzTime(TZ_INFO,"pool.ntp.org","time.google.com","time.cloudflare.com");
  struct tm now={};
  for(int i=0;i<30;i++){
    if(getLocalTime(&now,500)) return true;
    delay(250);
  }
  return false;
}

bool configurationLooksValid(){
  String ssid=WIFI_SSID;
  String cal1=CALENDAR_1_URL;
  ssid.trim();
  cal1.trim();

  if(ssid.isEmpty() || ssid=="YOUR_WIFI_NAME") return false;
  if(cal1.isEmpty() || cal1.indexOf("EXAMPLE")>=0) return false;
  return true;
}

void currentMonthWindow(time_t& monthStart,time_t& nextMonth){
  struct tm now={};
  getLocalTime(&now,100);
  now.tm_mday=1;
  now.tm_hour=0;
  now.tm_min=0;
  now.tm_sec=0;
  now.tm_isdst=-1;
  monthStart=mktime(&now);

  now.tm_mon+=1;
  nextMonth=mktime(&now);
}
