#define HOST_RENDER
#include "host_parser.cpp"
#include "U8g2_for_Adafruit_GFX.h"
using std::min;using std::max;
#define CUSTOM_MEMO_TEXT "온이네집"
constexpr uint16_t C_WHITE=0xffff,C_BLACK=0,C_RED=0xf800,C_BLUE=0x001f,C_GREEN=0x07e0,C_YELLOW=0xffe0;
constexpr uint8_t HOLIDAY_FEED_INDEX=4;
int batteryPercent=-1;
Adafruit_GFX display;
U8G2_FOR_ADAFRUIT_GFX u8g2;
#include "../src/weather.h"
WeatherCache weatherCache;
bool weatherEnabled=false;
#include "../src/firmware_ui.inc"
int main(int argc,char** argv){
 if(argc<3)return 2;
 setenv("TZ",TZ_INFO,1);tzset();bool ad=false;
 ws=parseIcsDateTime("20260913",&ad);we=parseIcsDateTime("20261025",&ad);
 time_t today=parseIcsDateTime("20260929",&ad);
 if(argc>4){ws=parseIcsDateTime(argv[4],&ad);we=ws+42*86400;}
 if(argc>5)today=parseIcsDateTime(argv[5],&ad);
 if(argc>6){
  std::ifstream f(argv[6]);uint32_t day;int lo,hi,code;
  while(f>>day>>lo>>hi>>code){auto& w=weatherCache.days[weatherCache.count++];w.day=day;w.low=lo;w.high=hi;w.code=code;}
  weatherEnabled=true;
 }
 if(argc>7)batteryPercent=atoi(argv[7]);
 hostNow=today;
 std::ifstream f(argv[1]);std::string raw((std::istreambuf_iterator<char>(f)),{});
 if(!parseIcsText(raw.c_str(),0))return 1;
 if(argc>3){std::ifstream h(argv[3]);std::string holidays((std::istreambuf_iterator<char>(h)),{});if(!parseIcsText(holidays.c_str(),4))return 1;}
 sortEvents();u8g2.begin(display);u8g2.setFontMode(1);u8g2.setFontDirection(0);
 if(argc>8 && std::string(argv[8])=="guest")drawGuestPage(today,true);
 else drawCalendarPage(today,true);
 std::ofstream out(argv[2],std::ios::binary);out<<"P6\n800 480\n255\n";
 for(auto& row:display.pixels)for(auto p:row){char rgb[3]={(char)((p>>11)*255/31),(char)(((p>>5)&63)*255/63),(char)((p&31)*255/31)};out.write(rgb,3);}
 return 0;
}
