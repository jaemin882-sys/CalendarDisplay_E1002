int utf8CharLen(uint8_t c){
  if((c&0x80)==0) return 1;
  if((c&0xE0)==0xC0) return 2;
  if((c&0xF0)==0xE0) return 3;
  if((c&0xF8)==0xF0) return 4;
  return 1;
}

int glyphWidthFor(const char* utf8,const uint8_t* font){
  u8g2.setFont(font);
  return u8g2.getUTF8Width(utf8);
}

int drawUtf8Korean(int x,int baselineY,const String& text,uint16_t color,int maxWidth=-1){
  u8g2.setForegroundColor(color);
  int cursor=x;
  int i=0;
  while(i<text.length()){
    int len=utf8CharLen((uint8_t)text[i]);
    String ch=text.substring(i,min(i+len,(int)text.length()));
    char buf[8]={0};
    ch.toCharArray(buf,sizeof(buf));

    const uint8_t* font=u8g2_font_unifont_t_korean1;
    int w=glyphWidthFor(buf,font);
    if(w<=0){ font=u8g2_font_unifont_t_korean2; w=glyphWidthFor(buf,font); }
    if(w<=0){ font=u8g2_font_helvR10_tf; w=glyphWidthFor(buf,font); }
    if(w<=0) w=8;
    if(maxWidth>0 && cursor+w>x+maxWidth) break;

    u8g2.setFont(font);
    u8g2.setCursor(cursor,baselineY);
    u8g2.print(buf);
    cursor+=w;
    i+=len;
  }
  return cursor-x;
}

String fitText(const String& input,int maxPixels){
  String out;
  int used=0;
  int i=0;
  while(i<input.length()){
    int len=utf8CharLen((uint8_t)input[i]);
    String ch=input.substring(i,min(i+len,(int)input.length()));
    char buf[8]={0};
    ch.toCharArray(buf,sizeof(buf));

    int w=glyphWidthFor(buf,u8g2_font_unifont_t_korean1);
    if(w<=0) w=glyphWidthFor(buf,u8g2_font_unifont_t_korean2);
    if(w<=0) w=glyphWidthFor(buf,u8g2_font_helvR10_tf);
    if(w<=0) w=8;

    if(used+w>maxPixels-12){
      out+="…";
      break;
    }
    out+=ch;
    used+=w;
    i+=len;
  }
  return out;
}

int takeTextLine(const String& input,int startByte,int maxPixels,String& out){
  out="";
  int used=0;
  int i=startByte;
  while(i<input.length()){
    int len=utf8CharLen((uint8_t)input[i]);
    String ch=input.substring(i,min(i+len,(int)input.length()));
    char buf[8]={0};
    ch.toCharArray(buf,sizeof(buf));

    int w=glyphWidthFor(buf,u8g2_font_unifont_t_korean1);
    if(w<=0) w=glyphWidthFor(buf,u8g2_font_unifont_t_korean2);
    if(w<=0) w=glyphWidthFor(buf,u8g2_font_helvR10_tf);
    if(w<=0) w=8;

    if(used+w>maxPixels && out.length()>0) break;
    out+=ch;
    used+=w;
    i+=len;
  }
  return i;
}

bool eventOccursOnDay(const CalendarEvent& e,time_t dayStart){
  time_t nextDay=dayStart+86400;
  time_t end=e.end>e.start?e.end:e.start+1;
  return e.start<nextDay && end>dayStart;
}

String formatEventLabel(const CalendarEvent& e){
  if(e.allDay) return e.summary;
  struct tm lt={};
  localtime_r(&e.start,&lt);
  char timebuf[8];
  snprintf(timebuf,sizeof(timebuf),"%02d:%02d",lt.tm_hour,lt.tm_min);
  return String(timebuf)+" "+e.summary;
}

String compactEventLabel(const CalendarEvent& e){
  String s=formatEventLabel(e);
  s.replace(" ","");
  s.replace("\t","");
  return s;
}

String holidayNameForDay(time_t dayStart){
  String name="";
  for(int i=0;i<eventCount;++i){
    if(events[i].calendarIndex!=HOLIDAY_FEED_INDEX) continue;
    if(!eventOccursOnDay(events[i],dayStart)) continue;
    if(name.length()) name+="·";
    name+=events[i].summary;
  }
  return name;
}

uint32_t computeRenderHash(time_t windowStart){
  uint32_t h=2166136261u;
  auto feedBytes=[&](const String& s){
    for(size_t i=0;i<s.length();++i){ h^=(uint8_t)s[i]; h*=16777619u; }
  };
  char windowBuf[32];
  snprintf(windowBuf,sizeof(windowBuf),"%lld",(long long)windowStart);
  feedBytes(String(windowBuf));
  feedBytes(CUSTOM_MEMO_TEXT);

  for(int i=0;i<eventCount;++i){
    char timeBuf[32];
    snprintf(timeBuf,sizeof(timeBuf),"%lld",(long long)events[i].start);
    feedBytes(String(timeBuf));
    snprintf(timeBuf,sizeof(timeBuf),"%lld",(long long)events[i].end);
    feedBytes(String(timeBuf));
    feedBytes(events[i].summary);
    h^=events[i].calendarIndex;
    h*=16777619u;
  }
  return h;
}

