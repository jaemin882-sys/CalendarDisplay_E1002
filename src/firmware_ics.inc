String rrulePart(const String& rule,const char* key){
  String needle=String(key)+"=";
  int start=0;
  while(start<rule.length()){
    int end=rule.indexOf(';',start);
    if(end<0) end=rule.length();
    String part=rule.substring(start,end);
    if(part.startsWith(needle)) return part.substring(needle.length());
    start=end+1;
  }
  return "";
}

int weekdayCode(const String& code){
  if(code=="SU") return 0;
  if(code=="MO") return 1;
  if(code=="TU") return 2;
  if(code=="WE") return 3;
  if(code=="TH") return 4;
  if(code=="FR") return 5;
  if(code=="SA") return 6;
  return -1;
}

bool byDayMatches(const String& byday,int wday){
  if(byday.isEmpty()) return true;
  int start=0;
  while(start<byday.length()){
    int end=byday.indexOf(',',start);
    if(end<0) end=byday.length();
    String token=byday.substring(start,end);
    token.trim();
    if(token.length()>=2){
      String last2=token.substring(token.length()-2);
      if(weekdayCode(last2)==wday) return true;
    }
    start=end+1;
  }
  return false;
}

bool isExcludedOccurrence(time_t occurrence,const String& exdates){
  if(exdates.isEmpty()) return false;
  int start=0;
  while(start<exdates.length()){
    int end=exdates.indexOf(',',start);
    if(end<0) end=exdates.length();
    String token=exdates.substring(start,end);
    token.trim();
    bool allDay=false;
    time_t ex=parseIcsDateTime(token,&allDay);
    if(ex>0){
      if(allDay){
        struct tm a={},b={};
        localtime_r(&occurrence,&a);
        localtime_r(&ex,&b);
        if(a.tm_year==b.tm_year && a.tm_yday==b.tm_yday) return true;
      }else if(llabs((long long)occurrence-(long long)ex)<60){
        return true;
      }
    }
    start=end+1;
  }
  return false;
}

void appendOccurrence(const CalendarEvent& base,time_t start,time_t duration){
  if(eventCount>=MAX_EVENTS) return;
  CalendarEvent e=base;
  e.start=start;
  e.end=start+duration;
  events[eventCount++]=e;
}

void emitRecurringEvent(const CalendarEvent& base,const String& rrule,const String& exdates){
  if(rrule.isEmpty()){
    if(!isExcludedOccurrence(base.start,exdates) && eventCount<MAX_EVENTS)
      events[eventCount++]=base;
    return;
  }

  String freq=rrulePart(rrule,"FREQ");
  int interval=(int)rrulePart(rrule,"INTERVAL").toInt();
  if(interval<1) interval=1;
  int maxCount=rrulePart(rrule,"COUNT").toInt();
  String untilValue=rrulePart(rrule,"UNTIL");
  String byday=rrulePart(rrule,"BYDAY");

  bool untilAllDay=false;
  time_t untilTime=untilValue.isEmpty()?0:parseIcsDateTime(untilValue,&untilAllDay);
  time_t duration=(base.end>base.start)?base.end-base.start:(base.allDay?86400:3600);

  time_t windowStart=0,windowEnd=0;
  displayWindow(windowStart,windowEnd);

  struct tm baseLocal={};
  localtime_r(&base.start,&baseLocal);

  struct tm baseDay=baseLocal;
  baseDay.tm_hour=0; baseDay.tm_min=0; baseDay.tm_sec=0; baseDay.tm_isdst=-1;
  time_t baseMidnight=mktime(&baseDay);

  int occurrenceCount=0;
  const int MAX_SCAN_DAYS=5500;

  for(int dayOffset=0;dayOffset<MAX_SCAN_DAYS;++dayOffset){
    struct tm candDay=baseDay;
    candDay.tm_mday+=dayOffset;
    time_t candMidnight=mktime(&candDay);
    if(candMidnight>=windowEnd) break;

    struct tm cand={};
    localtime_r(&candMidnight,&cand);
    bool match=false;

    if(freq=="DAILY"){
      match=(dayOffset%interval)==0;
    }else if(freq=="WEEKLY"){
      int weekIndex=dayOffset/7;
      bool weekdayMatch=byday.isEmpty()
        ? cand.tm_wday==baseLocal.tm_wday
        : byDayMatches(byday,cand.tm_wday);
      match=((weekIndex%interval)==0) && weekdayMatch;
    }else if(freq=="MONTHLY"){
      int monthDiff=(cand.tm_year-baseLocal.tm_year)*12+(cand.tm_mon-baseLocal.tm_mon);
      match=monthDiff>=0 && (monthDiff%interval)==0 &&
            cand.tm_mday==baseLocal.tm_mday;
    }else if(freq=="YEARLY"){
      int yearDiff=cand.tm_year-baseLocal.tm_year;
      match=yearDiff>=0 && (yearDiff%interval)==0 &&
            cand.tm_mon==baseLocal.tm_mon &&
            cand.tm_mday==baseLocal.tm_mday;
    }else{
      if(!isExcludedOccurrence(base.start,exdates) && eventCount<MAX_EVENTS)
        events[eventCount++]=base;
      return;
    }

    if(!match) continue;

    struct tm occurrenceTm=cand;
    occurrenceTm.tm_hour=baseLocal.tm_hour;
    occurrenceTm.tm_min=baseLocal.tm_min;
    occurrenceTm.tm_sec=baseLocal.tm_sec;
    occurrenceTm.tm_isdst=-1;
    time_t occurrence=mktime(&occurrenceTm);

    if(occurrence<base.start) continue;
    if(untilTime>0 && occurrence>untilTime) break;

    occurrenceCount++;
    if(maxCount>0 && occurrenceCount>maxCount) break;

    if(occurrence>=windowStart && occurrence<windowEnd &&
       !isExcludedOccurrence(occurrence,exdates)){
      appendOccurrence(base,occurrence,duration);
    }
  }
}

