#include "ui_layout.h"
extern int batteryPercent;

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

// Scale the same Unifont Korean2 glyph raster to 14/16 without changing
// title/date/weekday font choices. A tiny 1-bit canvas buffers one glyph.
GFXcanvas1 compactGlyphCanvas(24,32);
U8G2_FOR_ADAFRUIT_GFX compactGlyphPrinter;
bool compactGlyphReady=false;
void initCompactGlyph(){
  if(compactGlyphReady)return;
  compactGlyphPrinter.begin(compactGlyphCanvas);
  compactGlyphPrinter.setFontMode(1);
  compactGlyphReady=true;
}
int readEventGlyph(const String& text,int offset,char (&buf)[8],const uint8_t*& font){
  return readGlyph(text,offset,buf,font);
}
int compactGlyphWidth(const char* utf8,const uint8_t* font){
  int nativeWidth=glyphWidthFor(utf8,font);
  return max(1,(nativeWidth*14+8)/16);
}
void drawCompactGlyph(int x,int baselineY,const char* utf8,const uint8_t* font,uint16_t color){
  initCompactGlyph();
  compactGlyphCanvas.fillScreen(0);
  compactGlyphPrinter.setFont(font);
  compactGlyphPrinter.setFontMode(1);
  compactGlyphPrinter.setForegroundColor(1);
  compactGlyphPrinter.setBackgroundColor(0);
  compactGlyphPrinter.setCursor(0,24);
  compactGlyphPrinter.print(utf8);
  int glyphW=min(24,glyphWidthFor(utf8,font));
  // Nearest-neighbour downscale from 16px Unifont raster to 14px.
  for(int sy=0;sy<32;++sy){
    for(int sx=0;sx<glyphW;++sx){
      if(!compactGlyphCanvas.getPixel(sx,sy))continue;
      int dx=(sx*14)/16;
      int dy=((sy-24)*14)/16;
      display.drawPixel(x+dx,baselineY+dy,color);
    }
  }
}
static constexpr int EVENT_TRACKING=-1;

int eventGlyphAdvance(const char* utf8,const uint8_t* font){
  return max(1,compactGlyphWidth(utf8,font)+EVENT_TRACKING);
}

