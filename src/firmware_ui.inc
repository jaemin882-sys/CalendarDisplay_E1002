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

    const uint8_t* font=u8g2_font_gulim14_t_korean1;
    int w=glyphWidthFor(buf,font);
    if(w<=0){ font=u8g2_font_gulim14_t_korean2; w=glyphWidthFor(buf,font); }
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

    int w=glyphWidthFor(buf,u8g2_font_gulim14_t_korean1);
    if(w<=0) w=glyphWidthFor(buf,u8g2_font_gulim14_t_korean2);
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

int daysInMonth(int year,int month1){
  static const int d[]={31,28,31,30,31,30,31,31,30,31,30,31};
  if(month1!=2) return d[month1-1];
  bool leap=(year%400==0)||(year%4==0 && year%100!=0);
  return leap?29:28;
}

int weekdayOfFirst(int year,int month1){
  struct tm t={};
  t.tm_year=year-1900;
  t.tm_mon=month1-1;
  t.tm_mday=1;
  t.tm_hour=12;
  t.tm_isdst=-1;
  mktime(&t);
  return t.tm_wday;
}

bool eventOccursOnDay(const CalendarEvent& e,int year,int month1,int day){
  struct tm ds={};
  ds.tm_year=year-1900;
  ds.tm_mon=month1-1;
  ds.tm_mday=day;
  ds.tm_isdst=-1;
  time_t dayStart=mktime(&ds);
  ds.tm_mday+=1;
  time_t nextDay=mktime(&ds);

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

uint32_t computeRenderHash(int year,int month1){
  uint32_t h=2166136261u;
  auto feedBytes=[&](const String& s){
    for(size_t i=0;i<s.length();++i){ h^=(uint8_t)s[i]; h*=16777619u; }
  };
  feedBytes(String(year));
  feedBytes(String(month1));
  feedBytes(CUSTOM_MEMO_TEXT);

  for(int i=0;i<eventCount;++i){
    feedBytes(String((long long)events[i].start));
    feedBytes(String((long long)events[i].end));
    feedBytes(events[i].summary);
    h^=events[i].calendarIndex;
    h*=16777619u;
  }
  return h;
}

void drawCalendarPage(int year,int month1,int todayDay,bool syncOk){
  const int LEFT=16,RIGHT=784;
  const int GRID_TOP=86,HEADER_H=24,GRID_BOTTOM=470;
  const int CELL_W=(RIGHT-LEFT)/7;
  const int ROW_H=(GRID_BOTTOM-(GRID_TOP+HEADER_H))/6;

  display.setFullWindow();
  display.firstPage();
  do{
    display.fillScreen(C_WHITE);

    char title[24];
    snprintf(title,sizeof(title),"%04d.%02d",year,month1);
    u8g2.setFont(u8g2_font_helvB18_tf);
    u8g2.setForegroundColor(C_BLACK);
    u8g2.setCursor(18,36);
    u8g2.print(title);

    drawUtf8Korean(180,34,String(CUSTOM_MEMO_TEXT),C_BLACK,500);
    display.fillCircle(770,24,5,syncOk?C_GREEN:C_RED);

    const char* wd[]={"일","월","화","수","목","금","토"};
    for(int c=0;c<7;++c){
      int x=LEFT+c*CELL_W;
      uint16_t col=c==0?C_RED:(c==6?C_BLUE:C_BLACK);
      drawUtf8Korean(x+CELL_W/2-7,GRID_TOP+17,String(wd[c]),col,20);
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

    int firstWd=weekdayOfFirst(year,month1);
    int dim=daysInMonth(year,month1);

    for(int day=1;day<=dim;++day){
      int slot=firstWd+day-1;
      int row=slot/7;
      int col=slot%7;
      int x=LEFT+col*CELL_W;
      int y=y0+row*ROW_H;

      if(day==todayDay){
        display.drawRect(x+2,y+2,CELL_W-4,ROW_H-4,C_RED);
        display.drawRect(x+3,y+3,CELL_W-6,ROW_H-6,C_RED);
      }

      uint16_t dayColor=col==0?C_RED:(col==6?C_BLUE:C_BLACK);
      u8g2.setFont(u8g2_font_helvB10_tf);
      u8g2.setForegroundColor(dayColor);
      u8g2.setCursor(x+5,y+15);
      u8g2.print(day);

      int line=0,hidden=0;
      for(int i=0;i<eventCount;++i){
        if(!eventOccursOnDay(events[i],year,month1,day)) continue;
        if(line>=3){ hidden++; continue; }

        int ey=y+29+line*15;
        display.fillRect(x+5,ey-8,5,5,events[i].color);
        String label=fitText(formatEventLabel(events[i]),CELL_W-20);
        drawUtf8Korean(x+14,ey,label,C_BLACK,CELL_W-19);
        line++;
      }

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
