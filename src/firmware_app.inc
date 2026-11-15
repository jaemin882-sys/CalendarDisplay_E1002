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
void loadWeatherCache(){
  double latitude=0,longitude=0;
  weatherEnabled=weatherCoordinate(WEATHER_LATITUDE,90,latitude) &&
    weatherCoordinate(WEATHER_LONGITUDE,180,longitude);
  if(!weatherEnabled){Serial.println("Weather disabled: coordinates not configured/invalid.");return;}
  if(!prefs.isKey("weatherV1"))return;
  uint8_t data[WEATHER_BLOB_SIZE];
  if(prefs.getBytesLength("weatherV1")!=sizeof(data) ||
     prefs.getBytes("weatherV1",data,sizeof(data))!=sizeof(data) ||
     !decodeWeather(data,sizeof(data),weatherCache))
    Serial.println("Weather cache rejected: invalid length/version/CRC/records.");
}
void syncWeather(bool wifiOk,bool clockOk){
  if(!wifiOk || !clockOk || !weatherEnabled)return;
  WeatherCache forecast;
  uint32_t today=weatherDay(time(nullptr));
  if(!downloadWeather(today,forecast)){
    Serial.println("Weather unavailable: retaining cached forecasts; calendar sync continues.");return;
  }
  WeatherCache next=mergeWeather(weatherCache,forecast,today);
  uint8_t data[WEATHER_BLOB_SIZE],old[WEATHER_BLOB_SIZE];
  encodeWeather(next,data);
  bool same=prefs.getBytesLength("weatherV1")==sizeof(old) &&
    prefs.getBytes("weatherV1",old,sizeof(old))==sizeof(old) && !memcmp(data,old,sizeof(data));
  // NVS commits a blob atomically. Do not split its records/checksum across keys.
  if(!same && prefs.putBytes("weatherV1",data,sizeof(data))!=sizeof(data)){
    Serial.println("Weather cache write failed: retaining previous forecasts.");return;
  }
  weatherCache=next;
  Serial.printf("Weather: %u cached days; NVS %s\n",next.count,same?"unchanged":"updated");
}

// Guest D-Day fallback contains ONLY the two public reminders, never private
// appointments. It lets KEY0 hide the calendar before any Wi-Fi work.
time_t validLocalMidnight(){
  time_t now=time(nullptr);
  return now>=1704067200?midnightFor(now):0;
}
void saveGuestReminders(){
  String content="v1\n";
  for(int i=0;i<upcomingReminderCount && i<2;++i){
    String label=upcomingReminders[i].label;
    label.replace("\n"," ");label.replace("\r"," ");label.replace("\t"," ");
    content+=String((long long)upcomingReminders[i].day)+"\t"+label+"\n";
  }
  if(content.length()<=750 && content!=prefs.getString("guestDdays",""))
    prefs.putString("guestDdays",content);
}
void restoreGuestReminders(time_t today){
  if(today<=0)return; // Unknown RTC: never invent D-Day countdowns.
  String content=prefs.getString("guestDdays","");
  if(content.length()>750 || !content.startsWith("v1\n"))return;
  int pos=3;
  while(pos<(int)content.length()){
    int end=content.indexOf('\n',pos);
    if(end<0)break;
    String line=content.substring(pos,end);pos=end+1;
    int tab=line.indexOf('\t');if(tab<=0)continue;
    time_t when=(time_t)atoll(line.substring(0,tab).c_str());
    String label=line.substring(tab+1);
    if(label.isEmpty() || when<today || when>today+(time_t)366*9*86400)continue;
    addUpcomingReminder(label,when,today);
  }
}
Key0Action readKey0Action(bool keyWake){
  if(!keyWake)return Key0Action::Auto;
  // KEY0 is already debounced electrically by EXT1 wake. Measure residual
  // hold after boot (boot consumes ~300 ms), before Wi-Fi or e-paper work.
  uint32_t started=millis();
  while(digitalRead(SYNC_BUTTON_PIN)==LOW && millis()-started<KEY0_MODE_HOLD_MS)
    delay(15);
  bool longHold=digitalRead(SYNC_BUTTON_PIN)==LOW;
  Key0Action action=classifyKey0(true,longHold);
  Serial.println(action==Key0Action::SwitchMode?
                 "[KEY0] Long press: switch display mode":
                 "[KEY0] Short press: refresh current mode");
  return action;
}
void renderGuestIfChanged(bool force,bool hadDisplay,bool dataFresh){
  time_t today=validLocalMidnight();
  uint32_t hash=computeGuestRenderHash(today);
  hash^=(uint32_t)(batteryPercent<0?255:batteryPercent/5)*16777619u;
  if(!dataFresh)hash^=0x2fa819e5u; // The bottom-left OFFLINE marker is visible.
  uint32_t oldHash=prefs.getUInt("renderHash",0);
  Serial.printf("[GUEST] fresh=%d force=%d hash_old=%08x hash_new=%08x\n",
                dataFresh,force,(unsigned)oldHash,(unsigned)hash);
  if(force || !hadDisplay || oldHash!=hash){
    Serial.println("[GUEST] Rendering private-event-free dashboard");
    Serial.flush();
    initDisplay();drawGuestPage(today,dataFresh);
    if(digitalRead(EPD_BUSY_PIN)==HIGH){
      prefs.putUInt("renderHash",hash);prefs.putBool("hasCalendar",true);
    }else Serial.println("[GUEST] Panel BUSY; render hash not committed");
  }else Serial.println("[GUEST] Same image; refresh skipped");
}

