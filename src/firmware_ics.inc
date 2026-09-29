// The same parser is compiled by tests/host_parser.cpp. Bounded input, bounded
// exceptions and atomic per-feed rollback prevent plausible but incomplete UI.
String rrulePart(const String& rule,const char* key){
  String needle=String(key)+"=";
  int start=0;
  while(start<(int)rule.length()){
    int end=rule.indexOf(';',start); if(end<0) end=rule.length();
    String part=rule.substring(start,end);
    if(part.startsWith(needle)) return part.substring(needle.length());
    start=end+1;
  }
  return "";
}
int weekdayCode(const String& code){
  const char* days[]={"SU","MO","TU","WE","TH","FR","SA"};
  for(int i=0;i<7;++i) if(code==days[i]) return i;
  return -1;
}
struct IcsRecord {
  CalendarEvent event;
  String uid,rule;
  std::vector<time_t> exclusions;
  time_t recurrence=0;
  bool hasStart=false,hasEnd=false,cancelled=false,utc=false;
};
struct IcsOverride { String uid; time_t recurrence; };
static constexpr size_t MAX_OVERRIDES=128;
static constexpr size_t MAX_ICS_LINE=4096;
static bool parseOk=true;
static std::vector<IcsOverride> overrides;
static time_t parseWindowStart=0,parseWindowEnd=0;

