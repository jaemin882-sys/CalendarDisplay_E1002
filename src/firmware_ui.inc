#include "ui_layout.h"
// Upstream setFont resets transparency whenever the font changes.
// Restore it on EVERY switch, including fallback/measurement calls.
void selectFont(const uint8_t* font){
  u8g2.setFont(font);u8g2.setFontMode(1);u8g2.setBackgroundColor(C_WHITE);
}
// Decode one scalar without splitting a multibyte sequence. U8g2 supports BMP;
// missing glyphs and non-BMP emoji become visible '?' instead of invisible text.
int readGlyph(const String& text,int offset,char (&buf)[8],const uint8_t*& font){
  uint8_t c=text[offset];int len=1;uint32_t cp=c;
  if(c>=0xc2 && c<=0xdf){len=2;cp=c&31;}
  else if(c>=0xe0 && c<=0xef){len=3;cp=c&15;}
  else if(c>=0xf0 && c<=0xf4){len=4;cp=c&7;}
  else if(c>=0x80) cp=0xfffd;
  if(offset+len>(int)text.length()){len=1;cp=0xfffd;}
  for(int j=1;j<len;++j){
    uint8_t tail=text[offset+j];
    if((tail&0xc0)!=0x80){len=1;cp=0xfffd;break;}
    cp=(cp<<6)|(tail&63);
  }
  bool invalid=(len==2 && cp<128)||(len==3 && cp<2048)||(len==4 && cp<65536)||
    (cp>=0xd800 && cp<=0xdfff)||cp>0x10ffff;
  if(cp>0xffff || cp==0xfffd || invalid) strcpy(buf,"?");
  else{for(int j=0;j<len;++j)buf[j]=text[offset+j];buf[len]=0;}
  font=u8g2_font_unifont_t_korean2;selectFont(font);
  if(u8g2.getUTF8Width(buf)<=0){font=u8g2_font_helvR10_tf;selectFont(font);}
  if(u8g2.getUTF8Width(buf)<=0) strcpy(buf,"?");
  return len;
}
int glyphWidthFor(const char* utf8,const uint8_t* font){
  selectFont(font);return u8g2.getUTF8Width(utf8);
}

int drawUtf8Korean(int x,int baselineY,const String& text,uint16_t color,int maxWidth=-1){
  u8g2.setForegroundColor(color);
  int cursor=x;
  int i=0;
  while(i<text.length()){
    char buf[8]={0};const uint8_t* font=nullptr;
    int len=readGlyph(text,i,buf,font);
    int w=glyphWidthFor(buf,font);
    if(maxWidth>0 && cursor+w>x+maxWidth) break;

    selectFont(font);
    u8g2.setCursor(cursor,baselineY);
    u8g2.print(buf);
    cursor+=w;
    i+=len;
  }
  return cursor-x;
}

int takeTextLine(const String& input,int startByte,int maxPixels,String& out);

