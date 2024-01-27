// https://akizukidenshi.com/catalog/g/gM-16731/
// https://ja.aliexpress.com/item/1005002885902826.html

#include <M5Stack.h> //0.3.9
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_AHTX0.h> //2.0.3
#include <WiFi.h>
#include <WebServer.h>
#include "auth.h"

#define dateYpos 25
#define dateXpos 34
#define dateTextSize 3
#define dateTextColor YELLOW
#define timeXpos 60
#define timeYpos 80
#define timeTextSize 7 //(最大が7)
#define timeTextColor GREEN
#define tempYpos 160
#define tempXpos 45
#define tempTextColor WHITE
#define defTextSize 3
#define tempTextSize 3
#define secTimeOffset 13   // 環境に応じて設定
#define LogWriteCycle 3600 // sec

Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);
Adafruit_AHTX0 aht;
String sTemp;
String sHum;
int loopCount = 0;
unsigned long g_time;

// 保存するファイル名
const char *fname = "/log.csv";
int batteryLevel;

void wifiConnect()
{
  int tryCount = 0;
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password); // Wi-Fi接続
  while (WiFi.status() != WL_CONNECTED)
  {
    // Wi-Fi 接続待ち
    delay(200);
    Serial.printf(".");
    tryCount += 1;
    if (tryCount > 30)
    {
      Serial.println("\nwifi connect NG");
      return;
    };
  }
  Serial.println("\nwifi connect OK");
}

void writeData(String str)
{
  // SDカードへの書き込み処理（ファイル追加モード）
  // SD.beginはM5.begin内で処理されているので不要
  File file;
  file = SD.open(fname, FILE_APPEND);
  file.print(str);
  file.close();
  Serial.println("SD write!");
}

struct tm timeinfo;
int weekday;
void getTime(bool bWrite)
{
  const long gmtOffset_sec = 3600 * 9 + secTimeOffset; // UTC + 9
  const int daylightOffset_sec = 3600 * 0;             // Summer Timeなし
  const char *ntpServer = "pool.ntp.org";
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  String sweekday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

  // 元の時刻を消去
  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.setTextSize(dateTextSize);
  M5.Lcd.setCursor(dateXpos, dateYpos);
  M5.Lcd.print(&timeinfo, "%Y-%m-%d ");
  M5.Lcd.print(sweekday[weekday]);
  M5.Lcd.setCursor(timeXpos, timeYpos);
  M5.Lcd.setTextSize(timeTextSize);
  M5.Lcd.println(&timeinfo, "%H:%M");

  // 新たな時刻を出力
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  weekday = timeinfo.tm_wday; // 0 1 2 3 4 5 6

  M5.Lcd.setTextColor(dateTextColor);
  M5.Lcd.setTextSize(dateTextSize);
  M5.Lcd.setCursor(dateXpos, dateYpos);
  M5.Lcd.print(&timeinfo, "%Y-%m-%d ");
  M5.Lcd.print(sweekday[weekday]);
  M5.Lcd.setTextColor(timeTextColor);
  M5.Lcd.setCursor(timeXpos, timeYpos);
  M5.Lcd.setTextSize(timeTextSize);
  M5.Lcd.println(&timeinfo, "%H:%M");

  Serial.println(&timeinfo, "%Y-%m-%d %A %H:%M:%S");

  if (bWrite)
  {
    writeData(String(timeinfo.tm_year + 1900) + "-" + String(timeinfo.tm_mon + 1) + "-" + String(timeinfo.tm_mday));
    writeData(" " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec));
  }
  if (loopCount == 0)
  {
    // 秒単位の時刻補正
    int sec = timeinfo.tm_sec;
    delay((60 - sec) * 1000);
  }
  return;
}
void getTemp(bool bWrite)
{
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp); // populate temp and humidity objects with fresh data

  float t = temp.temperature;
  float h = humidity.relative_humidity;
  // LCD print
  // 元の出力を消去
  M5.Lcd.setTextSize(tempTextSize);
  M5.Lcd.setCursor(tempXpos, tempYpos);
  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.println(sTemp);
  M5.Lcd.setCursor(tempXpos, tempYpos + tempTextSize * 10);
  M5.Lcd.print(sHum);
  // 新たな読み値を出力
  sTemp = "Temp:" + String(t) + " C";
  sHum = "Hum :" + String(h) + " RH";
  M5.Lcd.setCursor(tempXpos, tempYpos);
  M5.Lcd.setTextColor(tempTextColor);
  M5.Lcd.println(sTemp);
  M5.Lcd.setCursor(tempXpos, tempYpos + tempTextSize * 10);
  M5.Lcd.print(sHum);

  // Serial print
  Serial.print("Temperature: ");
  Serial.print(temp.temperature);
  Serial.println(" degrees C");
  Serial.print("Humidity: ");
  Serial.print(humidity.relative_humidity);
  Serial.println("% RH");

  if (bWrite)
  {
    writeData(" AHT25 " + sTemp + " " + sHum);
  }
}

void getBattery()
{
  batteryLevel = M5.Power.getBatteryLevel(); // 残量取得
  writeData(" Battery:" + String(batteryLevel) + "\n");
}

void setup()
{
  M5.begin(true, true, true); // LCD SD Serial
  M5.Power.begin();
  M5.Lcd.setRotation(3);
  Serial.begin(115200);
  while (!Serial)
  {
  }
  Serial.println("Serial Open!");

  if (aht.begin())
  {
    Serial.println("Found AHT25");
  }
  else
  {
    Serial.println("Didn't find AHT25");
  }

  wifiConnect();

  if (!SD.begin())
  {
    Serial.println("Error! SD card NG.");
    M5.Lcd.setTextColor(YELLOW);
    M5.Lcd.setTextSize(defTextSize);
    M5.Lcd.println("Error! SD card NG.");
    delay(5000);
    m5.Lcd.clear(BLACK);
  }
  else
  {
    Serial.println("SD card OK.");
  }
}

void loop()
{
  g_time = millis();

  M5.update(); // ボタンを読み取る
  if (M5.BtnB.wasReleased() || M5.BtnB.pressedFor(100, 200))
  {
    m5.Lcd.clear(BLACK);
  }
  if (loopCount % LogWriteCycle == 0)
  {
    getTime(true);
    getTemp(true);
    getBattery();
  }

  loopCount += 1;
  Serial.println(loopCount);
  getTemp(false);
  if (loopCount % 60 == 0)
  {
    getTime(false);
  }

  while (millis() - g_time < 1000)
  {
    // 1秒待機
  }
}