void appendOccurrence(const CalendarEvent& base,time_t start,time_t duration){
  if(start>=parseWindowEnd || start+duration<=parseWindowStart) return;
  if(eventCount>=MAX_EVENTS){parseOk=false;return;}
  CalendarEvent e=base; e.start=start; e.end=start+duration;
  events[eventCount++]=e;
}
int daysInMonth(int year,int month){
  static const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  return days[month]+(month==1 && year%4==0 && (year%100!=0 || year%400==0));
}
bool numericListMatches(const String& list,int value,int maxValue){
  if(list.isEmpty()) return true;
  for(int pos=0;pos<(int)list.length();){
    int end=list.indexOf(',',pos); if(end<0) end=list.length();
    int n=list.substring(pos,end).toInt();
    if(n<0) n=maxValue+n+1;
    if(n==value) return true;
    pos=end+1;
  }
  return false;
}
bool byDayMatches(const String& list,const struct tm& day){
  if(list.isEmpty()) return true;
  int dim=daysInMonth(day.tm_year+1900,day.tm_mon);
  for(int pos=0;pos<(int)list.length();){
    int end=list.indexOf(',',pos); if(end<0) end=list.length();
    String token=list.substring(pos,end);
    int len=token.length();
    int wd=weekdayCode(token.substring(len>=2?len-2:0));
    int ordinal=len>2?token.substring(0,len-2).toInt():0;
    if(wd==day.tm_wday && (ordinal==0 ||
       (ordinal>0 && (day.tm_mday-1)/7+1==ordinal) ||
       (ordinal<0 && -((dim-day.tm_mday)/7+1)==ordinal))) return true;
    pos=end+1;
  }
  return false;
}
bool suppressed(const IcsRecord& record,time_t start){
  for(time_t ex:record.exclusions) if(ex==start) return true;
  for(const auto& o:overrides) if(o.uid==record.uid && o.recurrence==start) return true;
  return false;
}
bool supportedRule(const String& rule){
  const char* keys[]={"FREQ","INTERVAL","COUNT","UNTIL","BYDAY","BYMONTHDAY","BYMONTH","WKST"};
  for(int pos=0;pos<(int)rule.length();){
    int end=rule.indexOf(';',pos); if(end<0) end=rule.length();
    String part=rule.substring(pos,end); int eq=part.indexOf('=');
    bool found=false;
    for(const char* key:keys) if(part.substring(0,eq)==key) found=true;
    if(eq<1 || !found) return false;
    pos=end+1;
  }
  String freq=rrulePart(rule,"FREQ"),byday=rrulePart(rule,"BYDAY");
  if(freq!="DAILY" && freq!="WEEKLY" && freq!="MONTHLY" && freq!="YEARLY") return false;
  // Ordinal yearly BYDAY without BYMONTH is year-relative, deliberately unsupported.
  for(int pos=0;pos<(int)byday.length();){
    int end=byday.indexOf(',',pos); if(end<0) end=byday.length();
    String token=byday.substring(pos,end); int len=token.length();
    if(len<2 || weekdayCode(token.substring(len-2))<0) return false;
    if(len>2){
      int ord=token.substring(0,len-2).toInt();
      if(!ord || abs(ord)>5 || freq=="DAILY" || freq=="WEEKLY" ||
         (freq=="YEARLY" && rrulePart(rule,"BYMONTH").isEmpty())) return false;
    }
    pos=end+1;
  }
  String wkst=rrulePart(rule,"WKST");
  return wkst.isEmpty() || weekdayCode(wkst)>=0;
}
void emitRecurringEvent(const IcsRecord& record){
  const CalendarEvent& base=record.event;
  time_t duration=base.end-base.start;
  if(duration<=0) duration=1;
  if(record.rule.isEmpty()){
    if(!suppressed(record,base.start)) appendOccurrence(base,base.start,duration);
    return;
  }
  if(!supportedRule(record.rule)){parseOk=false;return;}
  String freq=rrulePart(record.rule,"FREQ");
  int interval=rrulePart(record.rule,"INTERVAL").toInt(); if(interval<1) interval=1;
  int count=rrulePart(record.rule,"COUNT").toInt();
  String untilValue=rrulePart(record.rule,"UNTIL"); bool allDay=false;
  time_t until=untilValue.isEmpty()?0:parseIcsDateTime(untilValue,&allDay);
  if(!untilValue.isEmpty() && !until){parseOk=false;return;}
  if(until && allDay) until+=86399;
  String byday=rrulePart(record.rule,"BYDAY"),bymonth=rrulePart(record.rule,"BYMONTH"),
    bymonthday=rrulePart(record.rule,"BYMONTHDAY"),wkst=rrulePart(record.rule,"WKST");
  int weekStart=wkst.isEmpty()?1:weekdayCode(wkst);
  struct tm bt={};
  if(record.utc) gmtime_r(&base.start,&bt); else localtime_r(&base.start,&bt);
  int generated=0;
  // KST is fixed UTC+9. UTC recurrence is also fixed, so local daily iteration
  // preserves its time. Arbitrary DST zones are rejected at parse time.
  int initial=0;
  if(count==0 && parseWindowStart>base.start+duration)
    initial=std::max(0,(int)((parseWindowStart-base.start-duration)/86400)-1);
  for(int offset=initial;offset<80000;++offset){
    time_t occurrence=base.start+(time_t)offset*86400;
    if(occurrence>=parseWindowEnd || (until && occurrence>until)) return;
    struct tm cand={};
    if(record.utc) gmtime_r(&occurrence,&cand); else localtime_r(&occurrence,&cand);
    int monthDiff=(cand.tm_year-bt.tm_year)*12+cand.tm_mon-bt.tm_mon;
    bool match=false;
    if(freq=="DAILY") match=offset%interval==0;
    if(freq=="WEEKLY"){
      int week=(offset+(bt.tm_wday-weekStart+7)%7)/7;
      match=week%interval==0 && (byday.length() || cand.tm_wday==bt.tm_wday);
    }
    if(freq=="MONTHLY") match=monthDiff%interval==0 &&
      (byday.length() || bymonthday.length() || cand.tm_mday==bt.tm_mday);
    if(freq=="YEARLY") match=(cand.tm_year-bt.tm_year)%interval==0 &&
      (bymonth.length() || byday.length() || bymonthday.length() || cand.tm_mon==bt.tm_mon) &&
      (byday.length() || bymonthday.length() || cand.tm_mday==bt.tm_mday);
    match=match && numericListMatches(bymonth,cand.tm_mon+1,12) &&
      numericListMatches(bymonthday,cand.tm_mday,daysInMonth(cand.tm_year+1900,cand.tm_mon)) &&
      byDayMatches(byday,cand);
    // DTSTART is always the first recurrence, including when rule filters differ.
    if(offset==0) match=true;
    if(!match) continue;
    ++generated;
    if(count>0 && generated>count) return;
    if(!suppressed(record,occurrence)) appendOccurrence(base,occurrence,duration);
    if(!parseOk) return;
  }
  parseOk=false; // Never silently truncate an exceptionally old recurrence.
}

