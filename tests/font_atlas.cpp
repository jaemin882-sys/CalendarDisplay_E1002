#define main firmware_render_main
#include "host_render.cpp"
#undef main
int main(){
 u8g2.begin(display);u8g2.setForegroundColor(C_BLACK);
 std::cout<<"{";
 const char* names[]={"body","title","day","more"};
 const uint8_t* fonts[]={nullptr,u8g2_font_helvB14_tf,u8g2_font_helvB10_tf,u8g2_font_helvR08_tf};
 for(int fontIndex=0;fontIndex<4;++fontIndex){
  if(fontIndex)std::cout<<",";std::cout<<"\""<<names[fontIndex]<<"\":{";bool comma=false;
  for(int cp=32;cp<=0xd7a3;++cp){
   if(cp>126 && !(fontIndex==0 && ((cp>=0xac00 && cp<=0xd7a3)||cp==0xb7)))continue;
   char utf8[8]={0};
   if(cp<128)utf8[0]=cp;
   else if(cp<2048){utf8[0]=0xc0|(cp>>6);utf8[1]=0x80|(cp&63);}
   else{utf8[0]=0xe0|(cp>>12);utf8[1]=0x80|((cp>>6)&63);utf8[2]=0x80|(cp&63);}
   const uint8_t* font=fonts[fontIndex];
   if(fontIndex==0){char buf[8]={0};readGlyph(String(utf8),0,buf,font);if(strcmp(buf,utf8))continue;}
   selectFont(font);int width=u8g2.getUTF8Width(utf8);if(width<=0)continue;
   display.fillScreen(C_WHITE);u8g2.setCursor(0,24);u8g2.print(utf8);
   if(comma)std::cout<<",";comma=true;
   std::cout<<"\""<<cp<<"\":["<<width;
   for(int y=0;y<32;++y){uint32_t row=0;for(int x=0;x<24;++x)if(display.pixels[y][x]==C_BLACK)row|=1u<<x;std::cout<<","<<row;}
   std::cout<<","<<u8g2.getCursorX()<<"]";
  }
  std::cout<<"}";
 }
 std::cout<<"}";
}
