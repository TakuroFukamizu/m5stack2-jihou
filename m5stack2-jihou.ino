#include <Arduino.h>
#include <M5Unified.h>
#include <driver/i2s.h>
#include <WiFi.h>
#include "AudioFileSourceSD.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
// #include "NTPClient.h"
#include "time.h"
#include "config.h"

// Different versions of the framework have different SNTP header file names and availability.
#if __has_include (<esp_sntp.h>)
  #include <esp_sntp.h>
  #define SNTP_ENABLED 1
#elif __has_include (<sntp.h>)
  #include <sntp.h>
  #define SNTP_ENABLED 1
#endif
#ifndef SNTP_ENABLED
  #define SNTP_ENABLED 0
#endif

/*
 * M5Unified
 * I2C BM8563 RTC 
 */

// ---------------------------------

// 設定は config.h に記述する
//     ".example.config.h" をリネームして使用してください

// ---------------------------------

AudioGeneratorMP3 *mp3;
AudioFileSourceSD *file;
AudioOutputI2S *out;
AudioFileSourceID3 *id3;

WiFiUDP udp;
//NTPClient ntp(udp, NTP_SERVER, 32400, 60000);  // udp, ServerName, timeOffset, updateInterval

//RTC_TimeTypeDef RTC_TimeStruct;
//RTC_DateTypeDef RTC_DateStruct;

bool timeFlag = false;

// ---------------------------------

//void update_time();
void play_sound();

// ---------------------------------

void setup() {
    auto cfg = M5.config();
    cfg.internal_spk = true;
    cfg.external_rtc = true;  // default=false. use Unit RTC.
    M5.begin(cfg);
 
//    M5.Axp.SetSpkEnable(true);

    // Setup WiFi
    WiFi.mode(WIFI_STA);
    delay(500);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("Check WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n");

    // ntp.begin();
//    update_time(); // update time by NTP
    configTzTime(NTP_TIMEZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);

#if SNTP_ENABLED
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
      Serial.print('.');
      delay(1000);
    }
#else
    delay(1600);
    struct tm timeInfo;
    while (!getLocalTime(&timeInfo, 1000)) {
      Serial.print('.');
    };
#endif

    Serial.println("\r\n NTP Connected.");
  
    time_t t = time(nullptr)+1; // Advance one second.
    while (t > time(nullptr));  /// Synchronization in seconds
    M5.Rtc.setDateTime( gmtime( &t ) );
  
    M5.Lcd.setTextFont(2);
    M5.Lcd.printf("Sample MP3 playback begins...\n");
    Serial.printf("Sample MP3 playback begins...\n");

    // setup audio
    file = new AudioFileSourceSD(AUDIO_FILE);
    id3 = new AudioFileSourceID3(file);
    out = new AudioOutputI2S(0, 0); // Output to ExternalDAC
    out->SetPinout(12, 0, 2);
    out->SetOutputModeMono(true);
    out->SetGain((float)OUTPUT_GAIN/100.0);
    mp3 = new AudioGeneratorMP3();
    play_sound(); // for test
}


void loop() {
    M5.update();

    if(M5.BtnA.wasPressed()) {
      play_sound(); // for test
    }
    
    static constexpr const char* const wd[7] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};
    auto dt = M5.Rtc.getDateTime();
    Serial.printf("RTC : %04d/%02d/%02d (%s)  %02d:%02d:%02d\r\n"
                 , dt.date.year
                 , dt.date.month
                 , dt.date.date
                 , wd[dt.date.weekDay]
                 , dt.time.hours
                 , dt.time.minutes
                 , dt.time.seconds
                 );
    M5.Display.setCursor(0,0);
    M5.Display.printf("RTC : %04d/%02d/%02d (%s)  %02d:%02d:%02d"
                 , dt.date.year
                 , dt.date.month
                 , dt.date.date
                 , wd[dt.date.weekDay]
                 , dt.time.hours
                 , dt.time.minutes
                 , dt.time.seconds
                 );
    /// ESP32 internal timer
    auto t = time(nullptr);
    auto tm = gmtime(&t);
    //auto tm = localtime(&t); // for local timezone.
  
    Serial.printf("ESP32:%04d/%02d/%02d (%s)  %02d:%02d:%02d\r\n",
          tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
          wd[tm->tm_wday],
          tm->tm_hour, tm->tm_min, tm->tm_sec);
    M5.Display.setCursor(0,20);
    M5.Display.printf("ESP32:%04d/%02d/%02d (%s)  %02d:%02d:%02d",
          tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
          wd[tm->tm_wday],
          tm->tm_hour, tm->tm_min, tm->tm_sec);
        
    // TODO: 時間になったらmp3を再生する
    if (dt.time.minutes == 0 && !timeFlag) {
        // play mp3
        timeFlag = true;
        play_sound();
    } else {
        timeFlag = false;
    }

    // TODO: 定期的に update_time を実行する

    delay(200);
}

// ---------------------------------

void play_sound() {
    Serial.println("playAudio");

    if (!file->isOpen()) {
        file->open(AUDIO_FILE);
    }
    mp3->begin(id3, out);
    Serial.println("end");
}

//void update_time() {
//    // ntp.update();
//
//    // unsigned long epochTime = ntp.getEpochTime();
//    // String formattedTime = ntp.getFormattedTime();  // hh:mm:ss
//
//    // Serial.print(epochTime);
//    // Serial.print("  ");
//    // Serial.println(formattedTime);
//
////    if (WiFi.status() != WL_CONNECTED) {
////        // connect to WiFi
////        Serial.printf("Connecting to %s ", ssid);
////        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
////        while (WiFi.status() != WL_CONNECTED) {
////            delay(500);
////            Serial.print(".");
////        }
////        Serial.println(" CONNECTED");
////    } else {
////        Serial.println(" CONNECTED");
////    }
//
//    // Set ntp time to local
//    configTime(9 * 3600, 0, NTP_SERVER);
//
//    // Get local time
//    struct tm timeInfo;
//    if (getLocalTime(&timeInfo)) {
//        M5.Lcd.print("NTP : ");
//        M5.Lcd.println(NTP_SERVER);
//        // Set RTC time
//        RTC_TimeTypeDef TimeStruct;
//        TimeStruct.Hours   = timeInfo.tm_hour;
//        TimeStruct.Minutes = timeInfo.tm_min;
//        TimeStruct.Seconds = timeInfo.tm_sec;
//        M5.Rtc.SetTime(&TimeStruct);
//        RTC_DateTypeDef DateStruct;
//        DateStruct.WeekDay = timeInfo.tm_wday;
//        DateStruct.Month = timeInfo.tm_mon + 1;
//        DateStruct.Date = timeInfo.tm_mday;
//        DateStruct.Year = timeInfo.tm_year + 1900;
//        M5.Rtc.SetData(&DateStruct);
//    }
//    //disconnect WiFi
//    WiFi.disconnect(true);
//    WiFi.mode(WIFI_OFF);
//}