bool parseIcsText(const char* raw,uint8_t feedIndex){
  int before=eventCount;
  parseOk=true;overrides.clear();
  displayWindow(parseWindowStart,parseWindowEnd);
  // Pass 0 records detached exceptions even if they appear after their master.
  // Pass 1 expands masters, then emits moved instances independently.
  for(int pass=0;pass<2 && parseOk;++pass){
    IcsRecord current; bool inEvent=false,calendar=false,closed=false;
    int nested=0;
    auto process=[&](const String& line){
      if(line=="BEGIN:VCALENDAR"){if(calendar) parseOk=false;calendar=true;return;}
      if(line=="END:VCALENDAR"){closed=true;return;}
      if(line=="BEGIN:VEVENT"){
        if(inEvent || !calendar || closed){parseOk=false;return;}
        current=IcsRecord();current.event.calendarIndex=feedIndex;
        current.event.color=feeds[feedIndex].color;
        current.event.summary="(제목없음)";
        inEvent=true;nested=0;return;
      }
      if(!inEvent) return;
      if(line=="END:VEVENT"){
        if(nested){parseOk=false;return;}
        inEvent=false;
        if(current.uid.isEmpty()){parseOk=false;return;}
        if(current.event.summary.isEmpty()) current.event.summary="(제목없음)";
        if(pass==0 && current.recurrence){
          if(overrides.size()>=MAX_OVERRIDES){parseOk=false;return;}
          for(const auto& o:overrides) if(o.uid==current.uid && o.recurrence==current.recurrence){parseOk=false;return;}
          overrides.push_back({current.uid,current.recurrence});
        }
        if(pass==1 && !current.cancelled){
          if(!current.hasStart){parseOk=false;return;}
          if(!current.hasEnd) current.event.end=current.event.start+(current.event.allDay?86400:1);
          if(current.event.end<current.event.start){parseOk=false;return;}
          if(current.recurrence) appendOccurrence(current.event,current.event.start,std::max((time_t)1,current.event.end-current.event.start));
          else emitRecurringEvent(current);
        }
        return;
      }
      if(line.startsWith("BEGIN:")){++nested;return;}
      if(line.startsWith("END:")){--nested;return;}
      if(nested) return; // VALARM DTSTART/SUMMARY must not replace event data.
      if(lineStartsWithProperty(line,"DTSTART")){
        current.event.start=parsePropertyDate(line,&current.event.allDay,parseOk);current.hasStart=true;
        current.utc=propertyValue(line).endsWith("Z") || line.indexOf("TZID=UTC")>=0 || line.indexOf("TZID=Etc/UTC")>=0 || line.indexOf("TZID=GMT")>=0;
      }else if(lineStartsWithProperty(line,"DTEND")){
        bool ad=false;current.event.end=parsePropertyDate(line,&ad,parseOk);current.hasEnd=true;
      }else if(lineStartsWithProperty(line,"SUMMARY")) current.event.summary=icsUnescape(propertyValue(line));
      else if(lineStartsWithProperty(line,"UID")) current.uid=propertyValue(line);
      else if(lineStartsWithProperty(line,"STATUS")) current.cancelled=propertyValue(line)=="CANCELLED";
      else if(lineStartsWithProperty(line,"RRULE")) current.rule=propertyValue(line);
      else if(lineStartsWithProperty(line,"RECURRENCE-ID")){
        if(line.indexOf("RANGE=")>=0){parseOk=false;return;}
        bool ad=false;current.recurrence=parsePropertyDate(line,&ad,parseOk);
      }else if(lineStartsWithProperty(line,"EXDATE")){
        String values=propertyValue(line), prop=line.substring(0,line.indexOf(':')+1);
        for(int pos=0;pos<(int)values.length();){
          int end=values.indexOf(',',pos);if(end<0) end=values.length();
          bool ad=false;
          if(current.exclusions.size()>=256){parseOk=false;return;}
          current.exclusions.push_back(parsePropertyDate(prop+values.substring(pos,end),&ad,parseOk));
          pos=end+1;
        }
      }else if(lineStartsWithProperty(line,"RDATE") || lineStartsWithProperty(line,"DURATION") ||
               lineStartsWithProperty(line,"EXRULE")){parseOk=false;}
    };
    String logical;logical.reserve(256);
    const char* pos=raw;
    if((uint8_t)pos[0]==0xef && (uint8_t)pos[1]==0xbb && (uint8_t)pos[2]==0xbf) pos+=3;
    while(*pos && parseOk){
      const char* end=strchr(pos,'\n');if(!end) end=pos+strlen(pos);
      size_t len=end-pos;if(len && pos[len-1]=='\r') --len;
      bool folded=len && (*pos==' ' || *pos=='\t');
      if(!folded){if(logical.length()) process(logical);logical="";}
      if(folded){++pos;--len;}
      if(logical.length()+len>MAX_ICS_LINE){parseOk=false;break;}
      for(size_t i=0;i<len;++i) logical+=pos[i];
      pos=*end?end+1:end;
    }
    if(logical.length() && parseOk) process(logical);
    if(!calendar || !closed || inEvent) parseOk=false;
  }
  overrides.clear();
  if(!parseOk){
    while(eventCount>before) events[--eventCount]=CalendarEvent();
    Serial.printf("Feed %u rejected: malformed/unsupported ICS or capacity exceeded.\n",feedIndex+1);
  }
  return parseOk;
}

#ifndef CALENDAR_HOST_TEST
#include "firmware_network.inc"
#endif
void sortEvents(){
  std::stable_sort(events,events+eventCount,[](const CalendarEvent& a,const CalendarEvent& b){
    if(a.start!=b.start) return a.start<b.start;
    if(a.allDay!=b.allDay) return a.allDay;
    if(a.calendarIndex!=b.calendarIndex) return a.calendarIndex<b.calendarIndex;
    return a.summary<b.summary;
  });
}
