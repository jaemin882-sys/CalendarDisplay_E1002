bool parseIcsText(const String& rawInput,uint8_t feedIndex){
  String raw=rawInput,logical;
  CalendarEvent current;
  bool inEvent=false,gotStart=false,gotEnd=false,startAllDay=false;

  auto processLine=[&](const String& line){
    if(line=="BEGIN:VEVENT"){
      inEvent=true;
      current=CalendarEvent();
      current.calendarIndex=feedIndex;
      current.color=feeds[feedIndex].color;
      gotStart=gotEnd=false;
      startAllDay=false;
      return;
    }
    if(line=="END:VEVENT"){
      if(inEvent && gotStart && eventCount<MAX_EVENTS){
        if(!gotEnd) current.end=current.start+(current.allDay?86400:3600);
        events[eventCount++]=current;
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
  http.setTimeout(20000);
  if(!http.begin(client,url)) return false;
  int code=http.GET();
  if(code!=HTTP_CODE_OK){
    Serial.printf("Calendar %u HTTP error: %d\n",index+1,code);
    http.end();
    return false;
  }
  String body=http.getString();
  http.end();
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
