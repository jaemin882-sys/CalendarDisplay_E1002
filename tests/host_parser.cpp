#include "arduino_compat.h"
#include <fstream>
#include <iostream>
#define TZ_INFO "KST-9"
#define CALENDAR_HOST_TEST
struct CalendarEvent {time_t start=0,end=0;bool allDay=false;String summary;uint16_t color=0;uint8_t calendarIndex=0;};
struct Feed {uint16_t color=0;} feeds[5];
static constexpr int MAX_EVENTS=512;
CalendarEvent events[MAX_EVENTS];int eventCount=0;
#include "../src/firmware_datetime.inc"
bool lineStartsWithProperty(const String& line,const char* prop){int n=line.indexOf(':');if(n<0)return false;String left=line.substring(0,n);int s=left.indexOf(';');return (s<0?left:left.substring(0,s))==prop;}
String propertyValue(const String& line){int n=line.indexOf(':');return n<0?String():line.substring(n+1);}
time_t ws,we;
void displayWindow(time_t& a,time_t& b){a=ws;b=we;}
#include "../src/firmware_ics.inc"
#ifndef HOST_RENDER
int main(int argc,char** argv){
 if(argc!=4)return 2;
 setenv("TZ",TZ_INFO,1);tzset();bool ad=false;
 ws=parseIcsDateTime(argv[2],&ad);we=parseIcsDateTime(argv[3],&ad);
 std::ifstream f(argv[1]);std::string raw((std::istreambuf_iterator<char>(f)),{});
 if(!parseIcsText(raw.c_str(),0))return 1;
 sortEvents();
 for(int i=0;i<eventCount;++i)std::cout<<events[i].start<<"\t"<<events[i].end<<"\t"<<events[i].summary.c_str()<<"\n";
}

#endif
