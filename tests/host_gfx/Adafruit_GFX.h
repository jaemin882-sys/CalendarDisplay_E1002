#pragma once
#include "../arduino_compat.h"
#include <cmath>
class Print {
public:
 virtual size_t write(uint8_t)=0;
 void print(const char* p){while(*p)write((uint8_t)*p++);}
 void print(const String& p){print(p.c_str());}
 void print(int n){print(String(n));}
};
class Adafruit_GFX {
public:
 uint16_t pixels[480][800]={};
 void drawPixel(int x,int y,uint16_t c){if(x>=0 && x<800 && y>=0 && y<480)pixels[y][x]=c;}
 void drawFastHLine(int x,int y,int n,uint16_t c){for(int i=0;i<n;++i)drawPixel(x+i,y,c);}
 void drawFastVLine(int x,int y,int n,uint16_t c){for(int i=0;i<n;++i)drawPixel(x,y+i,c);}
 void drawLine(int x,int y,int x1,int y1,uint16_t c){if(x==x1)drawFastVLine(x,y,y1-y+1,c);else drawFastHLine(x,y,x1-x+1,c);}
 void fillScreen(uint16_t c){for(auto& row:pixels)for(auto& p:row)p=c;}
 void fillRect(int x,int y,int w,int h,uint16_t c){for(int i=0;i<h;++i)drawFastHLine(x,y+i,w,c);}
 void drawRect(int x,int y,int w,int h,uint16_t c){drawFastHLine(x,y,w,c);drawFastHLine(x,y+h-1,w,c);drawFastVLine(x,y,h,c);drawFastVLine(x+w-1,y,h,c);}
 void fillCircle(int x,int y,int r,uint16_t c){for(int j=-r;j<=r;++j)for(int i=-r;i<=r;++i)if(i*i+j*j<=r*r)drawPixel(x+i,y+j,c);}
 void setFullWindow(){} void firstPage(){} bool nextPage(){return false;}
};
