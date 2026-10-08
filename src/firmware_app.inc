static bool displayInitialized=false;
int batteryPercent=-1;
int batteryMilliVolts=0;

int estimateBatteryPercent(int mv){
  // Approximate Li-ion resting-voltage curve; voltage is not an exact SOC meter.
  const int voltage[]={3270,3410,3490,3580,3680,3750,3800,3850,3910,3960,4150};
  const int percent[]={0,10,20,30,40,50,60,70,80,90,100};
  if(mv<=voltage[0]) return 0;
  if(mv>=voltage[10]) return 100;
  for(int i=1;i<11;++i){
    if(mv<=voltage[i])
      return percent[i-1]+(mv-voltage[i-1])*(percent[i]-percent[i-1])/(voltage[i]-voltage[i-1]);
  }
  return 100;
}
void measureBattery(){
  analogReadResolution(12);
  analogSetPinAttenuation(1,ADC_11db);
  pinMode(21,OUTPUT);
  digitalWrite(21,HIGH);
  delay(15);
  analogReadMilliVolts(1); // Discard first ADC reading.
  uint32_t total=0;
  for(int i=0;i<8;++i){total+=(uint32_t)analogReadMilliVolts(1);delay(2);}
  digitalWrite(21,LOW);
  batteryMilliVolts=(int)(total/8)*2;
  if(batteryMilliVolts>=2800 && batteryMilliVolts<=4500){
    batteryPercent=estimateBatteryPercent(batteryMilliVolts);
    Serial.printf("Battery estimate: %d mV, %d%% (USB/charging may affect accuracy)\n",batteryMilliVolts,batteryPercent);
  }else{
    batteryPercent=-1;
    Serial.printf("Battery measurement unavailable: %d mV\n",batteryMilliVolts);
  }
}