String fitText(const String& input,int maxPixels){
  String complete;
  if(takeTextLine(input,0,maxPixels,complete)==(int)input.length()) return complete;
  String out;
  int used=0;
  int i=0;
  while(i<input.length()){
    char buf[8]={0};const uint8_t* font=nullptr;
    int len=readGlyph(input,i,buf,font);
    String ch(buf);
    int w=glyphWidthFor(buf,font);

    if(used+w>maxPixels-24){
      out+="...";
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
    char buf[8]={0};const uint8_t* font=nullptr;
    int len=readGlyph(input,i,buf,font);
    String ch(buf);
    int w=glyphWidthFor(buf,font);

    if(used+w>maxPixels) break;
    out+=ch;
    used+=w;
    i+=len;
  }
  return i;
}

static constexpr int EVENT_TRACKING=-2;

int eventGlyphAdvance(const char* utf8,const uint8_t* font){
  return max(1,glyphWidthFor(utf8,font)+EVENT_TRACKING);
}

int takeEventTextLine(const String& input,int startByte,int maxPixels,String& out){
  out="";
  int used=0;
  int i=startByte;
  while(i<input.length()){
    char buf[8]={0};const uint8_t* font=nullptr;
    int len=readGlyph(input,i,buf,font);
    int advance=eventGlyphAdvance(buf,font);
    if(used+advance>maxPixels) break;
    out+=String(buf);
    used+=advance;
    i+=len;
  }
  return i;
}

String fitEventText(const String& input,int maxPixels){
  String complete;
  if(takeEventTextLine(input,0,maxPixels,complete)==(int)input.length()) return complete;
  String out;
  int used=0;
  int i=0;
  const int dotsWidth=18;
  while(i<input.length()){
    char buf[8]={0};const uint8_t* font=nullptr;
    int len=readGlyph(input,i,buf,font);
    int advance=eventGlyphAdvance(buf,font);
    if(used+advance>maxPixels-dotsWidth){out+="...";break;}
    out+=String(buf);
    used+=advance;
    i+=len;
  }
  return out;
}

int drawEventText(int x,int baselineY,const String& text,uint16_t color,int maxWidth){
  u8g2.setForegroundColor(color);
  int cursor=x;
  int i=0;
  while(i<text.length()){
    char buf[8]={0};const uint8_t* font=nullptr;
    int len=readGlyph(text,i,buf,font);
    int advance=eventGlyphAdvance(buf,font);
    if(cursor+advance>x+maxWidth) break;
    selectFont(font);
    u8g2.setCursor(cursor,baselineY);
    u8g2.print(buf);
    cursor+=advance;
    i+=len;
  }
  return cursor-x;
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
  s.replace("\n","");
  s.replace("\r","");
  s.replace("\xC2\xA0","");
  s.replace("　","");
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

uint32_t computeRenderHash(time_t windowStart,time_t todayMidnight,bool holidayFresh){
  uint32_t h=2166136261u;
  auto feedBytes=[&](const String& s){
    for(size_t i=0;i<s.length();++i){ h^=(uint8_t)s[i]; h*=16777619u; }
  };
  char windowBuf[32];
  snprintf(windowBuf,sizeof(windowBuf),"%lld",(long long)windowStart);
  feedBytes(String(windowBuf));
  feedBytes(CUSTOM_MEMO_TEXT);
  feedBytes(String((long long)todayMidnight));
  feedBytes("layout-v1.0.0");
  h^=holidayFresh?1:0;

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
  const int LEFT=UI_LEFT,RIGHT=UI_RIGHT;
  const int GRID_TOP=UI_GRID_TOP,HEADER_H=UI_HEADER_H,GRID_BOTTOM=UI_GRID_BOTTOM;
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
    selectFont(u8g2_font_helvB14_tf);
    u8g2.setForegroundColor(C_BLACK);
    u8g2.setCursor(10,23);
    u8g2.print(title);

    int titleWidth=u8g2.getUTF8Width(title);
    drawUtf8Korean(10+titleWidth+18,23,fitText(String(CUSTOM_MEMO_TEXT),740-titleWidth-18),C_BLACK,740-titleWidth-18);
    // Small marker means holiday source stale/unavailable, not personal failure.
    if(!syncOk) display.fillCircle(780,16,3,C_RED);

    const char* wd[]={"SUN","MON","TUE","WED","THU","FRI","SAT"};
    for(int c=0;c<7;++c){
      int x=LEFT+c*CELL_W;
      uint16_t col=c==0?C_RED:(c==6?C_BLUE:C_BLACK);
      selectFont(u8g2_font_helvB10_tf);
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

      selectFont(u8g2_font_helvB10_tf);
      u8g2.setForegroundColor(dayColor);
      bool showMonth=cellTm.tm_mday==1 || slot==0;
      char dayText[32];
      if(showMonth) snprintf(dayText,sizeof(dayText),"%d/%d",cellTm.tm_mon+1,cellTm.tm_mday);
      else snprintf(dayText,sizeof(dayText),"%d",cellTm.tm_mday);
      int dateWidth=u8g2.getUTF8Width(dayText);
      u8g2.setCursor(x+5,y+15);u8g2.print(dayText);
      if(holiday.length()){
        int hx=x+5+dateWidth+4;
        drawUtf8Korean(hx,y+15,fitText(holiday,CELL_W-(hx-x)-5),C_RED,CELL_W-(hx-x)-5);
      }
      int dayEventIndices[3];
      int dayEventCount=0;
      for(int i=0;i<eventCount;++i){
        if(events[i].calendarIndex==HOLIDAY_FEED_INDEX) continue;
        if(eventOccursOnDay(events[i],cellStart)){
          if(dayEventCount<3) dayEventIndices[dayEventCount]=i;
          ++dayEventCount;
        }
      }

      int maxVisibleEvents=min(dayEventCount,3);
      int line=0;
      const int EVENT_STEP=UI_EVENT_STEP;
      int hidden=max(0,dayEventCount-3);
      String more=String("+")+String(hidden);
      selectFont(u8g2_font_helvR08_tf);
      int moreWidth=hidden?u8g2.getUTF8Width(more.c_str())+5:0;
      for(int n=0;n<maxVisibleEvents;++n){
        String label=compactEventLabel(events[dayEventIndices[n]]);
        int perEventLines=dayEventCount==1?3:1;
        if(dayEventCount==2){
          // Allocate the three readable lines; never shrink Hangul or cross cells.
          String first;takeTextLine(compactEventLabel(events[dayEventIndices[0]]),0,CELL_W-10,first);
          bool firstNeedsTwo=first!=compactEventLabel(events[dayEventIndices[0]]);
          perEventLines=(n==0?firstNeedsTwo:!firstNeedsTwo)?2:1;
        }
        int pos=0;
        for(int part=0;part<perEventLines && pos<(int)label.length();++part){
          int width=CELL_W-10-((line==2)?moreWidth:0);
          String piece;
          int next=takeEventTextLine(label,pos,width,piece);
          if(next<(int)label.length() && part==perEventLines-1) piece=fitEventText(label.substring(pos),width);
          drawEventText(x+5,y+UI_EVENT_BASELINE+line*EVENT_STEP,piece,C_BLACK,width);
          ++line;
          if(next<=pos) break;
          pos=next;
        }
      }
      if(hidden){
        selectFont(u8g2_font_helvR08_tf);u8g2.setForegroundColor(C_BLACK);
        u8g2.setCursor(x+CELL_W-5-moreWidth+5,y+65);u8g2.print(more);
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
    selectFont(u8g2_font_helvB18_tf);
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
