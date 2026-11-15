#include <ArduinoJson.h>
bool parseWeatherJson(const char* data,size_t length,uint32_t today,WeatherCache& forecast){
  JsonDocument doc;
  if(deserializeJson(doc,data,length,DeserializationOption::NestingLimit(6)))return false;
  if(!doc["utc_offset_seconds"].is<int>() || doc["utc_offset_seconds"].as<int>()!=32400 ||
     strcmp(doc["timezone"]|"","Asia/Seoul") || doc["error"]==true)return false;
  JsonArrayConst dates=doc["daily"]["time"],low=doc["daily"]["temperature_2m_min"],
    high=doc["daily"]["temperature_2m_max"],codes=doc["daily"]["weather_code"];
  size_t n=dates.size();
  if(n<1 || n>WEATHER_FORECAST_DAYS || low.size()!=n || high.size()!=n || codes.size()!=n ||
     strcmp(doc["daily_units"]["temperature_2m_min"]|"","°C") ||
     strcmp(doc["daily_units"]["temperature_2m_max"]|"","°C"))return false;
  WeatherCache next;
  for(size_t i=0;i<n;++i){
    uint32_t day=weatherDate(dates[i].as<const char*>());
    if(day!=today+i)return false;
    if(low[i].isNull() || high[i].isNull() || codes[i].isNull())continue;
    if(!low[i].is<double>() || !high[i].is<double>() || !codes[i].is<int>())return false;
    double lo=low[i].as<double>(),hi=high[i].as<double>();int code=codes[i].as<int>();
    if(!isfinite(lo) || !isfinite(hi) || lo< -100 || hi>100 || lo>hi || !validWeatherCode(code))return false;
    auto& w=next.days[next.count++];w.day=day;w.low=(int16_t)lround(lo*10);w.high=(int16_t)lround(hi*10);w.code=(uint8_t)code;
  }
  forecast=next;return true;
}
