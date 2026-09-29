#include "tls_roots.h"
// HTTPClient decodes chunked transfer into this bounded sink. No second ICS copy.
class IcsBuffer: public Stream {
public:
  char* data=nullptr;size_t used=0,capacity=0;uint32_t deadline=0;
  explicit IcsBuffer(size_t size):capacity(size),deadline(millis()+45000){
    data=(char*)heap_caps_malloc(size+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!data && size<=98304) data=(char*)malloc(size+1);
    if(data) data[0]=0;
  }
  ~IcsBuffer(){free(data);}
  size_t write(uint8_t c) override {return write(&c,1);}
  size_t write(const uint8_t* src,size_t len) override {
    if(!data || used+len>capacity || (int32_t)(millis()-deadline)>0) return 0;
    memcpy(data+used,src,len);used+=len;data[used]=0;return len;
  }
  int available() override {return 0;}
  int read() override {return -1;}
  int peek() override {return -1;}
  void flush() override {}
};
bool downloadAndParseFeed(uint8_t index){
  String url=normalizeIcsUrl(feeds[index].url);
  if(url.isEmpty()) return true;
  if(!url.startsWith("https://")) return false;
  WiFiClientSecure client;
  client.setCACert(TLS_ROOTS); // Fail closed; no setInsecure fallback.
  client.setHandshakeTimeout(15);
  HTTPClient http;
  http.setConnectTimeout(12000);
  http.setTimeout(12000);
  http.setRedirectLimit(4);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if(!http.begin(client,url)) return false;
  http.addHeader("Accept","text/calendar");
  http.addHeader("Accept-Encoding","identity");
  int code=http.GET();
  if(code!=HTTP_CODE_OK){
    Serial.printf("Feed %u HTTP/TLS failure: %d\n",index+1,code);http.end();return false;
  }
  int declared=http.getSize();
  const size_t limit=psramFound()?768*1024:96*1024;
  if(declared>(int)limit){http.end();return false;}
  IcsBuffer body(declared>=0?(size_t)declared:limit);
  if(!body.data){http.end();return false;}
  int received=http.writeToStream(&body);
  http.end();
  if(received<0 || (declared>=0 && received!=declared) || body.used!=size_t(received)) return false;
  // A NUL in ICS would hide a truncated suffix from the parser.
  if(memchr(body.data,0,body.used)) return false;
  Serial.printf("Feed %u: %u bytes; free heap %u, PSRAM %u\n",index+1,(unsigned)body.used,ESP.getFreeHeap(),ESP.getFreePsram());
  return parseIcsText(body.data,index);
}