bool parseIcsText(const String& rawInput,uint8_t feedIndex){
  String raw=rawInput,logical;
  CalendarEvent current;
  String rrule;
  String exdates;
  bool inEvent=false,gotStart=false,gotEnd=false,startAllDay=false;

  auto processLine=[&](const String& line){
    if(line=="BEGIN:VEVENT"){
      inEvent=true;
      current=CalendarEvent();
      current.calendarIndex=feedIndex;
      current.color=feeds[feedIndex].color;
      gotStart=gotEnd=false;
      startAllDay=false;
      rrule="";
      exdates="";
      return;
    }

    if(line=="END:VEVENT"){
      if(inEvent && gotStart){
        if(!gotEnd) current.end=current.start+(current.allDay?86400:3600);
        emitRecurringEvent(current,rrule,exdates);
      }
      inEvent=false;
      return;
    }

    if(!inEvent) return;

    if(lineStartsWithProperty(line,"DTSTART")){
      current.start=parseIcsDateTime(propertyValue(line),&startAllDay);
      current.allDay=startAllDay;
      gotStart=current.start>0;
    }else if(lineStartsWithProperty(line,"DTEND")){
      bool dummy=false;
      current.end=parseIcsDateTime(propertyValue(line),&dummy);
      gotEnd=current.end>0;
    }else if(lineStartsWithProperty(line,"SUMMARY")){
      current.summary=icsUnescape(propertyValue(line));
    }else if(lineStartsWithProperty(line,"RRULE")){
      rrule=propertyValue(line);
    }else if(lineStartsWithProperty(line,"EXDATE")){
      if(exdates.length()) exdates+=",";
      exdates+=propertyValue(line);
    }
  };

  int pos=0;
  while(pos<raw.length()){
    int nl=raw.indexOf('\n',pos);
    if(nl<0) nl=raw.length();
    String line=raw.substring(pos,nl);
    if(line.endsWith("\r")) line.remove(line.length()-1);
    pos=nl+1;

    if(line.startsWith(" ")||line.startsWith("\t")){
      logical+=line.substring(1);
      continue;
    }

    if(logical.length()) processLine(logical);
    logical=line;
  }

  if(logical.length()) processLine(logical);
  return true;
}

bool downloadAndParseFeed(uint8_t index){
  String url=normalizeIcsUrl(feeds[index].url);
  if(url.isEmpty()) return true;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(25000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if(!http.begin(client,url)) return false;
  int code=http.GET();

  if(code!=HTTP_CODE_OK){
    Serial.printf("Calendar %u HTTP error: %d\n",index+1,code);
    http.end();
    return false;
  }

  String body=http.getString();
  http.end();

  Serial.printf("Calendar %u downloaded: %u bytes\n",index+1,body.length());
  return parseIcsText(body,index);
}

void sortEvents(){
  for(int i=1;i<eventCount;i++){
    CalendarEvent key=events[i];
    int j=i-1;
    while(j>=0 && events[j].start>key.start){
      events[j+1]=events[j];
      --j;
    }
    events[j+1]=key;
  }
}