int takeEventTextLine(const String& input,int startByte,int maxPixels,String& out){
  out="";
  int used=0;
  int i=startByte;
  while(i<input.length()){
    char buf[8]={0};const uint8_t* font=nullptr;
    int len=readEventGlyph(input,i,buf,font);
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
    int len=readEventGlyph(input,i,buf,font);
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
    int len=readEventGlyph(text,i,buf,font);
    int advance=eventGlyphAdvance(buf,font);
    if(cursor+advance>x+maxWidth) break;
    drawCompactGlyph(cursor,baselineY,buf,font,color);
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

// D-Day candidates are extracted from the actual iCloud feeds by firmware_ics.inc.
int upcomingAnniversaries(time_t today,String labels[2],int days[2]){
  int count=0;
  for(int i=0;i<upcomingReminderCount && count<2;++i){
    if(upcomingReminders[i].day<today)continue;
    labels[count]=upcomingReminders[i].label;
    days[count]=(int)((upcomingReminders[i].day-today)/86400);
    ++count;
  }
  return count;
}

// All weather marks fit a 13x13 box. Shapes remain distinct without color.
void drawWeatherIcon(int x,int y,WeatherIcon icon){
  if(icon==WeatherIcon::Snow){
    display.drawLine(x+6,y+1,x+6,y+11,C_BLUE);
    display.drawLine(x+1,y+3,x+11,y+9,C_BLUE);
    display.drawLine(x+1,y+9,x+11,y+3,C_BLUE);
    display.drawLine(x+4,y+1,x+6,y+3,C_BLUE);display.drawLine(x+6,y+3,x+8,y+1,C_BLUE);
    display.drawLine(x+4,y+11,x+6,y+9,C_BLUE);display.drawLine(x+6,y+9,x+8,y+11,C_BLUE);
    return;
  }
  if(icon==WeatherIcon::Sun || icon==WeatherIcon::PartlyCloudy){
    int cx=x+(icon==WeatherIcon::Sun?6:4),cy=y+4;
    display.fillCircle(cx,cy,3,C_BLACK);display.fillCircle(cx,cy,2,C_YELLOW);
    display.drawPixel(cx,cy-4,C_BLACK);display.drawPixel(cx,cy+4,C_BLACK);
    display.drawPixel(cx-4,cy,C_BLACK);display.drawPixel(cx+4,cy,C_BLACK);
    display.drawPixel(cx-3,cy-3,C_BLACK);display.drawPixel(cx+3,cy-3,C_BLACK);
    display.drawPixel(cx-3,cy+3,C_BLACK);display.drawPixel(cx+3,cy+3,C_BLACK);
    if(icon==WeatherIcon::Sun)return;
  }
  display.fillCircle(x+3,y+6,2,C_BLACK);display.fillCircle(x+7,y+5,3,C_BLACK);
  display.fillCircle(x+10,y+7,2,C_BLACK);display.fillRect(x+3,y+6,8,4,C_BLACK);
  if(icon==WeatherIcon::Rain){
    for(int i=0;i<3;++i)display.drawLine(x+3+i*4,y+10,x+2+i*4,y+12,C_BLUE);
  }
}

int drawDayWeather(int right,int y,int available,const DailyWeather& weather){
  char text[24];snprintf(text,sizeof(text),"%d/%d°",weatherDegrees(weather.low),weatherDegrees(weather.high));
  selectFont(u8g2_font_helvR08_tf);
  int width=u8g2.getUTF8Width(text);
  if(width+UI_WEATHER_ICON+UI_WEATHER_GAP>available){
    selectFont(u8g2_font_5x7_tf);width=u8g2.getUTF8Width(text);
  }
  if(width+UI_WEATHER_ICON+UI_WEATHER_GAP>available){
    selectFont(u8g2_font_4x6_tf);width=u8g2.getUTF8Width(text);
  }
  int left=right-width-UI_WEATHER_GAP-UI_WEATHER_ICON;
  u8g2.setForegroundColor(C_BLACK);u8g2.setCursor(right-width,y+14);u8g2.print(text);
  drawWeatherIcon(left,y+4,weatherIcon(weather.code));return left;
}

void drawHolidayHeader(int x,int y,const String& holiday,int width){
  if(width<=0 || holiday.isEmpty())return;
  String text;int end=takeEventTextLine(holiday,0,width,text);
  if(end==(int)holiday.length()){drawEventText(x,y,text,C_RED,width);return;}
  // Draw the ellipsis as three pixels: no dependency on a missing Unicode glyph.
  const int ellipsisWidth=5;
  if(width<ellipsisWidth)return;
  takeEventTextLine(holiday,0,width-ellipsisWidth-1,text);
  int used=drawEventText(x,y,text,C_RED,width);
  for(int i=0;i<3;++i)display.drawPixel(x+used+i*2,y,C_RED);
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
  feedBytes("layout-daily-weather-v9");
  // Hash rendered values only: raw tenths/WMO changes with identical pixels do not refresh.
  for(int slot=0;slot<42;++slot){
    const DailyWeather* w=weatherEnabled?weatherForDay(weatherCache,weatherDay(windowStart+(time_t)slot*86400)):nullptr;
    if(!w){feedBytes("|");continue;}
    char text[40];snprintf(text,sizeof(text),"|%d/%d/%d",weatherDegrees(w->low),weatherDegrees(w->high),(int)weatherIcon(w->code));
    feedBytes(text);
  }
  for(int i=0;i<upcomingReminderCount;++i){
    feedBytes(upcomingReminders[i].label);
    feedBytes(String((long long)upcomingReminders[i].day));
  }
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

    // Header has three independent alignment zones:
    // left: date range + household memo, center: next D-Day, right: battery.
    const int LEFT_END=280;
    // Center the whole D-Day label on the 800px panel (x=400).
    const int CENTER_LEFT=284;
    const int CENTER_RIGHT=516;
    const int RIGHT_EDGE=790;
    int titleWidth=u8g2.getUTF8Width(title);
    int memoX=10+titleWidth+12;
    int memoWidth=max(0,LEFT_END-memoX);
    if(memoWidth>0)
      drawUtf8Korean(memoX,23,fitText(String(CUSTOM_MEMO_TEXT),memoWidth),C_BLACK,memoWidth);

    String ddayLabels[2];
    int ddayDays[2]={0,0};
    int ddayCount=upcomingAnniversaries(todayMidnight,ddayLabels,ddayDays);
    if(ddayCount>0){
      // Two compact labels, kept together and centered as one group.
      String chunks[2];
      int chunkWidths[2]={0,0};
      for(int n=0;n<ddayCount;++n){
        String countdown=ddayDays[n]==0?String("D-DAY"):String("D-")+String(ddayDays[n]);
        chunks[n]=ddayLabels[n]+String(" ")+countdown;
        int k=0;
        while(k<chunks[n].length()){
          char buf[8]={0};const uint8_t* font=nullptr;
          int len=readGlyph(chunks[n],k,buf,font);
          chunkWidths[n]+=glyphWidthFor(buf,font);
          k+=len;
        }
      }
      const int groupGap=12;
      const int fullWidth=chunkWidths[0]+(ddayCount>1?groupGap+chunkWidths[1]:0);
      const int maxWidth=CENTER_RIGHT-CENTER_LEFT;
      int x=CENTER_LEFT+(maxWidth-fullWidth)/2;
      for(int n=0;n<ddayCount;++n){
        drawUtf8Korean(x,23,chunks[n],C_BLACK,chunkWidths[n]+1);
        x+=chunkWidths[n]+groupGap;
      }
    }

    if(batteryPercent>=0){
      char batteryText[12];
      snprintf(batteryText,sizeof(batteryText),"%d%%",batteryPercent);
      selectFont(u8g2_font_helvB10_tf);
      int batteryWidth=u8g2.getUTF8Width(batteryText);
      u8g2.setForegroundColor(C_BLACK);
      u8g2.setCursor(RIGHT_EDGE-batteryWidth,23);
      u8g2.print(batteryText);
    }
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

      // Highlight the actual boundary between calendar months.
      // A month beginning on Sunday gets a full-width horizontal divider;
      // otherwise only the vertical edge immediately before day 1 is bold.
      if(slot>0 && cellTm.tm_mday==1){
        if(col==0) display.fillRect(LEFT,y-1,RIGHT-LEFT+1,3,C_BLACK);
        else display.fillRect(x-1,y,3,ROW_H+1,C_BLACK);
      }

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
      int headerRight=x+CELL_W-UI_WEATHER_RIGHT;
      const DailyWeather* weather=weatherEnabled?weatherForDay(weatherCache,weatherDay(cellStart)):nullptr;
      if(weather)headerRight=drawDayWeather(headerRight,y,headerRight-(x+5+dateWidth+UI_HEADER_GAP),*weather)-UI_HEADER_GAP;
      int hx=x+5+dateWidth+UI_HEADER_GAP;
      drawHolidayHeader(hx,y+15,holiday,headerRight-hx);
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

// Guest mode intentionally renders no personal event grid. D-Days are public
// per household preference; weather comes from the same persisted cache.
int scaledKoreanWidth(const String& label,int scale){
  int width=0;
  for(int i=0;i<(int)label.length();){
    char glyph[8]={0};const uint8_t* font=nullptr;
    int bytes=readGlyph(label,i,glyph,font);
    width+=glyphWidthFor(glyph,font)*scale;i+=bytes;
  }
  return width;
}
void drawScaledKorean(int x,int baseline,const String& label,int scale,uint16_t color){
  initCompactGlyph();
  for(int i=0;i<(int)label.length();){
    char glyph[8]={0};const uint8_t* font=nullptr;
    int bytes=readGlyph(label,i,glyph,font);
    int width=min(24,glyphWidthFor(glyph,font));
    compactGlyphCanvas.fillScreen(0);
    compactGlyphPrinter.setFont(font);compactGlyphPrinter.setFontMode(1);
    compactGlyphPrinter.setForegroundColor(1);
    compactGlyphPrinter.setBackgroundColor(0);
    compactGlyphPrinter.setCursor(0,24);compactGlyphPrinter.print(glyph);
    for(int py=0;py<32;++py)
      for(int px=0;px<width;++px)
        if(compactGlyphCanvas.getPixel(px,py))
          display.fillRect(x+px*scale,baseline+(py-24)*scale,scale,scale,color);
    x+=width*scale;i+=bytes;
  }
}
uint32_t computeGuestRenderHash(time_t todayMidnight){
  uint32_t h=2166136261u;
  auto feedBytes=[&](const String& s){
    for(size_t i=0;i<s.length();++i){h^=(uint8_t)s[i];h*=16777619u;}
  };
  feedBytes("guest-weather-dday-v1");
  feedBytes(CUSTOM_MEMO_TEXT);
  feedBytes(String((long long)todayMidnight));
  for(int offset=0;offset<2;++offset){
    const DailyWeather* w=(weatherEnabled && todayMidnight>0)?
      weatherForDay(weatherCache,weatherDay(todayMidnight+(time_t)offset*86400)):nullptr;
    if(!w){feedBytes("|none");continue;}
    char value[40];
    snprintf(value,sizeof(value),"|%d/%d/%d",weatherDegrees(w->low),
             weatherDegrees(w->high),(int)weatherIcon(w->code));
    feedBytes(value);
  }
  String labels[2];int days[2]={0,0};
  int count=todayMidnight>0?upcomingAnniversaries(todayMidnight,labels,days):0;
  for(int i=0;i<count;++i){feedBytes(labels[i]);feedBytes(String(days[i]));}
  return h;
}
void drawGuestPage(time_t todayMidnight,bool dataFresh){
  display.setFullWindow();
  display.firstPage();
  do{
    display.fillScreen(C_WHITE);
    display.fillRect(8,7,784,4,C_GREEN);
    struct tm now={};
    if(todayMidnight>0){
      localtime_r(&todayMidnight,&now);
      char dateText[24];
      snprintf(dateText,sizeof(dateText),"%04d.%02d.%02d",
               now.tm_year+1900,now.tm_mon+1,now.tm_mday);
      selectFont(u8g2_font_helvB10_tf);
      u8g2.setForegroundColor(C_BLACK);u8g2.setCursor(30,35);
      u8g2.print(dateText);
    }
    if(batteryPercent>=0){
      char batteryText[12];
      snprintf(batteryText,sizeof(batteryText),"%d%%",batteryPercent);
      selectFont(u8g2_font_helvB10_tf);
      u8g2.setForegroundColor(C_BLACK);
      u8g2.setCursor(775-u8g2.getUTF8Width(batteryText),35);
      u8g2.print(batteryText);
    }
    // Original six-color e-paper artwork: hill lines and a sunrise.
    display.fillCircle(401,71,18,C_YELLOW);
    display.drawLine(303,115,354,75,C_GREEN);
    display.drawLine(354,75,411,115,C_GREEN);
    display.drawLine(376,115,444,59,C_BLUE);
    display.drawLine(444,59,507,115,C_BLUE);
    display.drawLine(448,115,484,87,C_GREEN);
    display.drawLine(484,87,519,115,C_GREEN);

    String title(CUSTOM_MEMO_TEXT);
    int scale=scaledKoreanWidth(title,2)<=600?2:1;
    int titleWidth=scaledKoreanWidth(title,scale);
    drawScaledKorean((800-titleWidth)/2,164,title,scale,C_BLACK);
    selectFont(u8g2_font_helvR10_tf);
    u8g2.setForegroundColor(C_BLACK);
    const char* greeting="Welcome Home";
    u8g2.setCursor((800-u8g2.getUTF8Width(greeting))/2,190);
    u8g2.print(greeting);
    display.drawLine(55,207,745,207,C_BLACK);

    // Weather: two large uncluttered cards, today and tomorrow.
    for(int offset=0;offset<2;++offset){
      int cardX=64+offset*342;
      display.drawRect(cardX,223,326,104,C_BLACK);
      drawUtf8Korean(cardX+20,251,offset==0?String("오늘"):String("내일"),C_BLACK,180);
      const DailyWeather* w=(weatherEnabled && todayMidnight>0)?
        weatherForDay(weatherCache,weatherDay(todayMidnight+(time_t)offset*86400)):nullptr;
      if(w){
        drawWeatherIcon(cardX+27,276,weatherIcon(w->code));
        char temperature[32];
        snprintf(temperature,sizeof(temperature),"%d / %d°",
                 weatherDegrees(w->low),weatherDegrees(w->high));
        selectFont(u8g2_font_helvB18_tf);
        u8g2.setForegroundColor(C_BLACK);
        u8g2.setCursor(cardX+70,305);u8g2.print(temperature);
      }else drawUtf8Korean(cardX+24,302,String("예보 없음"),C_BLACK,276);
    }

    display.drawLine(55,346,745,346,C_BLACK);
    String heading("다가오는 기념일");
    drawUtf8Korean((800-scaledKoreanWidth(heading,1))/2,375,heading,C_BLACK,500);
    String labels[2];int days[2]={0,0};
    int count=todayMidnight>0?upcomingAnniversaries(todayMidnight,labels,days):0;
    if(count==0){
      String empty("표시할 기념일 없음");
      drawUtf8Korean((800-scaledKoreanWidth(empty,1))/2,428,empty,C_BLACK,550);
    }else{
      for(int i=0;i<count;++i){
        int x=count==1?240:(i==0?78:424);
        drawUtf8Korean(x,412,fitText(labels[i],290),C_BLACK,290);
        char countdown[20];
        if(days[i]==0)snprintf(countdown,sizeof(countdown),"D-DAY");
        else snprintf(countdown,sizeof(countdown),"D-%d",days[i]);
        selectFont(u8g2_font_helvB18_tf);
        u8g2.setForegroundColor(C_BLUE);
        u8g2.setCursor(x,446);u8g2.print(countdown);
      }
    }
    if(!dataFresh){
      selectFont(u8g2_font_helvR08_tf);
      u8g2.setForegroundColor(C_BLACK);
      u8g2.setCursor(26,470);u8g2.print("OFFLINE");
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
