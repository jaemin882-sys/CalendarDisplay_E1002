#define main render_main
#include "host_render.cpp"
#undef main
#include "../src/firmware_weather_json.inc"
#include "../src/guest_mode.h"
#include <cassert>

const char* testLatitude="0";
const char* testLongitude="0";
#define WEATHER_LATITUDE testLatitude
#define WEATHER_LONGITUDE testLongitude
struct MemoryPreferences {
 std::vector<uint8_t> bytes;int writes=0;bool fail=false;
 bool isKey(const char*){return !bytes.empty();}
 size_t getBytesLength(const char*){return bytes.size();}
 size_t getBytes(const char*,void* out,size_t n){if(n<bytes.size())return 0;memcpy(out,bytes.data(),bytes.size());return bytes.size();}
 size_t putBytes(const char*,const void* data,size_t n){if(fail)return 0;++writes;const auto* p=(const uint8_t*)data;bytes.assign(p,p+n);return n;}
} prefs;
WeatherCache downloaded;bool apiOk=true;
bool downloadWeather(uint32_t,WeatherCache& out){if(!apiOk)return false;out=downloaded;return true;}
time_t testTime=1793458800; // 2026-11-01 00:00 KST
time_t mockTime(time_t*){return testTime;}
#define time mockTime
// Extracted verbatim from firmware_app.inc by test_weather.py.
#include "weather_store_under_test.inc"
#undef time

