void initDisplay(){
  pinMode(EPD_RES_PIN,OUTPUT);
  pinMode(EPD_DC_PIN,OUTPUT);
  pinMode(EPD_CS_PIN,OUTPUT);
  hspi.begin(EPD_SCK_PIN,-1,EPD_MOSI_PIN,-1);
  display.epd2.selectSPI(hspi,SPISettings(2000000,MSBFIRST,SPI_MODE0));
  display.init(0);
  u8g2.begin(display);
  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
}

void sleepFor24hOrButton(){
  display.hibernate();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  pinMode(SYNC_BUTTON_PIN,INPUT_PULLUP);

  esp_sleep_enable_timer_wakeup(
    (uint64_t)AUTO_SYNC_HOURS*60ULL*60ULL*1000000ULL
  );
  esp_sleep_enable_ext1_wakeup(1ULL << SYNC_BUTTON_PIN,ESP_EXT1_WAKEUP_ANY_LOW);
  rtc_gpio_pullup_en((gpio_num_t)SYNC_BUTTON_PIN);
  rtc_gpio_pulldown_dis((gpio_num_t)SYNC_BUTTON_PIN);
  Serial.println("Entering deep sleep. E1002 KEY0 / GPIO3 wakes for manual sync.");
  delay(100);
  esp_deep_sleep_start();
}

void setup(){
  Serial.begin(115200);
  delay(300);
  pinMode(SYNC_BUTTON_PIN,INPUT_PULLUP);

  bool manualWake=
    esp_sleep_get_wakeup_cause()==ESP_SLEEP_WAKEUP_EXT1;

  Serial.printf(
    "Calendar Display v0.1 (%s wake)\n",
    manualWake?"manual":"timer/power"
  );

  if(!configurationLooksValid()){
    initDisplay();
    drawStatusPage(
      "Calendar Display",
      String("Wi-Fi 또는 iCloud 캘린더 설정이 필요합니다."),
      String("src/config.h 파일을 확인하세요."),
      C_YELLOW
    );
    sleepFor24hOrButton();
    return;
  }

  bool wifiOk=connectWiFi();
  bool clockOk=wifiOk && syncClock();
  bool dataOk=true;

  if(wifiOk && clockOk){
    eventCount=0;
    for(uint8_t i=0;i<sizeof(feeds)/sizeof(feeds[0]);++i){
      if(strlen(feeds[i].url)==0) continue;
      if(!downloadAndParseFeed(i)) dataOk=false;
    }
    sortEvents();
  }else{
    dataOk=false;
  }

  struct tm now={};
  if(!getLocalTime(&now,100)){
    now.tm_year=2026-1900;
    now.tm_mon=9;
    now.tm_mday=1;
  }

  int year=now.tm_year+1900;
  int month1=now.tm_mon+1;
  int today=now.tm_mday;

  initDisplay();

  uint32_t hash=computeRenderHash(year,month1);
  prefs.begin("caldisplay",false);
  uint32_t oldHash=prefs.getUInt("renderHash",0);

  bool firstSuccessfulRender=(oldHash==0);

  if(dataOk){
    bool shouldRender=manualWake || oldHash!=hash || firstSuccessfulRender;
    if(shouldRender){
      drawCalendarPage(year,month1,today,true);
      prefs.putUInt("renderHash",hash);
    }
  }else if(firstSuccessfulRender){
    drawStatusPage(
      "Calendar Display",
      String("첫 동기화에 실패했습니다."),
      String("Wi-Fi / iCloud URL을 확인하고 버튼을 눌러 다시 시도하세요."),
      C_RED
    );
  }else{
    Serial.println("Sync failed. Keeping the last successful ePaper image.");
  }

  prefs.end();
  sleepFor24hOrButton();
}

void loop(){}
