// KST calendar device: floating dates/Asia/Seoul and UTC only. Unknown TZID
// fails the feed instead of silently moving appointments by several hours.
String icsUnescape(const String& s){
  String out; out.reserve(s.length());
  for(size_t i=0;i<s.length();++i){
    char c=s[i];
    if(c=='\\' && i+1<s.length()){
      c=s[++i];
      if(c=='n'||c=='N') c=' ';
    }
    out+=c;
  }
  return out;
}

time_t parseIcsDateTime(const String& value,bool* isAllDay){
  String v=value; v.trim();
  *isAllDay=v.length()==8;
  bool utc=v.endsWith("Z");
  if(!*isAllDay && v.length()!=(utc?16u:15u)) return 0;
  if(!*isAllDay && v[8]!='T') return 0;
  for(size_t i=0;i<v.length();++i){
    if(i==8 || (utc && i==15)) continue;
    if(v[i]<'0'||v[i]>'9') return 0;
  }
  struct tm t={};
  t.tm_year=v.substring(0,4).toInt()-1900;
  t.tm_mon=v.substring(4,6).toInt()-1;
  t.tm_mday=v.substring(6,8).toInt();
  if(!*isAllDay){
    t.tm_hour=v.substring(9,11).toInt();
    t.tm_min=v.substring(11,13).toInt();
    t.tm_sec=v.substring(13,15).toInt();
  }
  if(t.tm_year<70 || t.tm_year>199 || t.tm_mon<0 || t.tm_mon>11 ||
     t.tm_mday<1 || t.tm_mday>31 || t.tm_hour>23 || t.tm_min>59 || t.tm_sec>59) return 0;
  struct tm expected=t;
  t.tm_isdst=-1;
  if(utc){ setenv("TZ","UTC0",1); tzset(); }
  time_t result=mktime(&t);
  if(utc){ setenv("TZ",TZ_INFO,1); tzset(); }
  if(t.tm_year!=expected.tm_year || t.tm_mon!=expected.tm_mon || t.tm_mday!=expected.tm_mday) return 0;
  return result;
}

time_t parsePropertyDate(const String& line,bool* allDay,bool& ok){
  int colon=line.indexOf(':');
  if(colon<0){ok=false;return 0;}
  String prop=line.substring(0,colon), value=line.substring(colon+1);
  int tz=prop.indexOf("TZID=");
  if(tz>=0){
    String zone=prop.substring(tz+5); int semi=zone.indexOf(';');
    if(semi>=0) zone=zone.substring(0,semi);
    zone.replace("\"","");
    if(zone=="UTC" || zone=="Etc/UTC" || zone=="GMT"){
      if(value.length()==15) value+="Z";
    }else if(zone!="Asia/Seoul" && zone!="Asia/Tokyo"){
      ok=false; return 0;
    }
  }
  time_t result=parseIcsDateTime(value,allDay);
  if(!result) ok=false;
  if(prop.indexOf("VALUE=DATE")>=0 && prop.indexOf("VALUE=DATE-TIME")<0 && !*allDay) ok=false;
  return result;
}
