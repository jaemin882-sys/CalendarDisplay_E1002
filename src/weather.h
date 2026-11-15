#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

static constexpr int WEATHER_FORECAST_DAYS=16;
// The rolling calendar starts 14..20 days before today (Sunday..Saturday).
static constexpr int WEATHER_HISTORY_DAYS=20;
static constexpr int WEATHER_CAPACITY=WEATHER_HISTORY_DAYS+WEATHER_FORECAST_DAYS;
struct DailyWeather {
  uint32_t day=0; // Days since 1970-01-01, using the Asia/Seoul civil date.
  int16_t low=0,high=0; // Tenths Celsius; past values are forecasts, not observations.
  uint8_t code=0;
};
struct WeatherCache {DailyWeather days[WEATHER_CAPACITY];uint8_t count=0;};
enum class WeatherIcon {Sun,PartlyCloudy,Cloud,Rain,Snow};
inline WeatherIcon weatherIcon(uint8_t code){
  switch(code){
    case 71:case 73:case 75:case 77:case 85:case 86:return WeatherIcon::Snow;
    case 51:case 53:case 55:case 56:case 57:case 61:case 63:case 65:
    case 66:case 67:case 80:case 81:case 82:case 95:case 96:case 97:case 99:return WeatherIcon::Rain;
    case 0:return WeatherIcon::Sun;
    case 1:case 2:return WeatherIcon::PartlyCloudy;
    default:return WeatherIcon::Cloud;
  }
}
inline bool validWeatherCode(int code){
  switch(code){
    case 0:case 1:case 2:case 3:case 45:case 48:
    case 51:case 53:case 55:case 56:case 57:case 61:case 63:case 65:case 66:case 67:
    case 71:case 73:case 75:case 77:case 80:case 81:case 82:case 85:case 86:
    case 95:case 96:case 97:case 99:return true;
    default:return false;
  }
}
inline uint32_t weatherDay(int64_t timestamp){return (uint32_t)((timestamp+9*3600)/86400);}
inline bool weatherCoordinate(const char* text,double limit,double& value){
  if(!text || !*text)return false;
  char* end=nullptr;value=strtod(text,&end);
  return end!=text && *end==0 && isfinite(value) && value>=-limit && value<=limit;
}
inline uint32_t weatherDate(const char* date){
  if(!date || strlen(date)!=10 || date[4]!='-' || date[7]!='-')return 0;
  for(int i=0;i<10;++i)if(i!=4 && i!=7 && (date[i]<'0'||date[i]>'9'))return 0;
  int y=atoi(date),m=atoi(date+5),d=atoi(date+8);
  if(y<2024 || y>2099 || m<1 || m>12)return 0;
  const int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
  auto leap=[](int year){return year%4==0 && (year%100!=0 || year%400==0);};
  if(d<1 || d>lengths[m-1]+(m==2 && leap(y)))return 0;
  uint32_t result=0;
  for(int year=1970;year<y;++year)result+=365+leap(year);
  for(int month=1;month<m;++month)result+=lengths[month-1]+(month==2 && leap(y));
  return result+d-1;
}
inline const DailyWeather* weatherForDay(const WeatherCache& cache,uint32_t day){
  for(int i=0;i<cache.count;++i)if(cache.days[i].day==day)return &cache.days[i];
  return nullptr;
}
inline int weatherDegrees(int tenths){return (tenths<0?-1:1)*((abs(tenths)+5)/10);}
inline bool validWeather(const DailyWeather& w){
  return w.day>=19723 && w.day<=47481 && w.low>=-1000 && w.high<=1000 &&
    w.low<=w.high && validWeatherCode(w.code);
}
// Successful responses replace today/future, including missing/null forecasts.
// Failure never calls this function; historical records are copied verbatim.
inline WeatherCache mergeWeather(const WeatherCache& old,const WeatherCache& forecast,uint32_t today){
  WeatherCache next;
  for(int i=0;i<old.count;++i){
    const auto& w=old.days[i];
    if(w.day+WEATHER_HISTORY_DAYS>=today && w.day<today)next.days[next.count++]=w;
  }
  for(int i=0;i<forecast.count && next.count<WEATHER_CAPACITY;++i){
    const auto& w=forecast.days[i];
    if(w.day>=today && w.day<today+WEATHER_FORECAST_DAYS)next.days[next.count++]=w;
  }
  return next;
}

// Explicit little-endian encoding avoids struct padding/ABI and checks all bytes.
static constexpr size_t WEATHER_BLOB_SIZE=8+WEATHER_CAPACITY*9+4;
inline void weatherPut32(uint8_t* p,uint32_t n){for(int i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i));}
inline uint32_t weatherGet32(const uint8_t* p){uint32_t n=0;for(int i=0;i<4;++i)n|=(uint32_t)p[i]<<(8*i);return n;}
inline uint32_t weatherCrc(const uint8_t* data,size_t size){
  uint32_t crc=0xffffffffu;
  for(size_t i=0;i<size;++i){crc^=data[i];for(int j=0;j<8;++j)crc=(crc>>1)^(0xedb88320u & (0u-(crc&1)));}
  return ~crc;
}
inline void encodeWeather(const WeatherCache& cache,uint8_t (&data)[WEATHER_BLOB_SIZE]){
  memset(data,0,sizeof(data));memcpy(data,"WX01",4);data[4]=cache.count;
  for(int i=0;i<cache.count;++i){
    uint8_t* p=data+8+i*9;const auto& w=cache.days[i];weatherPut32(p,w.day);
    p[4]=(uint8_t)w.low;p[5]=(uint8_t)(w.low>>8);p[6]=(uint8_t)w.high;p[7]=(uint8_t)(w.high>>8);p[8]=w.code;
  }
  weatherPut32(data+sizeof(data)-4,weatherCrc(data,sizeof(data)-4));
}
inline bool decodeWeather(const uint8_t* data,size_t size,WeatherCache& cache){
  if(size!=WEATHER_BLOB_SIZE || memcmp(data,"WX01",4) || data[4]>WEATHER_CAPACITY ||
     data[5] || data[6] || data[7] || weatherGet32(data+size-4)!=weatherCrc(data,size-4))return false;
  WeatherCache next;next.count=data[4];
  for(int i=0;i<next.count;++i){
    const uint8_t* p=data+8+i*9;auto& w=next.days[i];w.day=weatherGet32(p);
    w.low=(int16_t)((uint16_t)p[4]|((uint16_t)p[5]<<8));
    w.high=(int16_t)((uint16_t)p[6]|((uint16_t)p[7]<<8));w.code=p[8];
    if(!validWeather(w) || (i && w.day<=next.days[i-1].day))return false;
  }
  cache=next;return true;
}
