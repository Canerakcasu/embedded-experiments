
#include <WiFi.h>
#include "time.h"
#include "esp_sntp.h"

// SSID and password of Wifi connection:
const char* ssid = "WIFIt";
const char* password = "PASSWORD";


const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

const char *time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";  // TimeZone rule for Europe/Rome including daylight adjustment rules (optional)

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("No time available (yet)");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void timeavailable(struct timeval *t) {
  Serial.println("Got time adjustment from NTP!");
  printLocalTime();
}

void setup() {
  Serial.begin(115200);

  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  esp_sntp_servermode_dhcp(1);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" CONNECTED");
  sntp_set_time_sync_notification_cb(timeavailable);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

}

void loop() {
  delay(5000);
  printLocalTime(); 
}