void initDisplay(){
  if(displayInitialized) return;
  pinMode(EPD_RES_PIN,OUTPUT);pinMode(EPD_DC_PIN,OUTPUT);pinMode(EPD_CS_PIN,OUTPUT);
  hspi.begin(EPD_SCK_PIN,-1,EPD_MOSI_PIN,-1);
  display.epd2.selectSPI(hspi,SPISettings(2000000,MSBFIRST,SPI_MODE0));
  display.init(115200);display.setRotation(0);
  u8g2.begin(display);u8g2.setFontMode(1);u8g2.setFontDirection(0);
  displayInitialized=true;
}
void quietPeripherals(){
  // Shared SPI: deselect microSD even when a card is inserted.
  pinMode(14,OUTPUT);digitalWrite(14,HIGH);
  pinMode(16,OUTPUT);digitalWrite(16,LOW); // SD power disabled
  pinMode(21,OUTPUT);digitalWrite(21,LOW); // Battery measurement divider disabled
  pinMode(6,OUTPUT);digitalWrite(6,HIGH);  // Active-low LED off
}
void sleepFor24hOrButton(){
  if(displayInitialized){display.hibernate();hspi.end();}
  WiFi.disconnect(true);WiFi.mode(WIFI_OFF);
  quietPeripherals();
  pinMode(SYNC_BUTTON_PIN,INPUT_PULLUP);
  uint32_t start=millis();
  while(digitalRead(SYNC_BUTTON_PIN)==LOW && millis()-start<3000) delay(20);
  esp_err_t timerResult=esp_sleep_enable_timer_wakeup((uint64_t)AUTO_SYNC_HOURS*3600ULL*1000000ULL);
  if(timerResult!=ESP_OK){Serial.println("Invalid sleep interval");esp_restart();}
  if(digitalRead(SYNC_BUTTON_PIN)==HIGH){
    rtc_gpio_init((gpio_num_t)SYNC_BUTTON_PIN);
    rtc_gpio_set_direction((gpio_num_t)SYNC_BUTTON_PIN,RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en((gpio_num_t)SYNC_BUTTON_PIN);
    rtc_gpio_pulldown_dis((gpio_num_t)SYNC_BUTTON_PIN);
    // ESP32-S3 supports ANY_LOW on this pinned framework.
    esp_err_t result=esp_sleep_enable_ext1_wakeup(1ULL<<SYNC_BUTTON_PIN,ESP_EXT1_WAKEUP_ANY_LOW);
    if(result!=ESP_OK) Serial.printf("EXT1 setup failed: %d\n",result);
    // Keep the RTC pull-up powered in the pinned IDF 4.4 runtime.
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH,ESP_PD_OPTION_ON);
  }else Serial.println("KEY0 held: timer-only sleep prevents a wake loop. Release, then reset if needed.");
  Serial.printf("Sleep: %u h; awake %u ms; min heap %u\n",AUTO_SYNC_HOURS,millis(),ESP.getMinFreeHeap());
  Serial.flush();delay(100);esp_deep_sleep_start();
}
void saveHolidayCache(){
  String data="BEGIN:VCALENDAR\nVERSION:2.0\n";
  for(int i=0;i<eventCount;++i){
    const auto& e=events[i];if(e.calendarIndex!=HOLIDAY_FEED_INDEX) continue;
    struct tm a={},b={};gmtime_r(&e.start,&a);gmtime_r(&e.end,&b);
    char start[20],end[20];strftime(start,sizeof(start),"%Y%m%dT%H%M%SZ",&a);strftime(end,sizeof(end),"%Y%m%dT%H%M%SZ",&b);
    String title=e.summary;title.replace("\\","\\\\");title.replace("\n","\\n");
    data+="BEGIN:VEVENT\nUID:h"+String(i)+"\nDTSTART:"+String(start)+"\nDTEND:"+String(end)+"\nSUMMARY:"+title+"\nEND:VEVENT\n";
  }
  data+="END:VCALENDAR\n";
  if(data.length()<=32768 && data!=prefs.getString("holidays","")) prefs.putString("holidays",data);
}
void setup(){
  Serial.begin(115200);delay(300);
  rtc_gpio_deinit((gpio_num_t)SYNC_BUTTON_PIN);
  pinMode(SYNC_BUTTON_PIN,INPUT_PULLUP);quietPeripherals();
  setenv("TZ",TZ_INFO,1);tzset();
  measureBattery();
  auto cause=esp_sleep_get_wakeup_cause();
  bool manualWake=cause==ESP_SLEEP_WAKEUP_EXT1;
  Serial.printf("CalendarDisplay v1.0.0: %s wake; PSRAM %u\n",manualWake?"KEY0":(cause==ESP_SLEEP_WAKEUP_TIMER?"timer":"power/reset"),ESP.getPsramSize());
  prefs.begin("caldisplay",false);
  bool hadCalendar=prefs.getBool("hasCalendar",false) || prefs.getUInt("renderHash",0)!=0;
  if(!configurationLooksValid()){
    Serial.println("Missing/invalid config.h; no credentials are logged.");
    if(!hadCalendar){initDisplay();drawStatusPage("Setup required","Wi-Fi / iCloud 설정이 필요합니다.","src/config.h 설정 후 다시 업로드하세요.",C_YELLOW);}
    prefs.end();sleepFor24hOrButton();return;
  }
  bool wifiOk=connectWiFi();
  bool clockOk=wifiOk && syncClock();
  bool dataOk=wifiOk && clockOk,holidayFresh=false;
  if(dataOk){
    for(uint8_t i=0;i<HOLIDAY_FEED_INDEX;++i){
      if(!strlen(feeds[i].url)) continue;
      if(!downloadAndParseFeed(i)) dataOk=false;
    }
    if(dataOk){
      holidayFresh=downloadAndParseFeed(HOLIDAY_FEED_INDEX);
      if(holidayFresh) saveHolidayCache();
      else{
        String cached=prefs.getString("holidays","");
        if(cached.length()) parseIcsText(cached.c_str(),HOLIDAY_FEED_INDEX);
        Serial.println("Holiday source unavailable: cached overlap only; personal calendars remain usable.");
      }
      sortEvents();
    }
  }
  WiFi.disconnect(true);WiFi.mode(WIFI_OFF); // Radio off during the slow ePaper refresh.
  if(dataOk){
    struct tm now={};getLocalTime(&now,100);
    now.tm_hour=0;now.tm_min=0;now.tm_sec=0;now.tm_isdst=-1;
    time_t today=mktime(&now),ws=0,we=0;displayWindow(ws,we);
    uint32_t hash=computeRenderHash(ws,today,holidayFresh);
    // Update display only when the estimate changes meaningfully (5% buckets).
    hash^=(uint32_t)(batteryPercent<0?255:batteryPercent/5)*16777619u;
    if(manualWake || !hadCalendar || prefs.getUInt("renderHash",0)!=hash){
      initDisplay();drawCalendarPage(today,holidayFresh);
      if(digitalRead(EPD_BUSY_PIN)==HIGH){
        prefs.putUInt("renderHash",hash);prefs.putBool("hasCalendar",true);
      }else Serial.println("Panel remains BUSY; render hash not committed, next wake retries.");
      Serial.println("Display update returned. Inspect panel; driver timeout is printed if BUSY fails.");
    }else Serial.println("Identical image: panel initialization/refresh skipped.");
  }else{
    Serial.printf("Sync failed: WiFi=%d clock=%d data=%d; retaining prior calendar=%d\n",wifiOk,clockOk,dataOk,hadCalendar);
    if(!hadCalendar){
      initDisplay();drawStatusPage("Sync failed",!wifiOk?"Wi-Fi 연결에 실패했습니다.":(!clockOk?"시간 동기화에 실패했습니다.":"캘린더 다운로드 또는 해석에 실패했습니다."),"설정과 시리얼 로그 확인 후 KEY0를 누르세요.",C_RED);
    }
  }
  prefs.end();sleepFor24hOrButton();
}
void loop(){}