int main(int argc,char** argv){
 if(argc==3){
  std::ifstream f(argv[1]);std::string json((std::istreambuf_iterator<char>(f)),{});
  WeatherCache result;result.count=1;result.days[0]={weatherDate("2026-10-31"),80,170,0};
  bool ok=parseWeatherJson(json.c_str(),json.size(),weatherDate("2026-11-01"),result);
  assert(ok==(atoi(argv[2])!=0));
  if(!ok)assert(result.count==1 && result.days[0].day==weatherDate("2026-10-31"));
  return 0;
 }
 assert(classifyKey0(false,true)==Key0Action::Auto);
 assert(classifyKey0(true,false)==Key0Action::Refresh);
 assert(classifyKey0(true,true)==Key0Action::SwitchMode);
 assert(!guestModeAfterAction(false,Key0Action::Refresh));
 assert(guestModeAfterAction(false,Key0Action::SwitchMode));
 assert(!guestModeAfterAction(true,Key0Action::SwitchMode));
 double coordinate=0;
 assert(!weatherCoordinate("",90,coordinate));assert(!weatherCoordinate("nan",90,coordinate));
 assert(!weatherCoordinate("91",90,coordinate));assert(!weatherCoordinate("1x",90,coordinate));
 assert(weatherCoordinate("0",90,coordinate));assert(weatherCoordinate("-180",180,coordinate));
 assert(weatherDate("2026-02-29")==0 && weatherDate("2024-02-29")!=0);
 assert(weatherDay(testTime)==weatherDate("2026-11-01"));
 assert(weatherDay(testTime-1)==weatherDate("2026-10-31"));
 assert(weatherIcon(97)==WeatherIcon::Rain && weatherIcon(67)==WeatherIcon::Rain);
 assert(weatherIcon(85)==WeatherIcon::Snow && weatherIcon(48)==WeatherIcon::Cloud);
 assert(weatherDegrees(-15)==-2 && weatherDegrees(-4)==0 && weatherDegrees(175)==18);
 uint32_t today=weatherDay(testTime);
 WeatherCache old;
 for(int i=21;i>=0;--i)old.days[old.count++]={today-(uint32_t)i,-120,100,71};
 for(int i=0;i<16;++i)downloaded.days[downloaded.count++]={today+(uint32_t)i,80,170,0};
 auto merged=mergeWeather(old,downloaded,today);
 assert(merged.count==36 && merged.days[0].day==today-20);
 assert(weatherForDay(merged,today-1)->low==-120 && weatherForDay(merged,today)->low==80);
 assert(!weatherForDay(merged,today-21) && !weatherForDay(merged,today+16));
 auto missing=mergeWeather(merged,WeatherCache(),today);
 assert(missing.count==20 && !weatherForDay(missing,today));
 uint8_t encoded[WEATHER_BLOB_SIZE];encodeWeather(merged,encoded);
 WeatherCache restored;assert(decodeWeather(encoded,sizeof(encoded),restored));
 for(size_t i=0;i<sizeof(encoded);++i){encoded[i]^=1;assert(!decodeWeather(encoded,sizeof(encoded),restored));encoded[i]^=1;}
 assert(!decodeWeather(encoded,sizeof(encoded)-1,restored));
 // Semantic corruption is rejected even with a matching CRC.
 encoded[4]=37;weatherPut32(encoded+sizeof(encoded)-4,weatherCrc(encoded,sizeof(encoded)-4));
 assert(!decodeWeather(encoded,sizeof(encoded),restored));
 weatherCache=old;weatherEnabled=true;syncWeather(true,true);assert(prefs.writes==1);
 auto stored=prefs.bytes;syncWeather(true,true);assert(prefs.writes==1 && prefs.bytes==stored);
 weatherCache=WeatherCache();loadWeatherCache();assert(weatherCache.count==36); // Reboot/deep sleep.
 syncWeather(false,true);syncWeather(true,false);assert(prefs.bytes==stored && prefs.writes==1);
 testLatitude="";loadWeatherCache();assert(!weatherEnabled);syncWeather(true,true);assert(prefs.bytes==stored && prefs.writes==1);
 testLatitude="0";loadWeatherCache();assert(weatherEnabled);
 apiOk=false;syncWeather(true,true);assert(prefs.bytes==stored && prefs.writes==1 && weatherCache.count==36);
 apiOk=true;testTime+=86400;
 // The previous day's last saved forecast is frozen; older incoming values cannot replace it.
 downloaded.days[0].low=-990;syncWeather(true,true);assert(weatherForDay(weatherCache,today)->low==80);
 assert(weatherForDay(weatherCache,today-19) && weatherForDay(weatherCache,today-1));
 stored=prefs.bytes;int writes=prefs.writes;
 downloaded.days[1].high=250;prefs.fail=true;syncWeather(true,true);
 assert(prefs.bytes==stored && prefs.writes==writes && weatherForDay(weatherCache,today+1)->high==170);
 prefs.fail=false;syncWeather(true,true);assert(prefs.writes==writes+1);
 // Corrupt NVS never publishes unvalidated values.
 prefs.bytes[12]^=1;weatherCache=WeatherCache();loadWeatherCache();assert(weatherCache.count==0);
 // No historical record is fabricated when the first successful fetch is today.
 weatherCache=mergeWeather(WeatherCache(),downloaded,today+1);assert(!weatherForDay(weatherCache,today));
 setenv("TZ",TZ_INFO,1);tzset();u8g2.begin(display);bool ad=false;
 ws=parseIcsDateTime("20261018",&ad);we=ws+42*86400;
 weatherCache=downloaded;weatherEnabled=true;
 uint32_t hash=computeRenderHash(ws,testTime,true);
 assert(hash==computeRenderHash(ws,testTime,true));
 weatherCache.days[1].high+=1;assert(hash==computeRenderHash(ws,testTime,true));
 weatherCache.days[1].high+=10;assert(hash!=computeRenderHash(ws,testTime,true));
 hash=computeRenderHash(ws,testTime,true);weatherCache.days[1].code=95;
 assert(hash!=computeRenderHash(ws,testTime,true));hash=computeRenderHash(ws,testTime,true);
 weatherCache.days[1].code=97;assert(hash==computeRenderHash(ws,testTime,true));
 weatherEnabled=false;assert(hash!=computeRenderHash(ws,testTime,true));
 // Exhaust all supported displayed temperatures with long month/day labels.
 for(const char* date:{"11/1","12/1","11/30","12/31"}){
  selectFont(u8g2_font_helvB10_tf);int dateEnd=5+u8g2.getUTF8Width(date);
  for(int lo=-1000;lo<=1000;lo+=10)for(int hi=lo;hi<=1000;hi+=10){
   DailyWeather w={today,(int16_t)lo,(int16_t)hi,0};
   int left=drawDayWeather(112-UI_WEATHER_RIGHT,0,112-UI_WEATHER_RIGHT-dateEnd-UI_HEADER_GAP,w);
   assert(left>=dateEnd+UI_HEADER_GAP);
  }
 }
 std::cout<<"Weather storage, rollover, failure, CRC, hash and header bounds passed\n";
}