void setup(){
  Serial.begin(115200,SERIAL_8N1,44,43);
  delay(300);
  Serial.println("[BOOT] E1002 CH340 UART diagnostic enabled");
  rtc_gpio_deinit((gpio_num_t)SYNC_BUTTON_PIN);
  pinMode(SYNC_BUTTON_PIN,INPUT_PULLUP);quietPeripherals();
  setenv("TZ",TZ_INFO,1);tzset();
  measureBattery();
  auto cause=esp_sleep_get_wakeup_cause();
  bool manualWake=cause==ESP_SLEEP_WAKEUP_EXT1;
  Key0Action keyAction=readKey0Action(manualWake);
  Serial.printf("CalendarDisplay v1.0.0: %s wake; PSRAM %u\n",manualWake?"KEY0":(cause==ESP_SLEEP_WAKEUP_TIMER?"timer":"power/reset"),ESP.getPsramSize());
  prefs.begin("caldisplay",false);
  bool guestMode=prefs.getBool("guestMode",false);
  if(keyAction==Key0Action::SwitchMode){
    guestMode=guestModeAfterAction(guestMode,keyAction);
    if(!prefs.putBool("guestMode",guestMode))
      Serial.println("[GUEST] Mode save failed; retry persistence on next wake");
    Serial.printf("[GUEST] Mode changed to %s\n",guestMode?"guest":"calendar");
  }
  loadWeatherCache();
  bool hadCalendar=prefs.getBool("hasCalendar",false) || prefs.getUInt("renderHash",0)!=0;
  // On entering guest mode, hide the private calendar BEFORE attempting network.
  // Persisted weather and the two D-Days are enough to render immediately.
  if(keyAction==Key0Action::SwitchMode && guestMode){
    restoreGuestReminders(validLocalMidnight());
    renderGuestIfChanged(true,hadCalendar,true);
    prefs.end();sleepFor24hOrButton();return;
  }
  if(!configurationLooksValid()){
    Serial.println("Missing/invalid config.h; no credentials are logged.");
    if(guestMode){
      restoreGuestReminders(validLocalMidnight());
      renderGuestIfChanged(keyAction!=Key0Action::Auto,hadCalendar,false);
    }else if(!hadCalendar || keyAction==Key0Action::SwitchMode){
      initDisplay();drawStatusPage("Setup required","Wi-Fi / iCloud 설정이 필요합니다.","src/config.h 설정 후 다시 업로드하세요.",C_YELLOW);
    }
    prefs.end();sleepFor24hOrButton();return;
  }
  Serial.println("[SYNC] Connecting Wi-Fi...");
  bool wifiOk=connectWiFi();
  Serial.printf("[SYNC] Wi-Fi %s\n",wifiOk?"connected":"failed");
  bool clockOk=wifiOk && syncClock();
  Serial.printf("[SYNC] Clock %s\n",clockOk?"ready":"failed");
  bool dataOk=wifiOk && clockOk,holidayFresh=false;
  syncWeather(wifiOk,clockOk);
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
      saveGuestReminders();
    }
  }
  WiFi.disconnect(true);WiFi.mode(WIFI_OFF); // Radio off during the slow ePaper refresh.
  if(guestMode){
    if(!dataOk){upcomingReminderCount=0;restoreGuestReminders(validLocalMidnight());}
    renderGuestIfChanged(keyAction!=Key0Action::Auto,hadCalendar,dataOk);
  }else if(dataOk){
    struct tm now={};getLocalTime(&now,100);
    now.tm_hour=0;now.tm_min=0;now.tm_sec=0;now.tm_isdst=-1;
    time_t today=mktime(&now),ws=0,we=0;displayWindow(ws,we);
    uint32_t hash=computeRenderHash(ws,today,holidayFresh);
    // Update display only when the estimate changes meaningfully (5% buckets).
    hash^=(uint32_t)(batteryPercent<0?255:batteryPercent/5)*16777619u;
    Serial.printf("[DISPLAY] manual=%d prior=%d hash_old=%08x hash_new=%08x\n",manualWake,hadCalendar,(unsigned)prefs.getUInt("renderHash",0),(unsigned)hash);
    if(keyAction!=Key0Action::Auto || !hadCalendar || prefs.getUInt("renderHash",0)!=hash){
      Serial.println("[DISPLAY] Starting full refresh");
      Serial.flush();
      initDisplay();drawCalendarPage(today,holidayFresh);
      if(digitalRead(EPD_BUSY_PIN)==HIGH){
        prefs.putUInt("renderHash",hash);prefs.putBool("hasCalendar",true);
      }else Serial.println("Panel remains BUSY; render hash not committed, next wake retries.");
      Serial.println("Display update returned. Inspect panel; driver timeout is printed if BUSY fails.");
    }else Serial.println("Identical image: panel initialization/refresh skipped.");
  }else{
    Serial.printf("Sync failed: WiFi=%d clock=%d data=%d; retaining prior calendar=%d\n",wifiOk,clockOk,dataOk,hadCalendar);
    if(!hadCalendar || keyAction==Key0Action::SwitchMode){
      initDisplay();drawStatusPage("Sync failed",!wifiOk?"Wi-Fi 연결에 실패했습니다.":(!clockOk?"시간 동기화에 실패했습니다.":"캘린더 다운로드 또는 해석에 실패했습니다."),"설정과 시리얼 로그 확인 후 KEY0를 누르세요.",C_RED);
    }
  }
  prefs.end();sleepFor24hOrButton();
}
void loop(){}
