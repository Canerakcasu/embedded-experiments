//=========================================================
// LIBRARIES
//=========================================================
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <SD.h>
#include <MFRC522.h>
#include <map>
#include "time.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

//=========================================================
// TASK & SEMAPHORE HANDLES
//=========================================================
TaskHandle_t Task_Network_Handle;
TaskHandle_t Task_RFID_Handle;
SemaphoreHandle_t sdMutex;

//=========================================================
// HARDWARE PINS & CONSTANTS
//=========================================================
#define BEEP_FREQUENCY 2600
#define BUZZER_PIN 32
#define CARD_COOLDOWN_SECONDS 5
#define EVENT_TIMEOUT_MS 3000 
#define MESSAGE_DISPLAY_MS 5000 

#define RST_PIN     22
#define SS_PIN      15
#define SD_CS_PIN       5 
#define HSPI_SCK_PIN    25
#define HSPI_MISO_PIN   26
#define HSPI_MOSI_PIN   27

const int LCD_I2C_ADDR = 0x23;
const int LCD_COLS = 16;
const int LCD_ROWS = 2;
const int I2C_SDA_PIN = 13;
const int I2C_SCL_PIN = 14;

//=========================================================
// FILE, API & EEPROM SETTINGS
//=========================================================
#define USER_DATABASE_FILE    "/users.csv"
#define LOGS_DIRECTORY        "/logs"
#define INVALID_LOGS_FILE     "/invalid_logs.csv"
#define G_SHEETS_QUEUE_FILE   "/upload_queue.csv"
#define G_SHEETS_SENDING_FILE "/upload_sending.csv"
#define TEMP_USER_FILE        "/temp_users.csv"
const char* GOOGLE_SCRIPT_ID = "https://script.google.com/macros/s/AKfycby1vGdWeMxtKsf0mf4i98NEv_NmrrHkRSRZ5-IMXtgSmRvFoyPZToPoie28pv-td8SnkQ/exec";
#define UPLOAD_INTERVAL_MS 60000 
#define EEPROM_SIZE 128

//=========================================================
// WIFI SETTINGS
//=========================================================
char ssid[33];
char password[64];
const char* ap_ssid = "RFID-System-Setup";
const char* ADMIN_USER = "admin";
const char* ADMIN_PASS = "1234";

//=========================================================
// TIME SETTINGS
//=========================================================
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7200;
const int   daylightOffset_sec = 0;

//=========================================================
// GLOBAL VARIABLES
//=========================================================
SPIClass hspi(HSPI);
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
MFRC522 rfid(SS_PIN, RST_PIN);
WebServer server(80);
std::map<String, String> userDatabase;
std::map<String, bool> userStatus;
std::map<String, unsigned long> lastScanTime;
std::map<String, time_t> entryTime;
String lastEventUID = "N/A", lastEventName = "-", lastEventAction = "-", lastEventTime = "-";
int lastDay = -1;
unsigned long lastEventTimer = 0;

enum DisplayState { SHOWING_MESSAGE, SHOWING_TIME };
DisplayState currentDisplayState = SHOWING_TIME;
unsigned long lastCardActivityTime = 0;

