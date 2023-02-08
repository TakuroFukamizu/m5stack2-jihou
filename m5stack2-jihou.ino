#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include <WiFi.h>
#include <time.h>

/// need ESP8266Audio library. ( URL : https://github.com/earlephilhower/ESP8266Audio/ )
#include <AudioOutput.h>
#include <AudioFileSourceSD.h>
#include <AudioFileSourceID3.h>
#include <AudioGeneratorMP3.h>

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

class AudioOutputM5Speaker : public AudioOutput {
  public:
    AudioOutputM5Speaker(m5::Speaker_Class* m5sound, uint8_t virtual_sound_channel = 0)
    {
      _m5sound = m5sound;
      _virtual_ch = virtual_sound_channel;
    }
    virtual ~AudioOutputM5Speaker(void) {};
    virtual bool begin(void) override { return true; }
    virtual bool ConsumeSample(int16_t sample[2]) override
    {
      if (_tri_buffer_index < tri_buf_size)
      {
        _tri_buffer[_tri_index][_tri_buffer_index  ] = sample[0];
        _tri_buffer[_tri_index][_tri_buffer_index+1] = sample[1];
        _tri_buffer_index += 2;

        return true;
      }

      flush();
      return false;
    }
    virtual void flush(void) override
    {
      if (_tri_buffer_index)
      {
        _m5sound->playRaw(_tri_buffer[_tri_index], _tri_buffer_index, hertz, true, 1, _virtual_ch);
        _tri_index = _tri_index < 2 ? _tri_index + 1 : 0;
        _tri_buffer_index = 0;
      }
    }
    virtual bool stop(void) override
    {
      flush();
      _m5sound->stop(_virtual_ch);
      return true;
    }

    const int16_t* getBuffer(void) const { return _tri_buffer[(_tri_index + 2) % 3]; }
  protected:
    m5::Speaker_Class* _m5sound;
    uint8_t _virtual_ch;
    static constexpr size_t tri_buf_size = 1536;
    int16_t _tri_buffer[3][tri_buf_size];
    size_t _tri_buffer_index = 0;
    size_t _tri_index = 0;
};

// ---------------------------------

/// set M5Speaker virtual channel (0-7)
static constexpr uint8_t m5spk_virtual_channel = 0;

static AudioFileSourceSD file;
static AudioOutputM5Speaker out(&M5.Speaker, m5spk_virtual_channel);
static AudioGeneratorMP3 mp3;
static AudioFileSourceID3* id3 = nullptr;


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
  cfg.external_spk = true;    /// use external speaker (SPK HAT / ATOMIC SPK)
  //cfg.external_spk_detail.omit_atomic_spk = true; // exclude ATOMIC SPK
  //cfg.external_spk_detail.omit_spk_hat    = true; // exclude SPK HAT
  cfg.external_rtc = true;  // default=false. use Unit RTC.
  M5.begin(cfg);
 
  { /// custom setting
    auto spk_cfg = M5.Speaker.config();
    /// Increasing the sample_rate will improve the sound quality instead of increasing the CPU load.
    spk_cfg.sample_rate = 96000; // default:64000 (64kHz)  e.g. 48000 , 50000 , 80000 , 96000 , 100000 , 128000 , 144000 , 192000 , 200000
    M5.Speaker.config(spk_cfg);
  }
  M5.Speaker.begin();

  // connect to sd card
  while (false == SD.begin(GPIO_NUM_4, SPI, 25000000)) {
    delay(500);
  }

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

  // setup NTP
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

  // // setup audio
  // file = new AudioFileSourceSD(AUDIO_FILE);
  // id3 = new AudioFileSourceID3(file);
  // out = new AudioOutputI2S(0, 0); // Output to ExternalDAC
  // out->SetPinout(12, 0, 2);
  // out->SetOutputModeMono(true);
  // out->SetGain((float)OUTPUT_GAIN/100.0);
  // mp3 = new AudioGeneratorMP3();
  play_sound(); // for test
}


void loop() {
  if (mp3.isRunning()) {
    if (!mp3.loop()) { mp3.stop(); }
  } else {
    delay(1);
  }

  M5.update();

  if(M5.BtnA.wasClicked()) {
    play_sound(); // for test
  }
  // if (M5.BtnA.wasClicked()) {
  //   M5.Speaker.tone(1000, 100);
  //   stop();
  //   if (++fileindex >= filecount) { fileindex = 0; }
  //   play(filename[fileindex]);
  // }
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
static int header_height = 0;
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
  (void)cbData;
  if (string[0] == 0) { return; }
  if (strcmp(type, "eof") == 0) {
    M5.Display.display();
    return;
  }
  int y = M5.Display.getCursorY();
  if (y+1 >= header_height) { return; }
  M5.Display.fillRect(0, y, M5.Display.width(), 12, M5.Display.getBaseColor());
  M5.Display.printf("%s: %s", type, string);
  M5.Display.setCursor(0, y+12);
}


void play_sound() {
  Serial.println("playAudio");
  if (id3 != nullptr) { stop_sound(); }
  file.open(AUDIO_FILE);
  id3 = new AudioFileSourceID3(&file);
  // id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
  id3->open(AUDIO_FILE);
  mp3.begin(id3, &out);
}

void stop_sound() {
  if (id3 == nullptr) return;
  out.stop();
  mp3.stop();
  id3->RegisterMetadataCB(nullptr, nullptr);
  id3->close();
  file.close();
  delete id3;
  id3 = nullptr;
}