void drawCalendarPage(time_t todayMidnight,bool syncOk){
  const int LEFT=16,RIGHT=784;
  const int GRID_TOP=48,HEADER_H=20,GRID_BOTTOM=476;
  const int CELL_W=(RIGHT-LEFT)/7;
  const int ROW_H=(GRID_BOTTOM-(GRID_TOP+HEADER_H))/6;

  time_t windowStart=0,windowEnd=0;
  displayWindow(windowStart,windowEnd);

  struct tm firstTm={},lastTm={};
  localtime_r(&windowStart,&firstTm);
  time_t lastDay=windowEnd-86400;
  localtime_r(&lastDay,&lastTm);

  display.setFullWindow();
  display.firstPage();
  do{
    display.fillScreen(C_WHITE);

    char title[32];
    snprintf(
      title,sizeof(title),
      "%02d.%02d - %02d.%02d",
      firstTm.tm_mon+1,firstTm.tm_mday,
      lastTm.tm_mon+1,lastTm.tm_mday
    );
    u8g2.setFont(u8g2_font_helvB18_tf);
    u8g2.setForegroundColor(C_BLACK);
    u8g2.setCursor(18,30);
    u8g2.print(title);

    drawUtf8Korean(240,29,String(CUSTOM_MEMO_TEXT),C_BLACK,440);
    display.fillCircle(770,20,5,syncOk?C_GREEN:C_RED);

    const char* wd[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
    for(int c=0;c<7;++c){
      int x=LEFT+c*CELL_W;
      uint16_t col=c==0?C_RED:(c==6?C_BLUE:C_BLACK);
      u8g2.setFont(u8g2_font_helvB10_tf);
      u8g2.setForegroundColor(col);
      int ww=u8g2.getUTF8Width(wd[c]);
      u8g2.setCursor(x+(CELL_W-ww)/2,GRID_TOP+15);
      u8g2.print(wd[c]);
    }

    int y0=GRID_TOP+HEADER_H;
    for(int c=0;c<=7;++c){
      int x=LEFT+c*CELL_W;
      display.drawLine(x,y0,x,GRID_BOTTOM,C_BLACK);
    }
    for(int r=0;r<=6;++r){
      int y=y0+r*ROW_H;
      display.drawLine(LEFT,y,RIGHT,y,C_BLACK);
    }

    for(int slot=0;slot<42;++slot){
      int row=slot/7;
      int col=slot%7;
      int x=LEFT+col*CELL_W;
      int y=y0+row*ROW_H;
      time_t cellStart=windowStart+(time_t)slot*86400;

      struct tm cellTm={};
      localtime_r(&cellStart,&cellTm);

      if(cellStart==todayMidnight){
        display.drawRect(x+2,y+2,CELL_W-4,ROW_H-4,C_RED);
        display.drawRect(x+3,y+3,CELL_W-6,ROW_H-6,C_RED);
      }

      uint16_t dayColor=col==0?C_RED:(col==6?C_BLUE:C_BLACK);
      String holiday=holidayNameForDay(cellStart);
      if(holiday.length()) dayColor=C_RED;

      u8g2.setFont(u8g2_font_helvB10_tf);
      u8g2.setForegroundColor(dayColor);
      u8g2.setCursor(x+5,y+15);

      bool showMonth=cellTm.tm_mday==1 || slot==0;
      if(showMonth){
        u8g2.print(cellTm.tm_mon+1);
        u8g2.print("/");
      }
      u8g2.print(cellTm.tm_mday);

      if(holiday.length()){
        int hx=x+(showMonth?38:24);
        drawUtf8Korean(hx,y+15,holiday,C_RED,CELL_W-(hx-x)-4);
      }

      int dayEventIndices[12];
      int dayEventCount=0;
      for(int i=0;i<eventCount && dayEventCount<12;++i){
        if(events[i].calendarIndex==HOLIDAY_FEED_INDEX) continue;
        if(eventOccursOnDay(events[i],cellStart)) dayEventIndices[dayEventCount++]=i;
      }

      int perEventLines=1;
      int maxVisibleEvents=min(dayEventCount,3);
      if(dayEventCount==1) perEventLines=3;
      else if(dayEventCount==2) perEventLines=2;
      else perEventLines=1;

      int line=0,shownEvents=0;
      const int EVENT_STEP=14;
      for(int n=0;n<maxVisibleEvents;++n){
        CalendarEvent& ev=events[dayEventIndices[n]];
        String label=compactEventLabel(ev);
        int pos=0;

        for(int part=0;part<perEventLines && pos<label.length();++part){
          String piece;
          int next=takeTextLine(label,pos,CELL_W-10,piece);
          if(next<label.length() && part==perEventLines-1){
            piece=fitText(label.substring(pos),CELL_W-10);
          }

          int ey=y+29+line*EVENT_STEP;
          drawUtf8Korean(x+5,ey,piece,C_BLACK,CELL_W-9);
          line++;
          if(next<=pos) break;
          pos=next;
        }
        shownEvents++;
      }

      int hidden=dayEventCount-shownEvents;
      if(hidden>0){
        u8g2.setFont(u8g2_font_helvR08_tf);
        u8g2.setForegroundColor(C_BLACK);
        u8g2.setCursor(x+14,y+ROW_H-5);
        u8g2.print("+");
        u8g2.print(hidden);
      }
    }
  }while(display.nextPage());
}

void drawStatusPage(const String& title,const String& line1,const String& line2,uint16_t accent){
  display.setFullWindow();
  display.firstPage();
  do{
    display.fillScreen(C_WHITE);
    display.fillRect(0,0,14,480,accent);

    u8g2.setForegroundColor(C_BLACK);
    u8g2.setFont(u8g2_font_helvB18_tf);
    u8g2.setCursor(38,90);
    u8g2.print(title);

    drawUtf8Korean(38,150,line1,C_BLACK,710);
    drawUtf8Korean(38,190,line2,C_BLACK,710);

    drawUtf8Korean(
      38,430,
      String("설정 후 USB-C로 다시 업로드하면 됩니다."),
      C_BLACK,710
    );
  }while(display.nextPage());
}