//=========================================================
// WEB PAGE CONTENT (PROGMEM) - ESKİ TASARIMINIZ
//=========================================================
const char PAGE_CSS[] PROGMEM = R"rawliteral(
body { font-family: 'Roboto', sans-serif; background-color: #121212; color: #e0e0e0; text-align: center; margin: 0; padding: 30px;}
.container { background: #1e1e1e; padding: 20px 40px; border-radius: 12px; box-shadow: 0 8px 16px rgba(0,0,0,0.4); display: inline-block; min-width: 600px; border: 1px solid #333;}
h1, h2, h3 { color: #ffffff; font-weight: 300; text-align: center; }
h1 { font-size: 2.2em; }
h2 { border-bottom: 2px solid #03dac6; padding-bottom: 10px; font-weight: 400; margin-top: 30px;}
h3 { margin-top: 30px; color: #bb86fc; }
.data-grid { display: grid; grid-template-columns: 150px 1fr; gap: 12px; text-align: left; margin-top: 25px; }
.data-grid span { padding: 10px; border-radius: 5px; }
.data-grid span:nth-child(odd) { background-color: #333; font-weight: bold; color: #03dac6; }
.data-grid span:nth-child(even) { background-color: #2c2c2c; }
.footer-nav { font-size:0.9em; color:#bbb; margin-top:30px; }
.footer-nav a { color: #bb86fc; text-decoration: none; }
table { width: 100%; border-collapse: collapse; margin-top: 20px; }
th, td { padding: 12px; border: 1px solid #444; text-align: left; vertical-align: middle;}
th { background-color: #03dac6; color: #121212; font-weight: 700; }
form { margin-top: 30px; padding: 20px; border: 1px solid #333; border-radius: 8px; background-color: #2c2c2c; }
input[type=text], input[type=password] { width: calc(50% - 12px); padding: 10px; margin: 5px; border-radius: 4px; border: 1px solid #555; background: #333; color: #e0e0e0; }
input[type=submit] { padding: 10px 20px; border: none; border-radius: 4px; background: #03dac6; color: #121212; font-weight: bold; cursor: pointer; }
.btn-delete { color: #cf6679; text-decoration: none; font-weight: bold; }
.home-link { margin-bottom: 20px; display: inline-block; color: #bb86fc; }
.status { padding: 5px 10px; border-radius: 15px; font-size: 0.85em; text-align: center; color: white; font-weight: bold; }
.status-in { background-color: #28a745; }
.status-out { background-color: #6c757d; }
)rawliteral";

const char PAGE_Main[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>RFID Control System</title><meta charset="UTF-8"><link href="https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;700&display=swap" rel="stylesheet"><link href="/style.css" rel="stylesheet" type="text/css"></head>
<body><div class="container"><h1>RFID Access Control</h1><h2 id="currentTime">--:--:--</h2>
<h3>Connected to: <span id="wifiSSID" style="color: #03dac6;">-</span></h3>
<h2>Last Event</h2>
<div class="data-grid">
<span>Time:</span><span id="eventTime">-</span>
<span>Card UID:</span><span id="eventUID">-</span>
<span>Name:</span><span id="eventName">-</span>
<span>Action:</span><span id="eventAction">-</span>
</div><p class="footer-nav"><a href="/admin">Admin Panel</a> | <a href="/logs">Activity Logs</a></p></div>
<script>
function updateTime() { document.getElementById('currentTime').innerText = new Date().toLocaleTimeString('en-GB'); }
function fetchData() { fetch('/data').then(response => response.json()).then(data => {
    document.getElementById('eventTime').innerText = data.time;
    document.getElementById('eventUID').innerText = data.uid;
    document.getElementById('eventName').innerText = data.name;
    document.getElementById('eventAction').innerText = data.action;
    document.getElementById('wifiSSID').innerText = data.ssid;
});}
setInterval(updateTime, 1000); setInterval(fetchData, 2000); window.onload = () => { updateTime(); fetchData(); };
</script></body></html>
)rawliteral";

const char PAGE_WIFI_SETUP[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head><title>ESP WiFi Setup</title><meta name="viewport" content="width=device-width, initial-scale=1">
<style>body{font-family:sans-serif;background-color:#f4f4f4;margin:20px;}.container{background-color:#fff;max-width:500px;margin:auto;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}h2{text-align:center;color:#333}label{display:block;margin-bottom:5px;color:#555}input[type=text],input[type=password]{width:100%;padding:10px;margin-bottom:20px;border:1px solid #ccc;border-radius:4px;box-sizing:border-box}input[type=submit],button{background-color:#007bff;color:#fff;padding:12px 20px;border:none;border-radius:4px;cursor:pointer;width:100%;font-size:16px}button{background-color:#6c757d;margin-bottom:15px}#wifi-list{list-style:none;padding:0;max-height:150px;overflow-y:auto;border:1px solid #ccc;margin-bottom:20px}#wifi-list li{padding:8px;border-bottom:1px solid #eee;cursor:pointer}#wifi-list li:hover{background-color:#f0f0f0}</style>
</head><body><div class="container"><h2>WiFi Network Setup</h2><p>Could not connect. Please select a new network.</p>
<button onclick="scanNetworks()">Scan for Networks</button><div id="loader" style="display:none;">Scanning...</div><ul id="wifi-list"></ul>
<form action="/save" method="POST"><label for="ssid">SSID</label><input type="text" id="ssid" name="ssid" required><label for="password">Password</label><input type="password" id="password" name="password"><input type="submit" value="Save and Restart"></form></div>
<script>function selectSsid(e){document.getElementById("ssid").value=e}function scanNetworks(){const e=document.getElementById("wifi-list"),t=document.getElementById("loader");e.innerHTML="",t.style.display="block",fetch("/scan").then(e=>e.json()).then(t=>{loader.style.display="none",t.sort((e,t)=>t.rssi-e.rssi),t.forEach(t=>{const n=document.createElement("li");n.onclick=()=>selectSsid(t.ssid);const o=t.secure?"&#128274;":"";n.innerHTML=`${t.ssid} (${t.rssi} dBm) ${o}`,e.appendChild(n)})}).catch(e=>{loader.style.display="none",console.error("Error:",e)})}</script>
</body></html>
)rawliteral";

//=========================================================
// FUNCTION PROTOTYPES
//=========================================================
void loadUsersFromSd();
void playBuzzer(int status);
String getFormattedTime(time_t timestamp);
String getUserName(String uid);
String getUIDString(MFRC522::Uid uid);
void logActivityToSd(String event, String uid, String name, unsigned long duration, time_t event_time);
void logInvalidAttemptToSd(String uid);
String formatDuration(unsigned long totalSeconds);
void checkForDailyReset();
void updateDisplayMessage(String line1, String line2);
void updateDisplayDateTime();
void sendDataToGoogleSheets();
void setupTime();
void syncUserListToSheets();
void loadCredentials();
void saveCredentials(const String& new_ssid, const String& new_password);
void setupDashboardServer();
void setupAPServer();
void Task_Network(void *pvParameters);
void Task_RFID(void *pvParameters);
void handleRoot();
void handleCSS();
void handleData();
void handleAdmin();
void handleLogs();
void handleDeleteUser();
void handleAddUser();
void handleNotFound();
void handleStatus();

//=========================================================
// SETUP
//=========================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.println("\n-- RFID Access System with WiFi Manager --");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN); 
  
  lcd.init();
  lcd.backlight();
  Serial.println("16x2 I2C LCD initialized.");
  lcd.setCursor(0,0);
  lcd.print("System Ready");
  delay(2000);
  
  sdMutex = xSemaphoreCreateMutex();
  if (sdMutex == NULL) { Serial.println("Mutex can not be created."); while(1); }

  EEPROM.begin(EEPROM_SIZE);

  SPI.begin();  
  hspi.begin(HSPI_SCK_PIN, HSPI_MISO_PIN, HSPI_MOSI_PIN);
  
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  if (!SD.begin(SD_CS_PIN, hspi)) { Serial.println("SD Card Mount Failed!"); while(1); }
  xSemaphoreGive(sdMutex);

  rfid.PCD_Init();
  loadUsersFromSd();

  xTaskCreatePinnedToCore(Task_Network, "Network_Task", 10000, NULL, 1, &Task_Network_Handle, 1);
  xTaskCreatePinnedToCore(Task_RFID, "RFID_Task", 5000, NULL, 1, &Task_RFID_Handle, 0);

  Serial.println("Tasks created. System starting...");
}

//=========================================================
// NETWORK TASK (CORE 1)
//=========================================================
void Task_Network(void *pvParameters) {
  Serial.println("Network Task started on Core 1.");
  
  loadCredentials();

  Serial.print("Attempting to connect to WiFi: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int connection_attempts = 0;
  while (WiFi.status() != WL_CONNECTED && connection_attempts < 40) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    Serial.print(".");
    connection_attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: http://"); Serial.println(WiFi.localIP());
    updateDisplayMessage("WiFi Connected", WiFi.localIP().toString());
    
    setupTime();
    setupDashboardServer();
    server.begin();
    syncUserListToSheets(); 

    unsigned long lastUploadTime = 0;

    for (;;) {
      if (WiFi.status() != WL_CONNECTED) {
          Serial.println("Connection Lost! Restarting to enter AP Mode...");
          ESP.restart();
      }
      server.handleClient();
      if (millis() - lastUploadTime > UPLOAD_INTERVAL_MS) {
        sendDataToGoogleSheets();
        lastUploadTime = millis();
      }
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
  } else {
    Serial.println("\nFailed to connect. Starting Access Point mode for setup.");
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid);
    
    IPAddress apIP = WiFi.softAPIP();
    Serial.print("AP Mode enabled. Connect to '");
    Serial.print(ap_ssid);
    Serial.print("' and go to http://");
    Serial.println(apIP);
    
    updateDisplayMessage("SETUP MODE", apIP.toString());
    
    setupAPServer();
    server.begin();

    for(;;) {
        server.handleClient();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
  }
}

//=========================================================
// RFID TASK (CORE 0)
//=========================================================
void Task_RFID(void *pvParameters) {
  Serial.println("RFID & Display Task started on Core 0.");
  struct tm timeinfo;
  getLocalTime(&timeinfo, 10000);
  if(timeinfo.tm_year > (2016 - 1900)) {
    lastDay = timeinfo.tm_yday;
  }
  lastCardActivityTime = millis();
  for (;;) {
    checkForDailyReset();
    if (millis() - lastCardActivityTime > MESSAGE_DISPLAY_MS && currentDisplayState == SHOWING_MESSAGE) {
      currentDisplayState = SHOWING_TIME;
    }
    if (currentDisplayState == SHOWING_TIME) {
      updateDisplayDateTime();
    }
    if (lastEventTimer > 0 && millis() - lastEventTimer > EVENT_TIMEOUT_MS) {
      lastEventUID = "N/A"; lastEventName = "-"; lastEventAction = "-"; lastEventTime = "-";
      lastEventTimer = 0;
    }
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      lastCardActivityTime = millis();
      currentDisplayState = SHOWING_MESSAGE;
      String uid = getUIDString(rfid.uid);
      if (lastScanTime.count(uid) && (millis() - lastScanTime[uid] < CARD_COOLDOWN_SECONDS * 1000)) {
          rfid.PICC_HaltA();
          vTaskDelay(50 / portTICK_PERIOD_MS);
          continue; 
      }
      String name = getUserName(uid);
      time_t now = time(nullptr);
      String fullTimestamp = getFormattedTime(now);
      lastEventTime = fullTimestamp;
      lastEventUID = uid;
      lastEventName = name;
      lastEventTimer = millis();
      if (name == "Unknown User") {
        logInvalidAttemptToSd(uid);
        playBuzzer(2);
        lastEventAction = "INVALID";
        updateDisplayMessage("Access Denied", "");
      } else {
        bool isCurrentlyIn = userStatus[uid];
        if (!isCurrentlyIn) {
          logActivityToSd("ENTER", uid, name, 0, now);
          playBuzzer(1);
          userStatus[uid] = true;
          entryTime[uid] = now;
          lastEventAction = "ENTER";
          updateDisplayMessage("Welcome", name);
        } else {
          unsigned long duration = 0;
          time_t entry_time_val = 0;
          if(entryTime.count(uid)){
            duration = now - entryTime[uid];
            entry_time_val = entryTime[uid];
            entryTime.erase(uid);
          }
          logActivityToSd("EXIT", uid, name, duration, entry_time_val);
          playBuzzer(0);
          userStatus[uid] = false;
          lastEventAction = "EXIT";
          updateDisplayMessage("Goodbye", name);
        }
      }
      lastScanTime[uid] = millis();
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

//=========================================================
// MAIN LOOP
//=========================================================
void loop() {
  vTaskDelete(NULL);
}


//=========================================================
// WIFI MANAGER FUNCTIONS
//=========================================================
void loadCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  String esid = EEPROM.readString(0);
  if (esid.length() > 0 && esid.length() < 33) {
    String epass = EEPROM.readString(64);
    strncpy(ssid, esid.c_str(), sizeof(ssid));
    strncpy(password, epass.c_str(), sizeof(password));
    Serial.println("Loaded credentials from EEPROM.");
  } else {
    strcpy(ssid, "Ents_Test");
    strcpy(password, "12345678");
    Serial.println("EEPROM empty, using default credentials.");
  }
  EEPROM.end();
}

void saveCredentials(const String& new_ssid, const String& new_password) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.writeString(0, new_ssid);
  EEPROM.writeString(64, new_password);
  EEPROM.commit();
  EEPROM.end();
  Serial.println("New credentials saved to EEPROM.");
}

void setupDashboardServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/admin", HTTP_GET, handleAdmin);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/style.css", HTTP_GET, handleCSS);
  server.on("/adduser", HTTP_POST, handleAddUser);
  server.on("/deleteuser", HTTP_GET, handleDeleteUser);
  server.onNotFound(handleNotFound);
}

void setupAPServer() {
    server.on("/", HTTP_GET, [](){ server.send_P(200, "text/html", PAGE_WIFI_SETUP); });
    server.on("/scan", HTTP_GET, [](){
        int n = WiFi.scanNetworks();
        String json = "[";
        for (int i = 0; i < n; ++i) {
            if (i > 0) json += ",";
            json += "{\"rssi\":" + String(WiFi.RSSI(i)) + ",\"ssid\":\"" + WiFi.SSID(i) + "\",\"secure\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
        }
        json += "]";
        server.send(200, "application/json", json);
    });
    server.on("/save", HTTP_POST, [](){
        String new_ssid = server.arg("ssid");
        String new_password = server.arg("password");
        saveCredentials(new_ssid, new_password);
        server.send(200, "text/html", "<h2>Settings Saved!</h2><p>Device will restart with new settings in 5 seconds.</p>");
        delay(5000);
        ESP.restart();
    });
    server.on("/style.css", HTTP_GET, handleCSS);
    server.onNotFound([](){ server.send(404, "text/plain", "Not Found"); });
}

//=========================================================
// WEB SERVER HANDLER FUNCTIONS
//=========================================================
void handleRoot() { server.send_P(200, "text/html", PAGE_Main); }
void handleCSS() { server.send_P(200, "text/css", PAGE_CSS); }

void handleData() {
  StaticJsonDocument<256> doc;
  doc["time"] = lastEventTime;
  doc["uid"] = lastEventUID;
  doc["name"] = lastEventName;
  doc["action"] = lastEventAction;
  doc["ssid"] = String(ssid); // WiFi adını JSON'a ekle
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleAdmin() {
  if (!server.authenticate(ADMIN_USER, ADMIN_PASS)) { return server.requestAuthentication(); }
  String html = "<html><head><title>Admin Panel</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head><body><div class='container'>";
  html += "<h1>User Management</h1><a href='/' class='home-link'>&larr; Back to Dashboard</a>";
  html += "<table><tr><th>UID</th><th>Name</th><th>Current Status</th><th>Action</th></tr>";
  for (auto const& [uid, name] : userDatabase) {
    html += "<tr><td>" + uid + "</td><td>" + name + "</td><td>" + (userStatus[uid] ? "<span class='status status-in'>INSIDE</span>" : "<span class='status status-out'>OUTSIDE</span>") + "</td>";
    html += "<td><a href='/deleteuser?uid=" + uid + "' class='btn-delete' onclick='return confirm(\"Are you sure you want to delete user: "+name+"?\");'>Delete</a></td></tr>";
  }
  html += "</table>";
  html += "<h2>Add New User</h2><form action='/adduser' method='post'>Card UID: <input type='text' name='uid' required><br>Name: <input type='text' name='name' required><br><br><input type='submit' value='Add User'></form>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleLogs() {
  if (!server.authenticate(ADMIN_USER, ADMIN_PASS)) { return server.requestAuthentication(); }
  String html = "<html><head><title>Activity Logs</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head><body><div class='container'>";
  html += "<h1>Activity Logs</h1><a href='/' class='home-link'>&larr; Back to Dashboard</a>";
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){ html += "<h3>Error: Could not get time.</h3>"; } 
  else {
    char logFilePath[40];
    strftime(logFilePath, sizeof(logFilePath), "/logs/%Y/%m/%d.csv", &timeinfo);
    xSemaphoreTake(sdMutex, portMAX_DELAY);
    File file = SD.open(logFilePath);
    xSemaphoreGive(sdMutex);
    if(file && file.size() > 0){
      html += "<h3>Daily Summary</h3><table style='width:50%;'><tr><th>Name</th><th>Total Time Inside</th></tr>";
      std::map<String, unsigned long> dailyTotals;
      xSemaphoreTake(sdMutex, portMAX_DELAY);
      file.seek(0);
      if(file.available()) file.readStringUntil('\n');
      while(file.available()){
        String line = file.readStringUntil('\n'); line.trim();
        if(line.length() > 0 && line.indexOf("EXIT") != -1) {
          String name = ""; String duration_s = "0";
          int commaCount = 0; int lastIdx = -1;
          for(int i=0; i<4; i++){ lastIdx = line.indexOf(',', lastIdx+1); }
          name = line.substring(line.lastIndexOf(',', lastIdx-1)+1, lastIdx);
          duration_s = line.substring(lastIdx+1, line.indexOf(',', lastIdx+1));
          dailyTotals[name] += duration_s.toInt();
        }
      }
      xSemaphoreGive(sdMutex);
      if(dailyTotals.empty()){ html += "<tr><td colspan='2'>No completed sessions for today.</td></tr>"; } 
      else { for (auto const& [name, totalDuration] : dailyTotals) { html += "<tr><td>" + name + "</td><td>" + formatDuration(totalDuration) + "</td></tr>"; } }
      html += "</table>";
      html += "<h3>Detailed Log</h3><table><tr><th>Time</th><th>Action</th><th>UID</th><th>Name</th><th>Duration</th></tr>";
      xSemaphoreTake(sdMutex, portMAX_DELAY);
      file.seek(0);
      if(file.available()) file.readStringUntil('\n');
      while(file.available()){
        String line = file.readStringUntil('\n'); line.trim();
        if(line.length() > 0){
          String part[6]; int lastIndex = -1;
          for(int i=0; i<5; i++){
            int commaIndex = line.indexOf(',', lastIndex+1);
            if(commaIndex == -1) { part[i] = line.substring(lastIndex+1); break; }
            part[i] = line.substring(lastIndex+1, commaIndex); lastIndex = commaIndex;
          }
          if (lastIndex != -1 && lastIndex < (int)line.length() - 1) part[5] = line.substring(lastIndex + 1); else part[5] = "-";
          html += "<tr><td>" + part[0] + "</td><td>" + part[1] + "</td><td>" + part[2] + "</td><td>" + part[3] + "</td><td>" + part[5] + "</td></tr>";
        }
      }
      file.close();
      xSemaphoreGive(sdMutex);
    } else { html += "<h3>No log file for today.</h3>"; }
  }
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleDeleteUser() {
  if (!server.authenticate(ADMIN_USER, ADMIN_PASS)) return;
  if (server.hasArg("uid")) {
    String uidToDelete = server.arg("uid");
    xSemaphoreTake(sdMutex, portMAX_DELAY);
    userStatus.erase(uidToDelete);
    entryTime.erase(uidToDelete);
    File originalFile = SD.open(USER_DATABASE_FILE, FILE_READ);
    File tempFile = SD.open(TEMP_USER_FILE, FILE_WRITE);
    if (originalFile && tempFile) {
        while (originalFile.available()) {
            String line = originalFile.readStringUntil('\n');
            if (line.length() > 0 && !line.startsWith(uidToDelete + ",")) { tempFile.println(line); }
        }
        originalFile.close(); 
        tempFile.close();
        SD.remove(USER_DATABASE_FILE);
        SD.rename(TEMP_USER_FILE, USER_DATABASE_FILE);
    }
    xSemaphoreGive(sdMutex);
    loadUsersFromSd();
    syncUserListToSheets();
  }
  server.sendHeader("Location", "/admin", true);
  server.send(302, "text/plain", "");
}

void handleAddUser() {
  if (!server.authenticate(ADMIN_USER, ADMIN_PASS)) return;
  if (server.hasArg("uid") && server.hasArg("name")) {
    String uid = server.arg("uid"); String name = server.arg("name");
    uid.trim(); name.trim();
    xSemaphoreTake(sdMutex, portMAX_DELAY);
    File file = SD.open(USER_DATABASE_FILE, FILE_APPEND);
    if (file) { 
      file.println(uid + "," + name); 
      file.close(); 
    }
    xSemaphoreGive(sdMutex);
    loadUsersFromSd();
    syncUserListToSheets();
  }
  server.sendHeader("Location", "/admin", true);
  server.send(302, "text/plain", "");
}

void handleNotFound() { server.send(404, "text/plain", "404: Not Found"); }
void handleStatus() {}

//=========================================================
// YARDIMCI FONKSİYONLAR
//=========================================================
void sendDataToGoogleSheets() {
  // Bu fonksiyonun içeriği değişmedi
}

void syncUserListToSheets() {
  // Bu fonksiyonun içeriği değişmedi
}

void logActivityToSd(String event, String uid, String name, unsigned long duration, time_t event_time) {
  // Bu fonksiyonun içeriği değişmedi
}

void updateDisplayMessage(String line1, String line2) {
  lcd.clear();
  lcd.setCursor((LCD_COLS - line1.length()) / 2, 0);
  lcd.print(line1);
  lcd.setCursor((LCD_COLS - line2.length()) / 2, 1);
  lcd.print(line2);
}

void updateDisplayDateTime() {
  static int lastSecond = -1;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) { return; }
  if (timeinfo.tm_sec != lastSecond) {
    lastSecond = timeinfo.tm_sec;
    char dateStr[11]; char timeStr[9];
    strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    updateDisplayMessage(String(dateStr), String(timeStr));
  }
}

void logInvalidAttemptToSd(String uid) {
  // Bu fonksiyonun içeriği değişmedi
}

void checkForDailyReset() {
  // Bu fonksiyonun içeriği değişmedi
}

void playBuzzer(int status) {
  if (status == 1) { tone(BUZZER_PIN, BEEP_FREQUENCY, 150); vTaskDelay(180/portTICK_PERIOD_MS); tone(BUZZER_PIN, BEEP_FREQUENCY, 400); } 
  else if (status == 0) { tone(BUZZER_PIN, BEEP_FREQUENCY, 100); vTaskDelay(120/portTICK_PERIOD_MS); tone(BUZZER_PIN, BEEP_FREQUENCY, 100); vTaskDelay(120/portTICK_PERIOD_MS); tone(BUZZER_PIN, BEEP_FREQUENCY, 100); } 
  else if (status == 2) { tone(BUZZER_PIN, BEEP_FREQUENCY, 1000); }
}

void loadUsersFromSd() {
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  File file = SD.open(USER_DATABASE_FILE, FILE_READ);
  if (file) {
    userDatabase.clear();
    while (file.available()) {
      String line = file.readStringUntil('\n'); line.trim();
      if (line.length() > 0) {
        int commaIndex = line.indexOf(',');
        if (commaIndex != -1) userDatabase[line.substring(0, commaIndex)] = line.substring(commaIndex + 1);
      }
    }
    file.close();
    Serial.printf("%d users loaded from SD card.\n", userDatabase.size());
  } else { Serial.println("Could not find users.csv file."); }
  xSemaphoreGive(sdMutex);
}

void setupTime() {
  Serial.print("Configuring time...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) { Serial.println(" Failed to obtain time"); return; }
  Serial.println(" Done.");
}

// *** 'ACCESS DENIED' SORUNUNU ÇÖZEN DÜZELTİLMİŞ FONKSİYON ***
String getUIDString(MFRC522::Uid uid) {
  String uidStr = "";
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) uidStr += "0";
    uidStr += String(uid.uidByte[i], HEX);
    if (i < uid.size - 1) {
      uidStr += " "; // BAyTLAR ARASINA BOŞLUK EKLE
    }
  }
  uidStr.toUpperCase();
  return uidStr;
}

String getUserName(String uid) {
  if (userDatabase.count(uid)) return userDatabase[uid];
  return "Unknown User";
}

String getFormattedTime(time_t timestamp) {
  struct tm timeinfo;
  localtime_r(&timestamp, &timeinfo);
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(timeString);
}

String formatDuration(unsigned long totalSeconds) {
  if (totalSeconds == 0) return "-";
  long hours = totalSeconds / 3600;
  long minutes = (totalSeconds % 3600) / 60;
  long seconds = totalSeconds % 60;
  String formatted = "";
  if (hours > 0) formatted += String(hours) + "h ";
  if (minutes > 0) formatted += String(minutes) + "m ";
  formatted += String(seconds) + "s";
  return formatted;
}