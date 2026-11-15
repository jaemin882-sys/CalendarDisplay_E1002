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
 virtual void drawPixel(int x,int y,uint16_t c){if(x>=0 && x<800 && y>=0 && y<480)pixels[y][x]=c;}
 void drawFastHLine(int x,int y,int n,uint16_t c){for(int i=0;i<n;++i)drawPixel(x+i,y,c);}
 void drawFastVLine(int x,int y,int n,uint16_t c){for(int i=0;i<n;++i)drawPixel(x,y+i,c);}
 void drawLine(int x,int y,int x1,int y1,uint16_t c){
  bool steep=abs(y1-y)>abs(x1-x);if(steep){std::swap(x,y);std::swap(x1,y1);}
  if(x>x1){std::swap(x,x1);std::swap(y,y1);}
  int dx=x1-x,dy=abs(y1-y),err=dx/2,step=y<y1?1:-1;
  for(;x<=x1;++x){if(steep)drawPixel(y,x,c);else drawPixel(x,y,c);err-=dy;if(err<0){y+=step;err+=dx;}}
 }
 virtual void fillScreen(uint16_t c){for(auto& row:pixels)for(auto& p:row)p=c;}
 void fillRect(int x,int y,int w,int h,uint16_t c){for(int i=0;i<h;++i)drawFastHLine(x,y+i,w,c);}
 void drawRect(int x,int y,int w,int h,uint16_t c){drawFastHLine(x,y,w,c);drawFastHLine(x,y+h-1,w,c);drawFastVLine(x,y,h,c);drawFastVLine(x+w-1,y,h,c);}
 void fillCircle(int x0,int y0,int r,uint16_t c){
  // Adafruit midpoint-circle raster, including the filled helper's spans.
  drawFastVLine(x0,y0-r,2*r+1,c);
  int f=1-r,dx=1,dy=-2*r,x=0,y=r,px=x,py=y;
  while(x<y){
   if(f>=0){--y;dy+=2;f+=dy;}++x;dx+=2;f+=dx;
   if(x<y+1){drawFastVLine(x0+x,y0-y,2*y+1,c);drawFastVLine(x0-x,y0-y,2*y+1,c);}
   if(y!=py){drawFastVLine(x0+py,y0-px,2*px+1,c);drawFastVLine(x0-py,y0-px,2*px+1,c);py=y;}
   px=x;
  }
 }
 void setFullWindow(){} void firstPage(){} bool nextPage(){return false;}
};

class GFXcanvas1 : public Adafruit_GFX {
public:
 int16_t width,height;
 uint8_t data[24*32]={};
 GFXcanvas1(uint16_t w,uint16_t h):width(w),height(h){}
 void drawPixel(int x,int y,uint16_t c) override {
   if(x>=0 && x<width && y>=0 && y<height)data[y*width+x]=(uint8_t)(c?1:0);
 }
 void fillScreen(uint16_t c) override {
   for(int i=0;i<width*height;++i)data[i]=(uint8_t)(c?1:0);
 }
 uint8_t getPixel(int x,int y) const{
   return (x>=0 && x<width && y>=0 && y<height)?data[y*width+x]:0;
 }
};
