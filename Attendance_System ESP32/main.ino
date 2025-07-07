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
#include <Update.h>

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
#define USER_SYNC_INTERVAL_MS 60000
#define EEPROM_SIZE 512

#define EEPROM_WIFI_SSID_ADDR           0
#define EEPROM_WIFI_PASS_ADDR          60
#define EEPROM_ADMIN_USER_ADDR         120
#define EEPROM_ADMIN_PASS_ADDR         180
#define EEPROM_ADD_USER_USER_ADDR      240
#define EEPROM_ADD_USER_PASS_ADDR      300

//=========================================================
// WIFI & AUTHENTICATION SETTINGS
//=========================================================
char ssid[33];
char password[64];
const char* ap_ssid = "RFID-System-Setup";
String ADMIN_USER = "admin";
String ADMIN_PASS = "";
String ADD_USER_AUTH_USER = "user";
String ADD_USER_PASS = "";

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
long lastUserFileSize = 0;

enum DisplayState { SHOWING_MESSAGE, SHOWING_TIME };
DisplayState currentDisplayState = SHOWING_TIME;
unsigned long lastCardActivityTime = 0;

//=========================================================
// WEB PAGE CONTENT (PROGMEM)
//=========================================================
const char PAGE_CSS[] PROGMEM = R"rawliteral(
body { font-family: 'Roboto', sans-serif; background-color: #121212; color: #e0e0e0; text-align: center; margin: 0; padding: 30px;}
.container { background: #1e1e1e; padding: 20px 40px; border-radius: 12px; box-shadow: 0 8px 16px rgba(0,0,0,0.4); display: inline-block; min-width: 700px; border: 1px solid #333;}
h1, h2, h3 { color: #ffffff; font-weight: 300; text-align: center; }
h1 { font-size: 2.2em; }
h2 { border-bottom: 2px solid #03dac6; padding-bottom: 10px; font-weight: 400; margin-top: 30px;}
h3 { margin-top: 30px; color: #bb86fc; }
.data-grid { display: grid; grid-template-columns: 150px 1fr; gap: 12px; text-align: left; margin-top: 25px; }
.data-grid span { padding: 10px; border-radius: 5px; }
.data-grid span:nth-child(odd) { background-color: #333; font-weight: bold; color: #03dac6; }
.data-grid span:nth-child(even) { background-color: #2c2c2c; }
.footer-nav { font-size:0.9em; color:#bbb; margin-top:30px; }
.footer-nav a { color: #bb86fc; text-decoration: none; padding: 0 10px;}
table { width: 100%; border-collapse: collapse; margin-top: 20px; }
tr:nth-child(even) { background-color: #2c2c2c; }
th, td { padding: 12px; border-bottom: 1px solid #444; text-align: left; vertical-align: middle;}
th { background-color: #03dac6; color: #121212; font-weight: 700; }
form { margin-top: 20px; padding: 20px; border: 1px solid #333; border-radius: 8px; background-color: #2c2c2c; }
input[type=text], input[type=password] { width: calc(50% - 24px); padding: 10px; margin: 5px; border-radius: 4px; border: 1px solid #555; background: #333; color: #e0e0e0; }
input[type=submit], button { padding: 10px 20px; border: none; border-radius: 4px; background: #03dac6; color: #121212; font-weight: bold; cursor: pointer; margin-top: 10px; transition: all 0.2s ease-in-out; }
input[type=submit]:hover, button:hover { transform: translateY(-3px); box-shadow: 0 4px 12px rgba(0, 0, 0, 0.4); }
button.btn-reboot { background-color: #b00020; color: #fff; }
.btn-delete { color: #cf6679; text-decoration: none; font-weight: bold; }
.home-link { margin-bottom: 20px; display: inline-block; color: #bb86fc; font-size: 1.1em; }
.status { padding: 5px 10px; border-radius: 15px; font-size: 0.85em; text-align: center; color: white; font-weight: bold; }
.status-in { background-color: #28a745; }
.status-out { background-color: #6c757d; }
.uid-reader { background-color: #252525; border: 1px dashed #555; padding: 20px; margin-top: 15px; border-radius: 8px; }
.uid-reader p { margin: 0; font-size: 1.1em; color: #aaa; }
.uid-reader #lastUID { font-family: 'Courier New', monospace; font-size: 1.5em; color: #03dac6; font-weight: bold; margin-top: 10px; min-height: 28px;}
)rawliteral";

const char PAGE_Main[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>RFID Control System</title><meta charset="UTF-8"><link href="https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;700&display=swap" rel="stylesheet"><link href="/style.css" rel="stylesheet" type="text/css"></head>
<body><div class="container"><h1>RFID Access Control</h1><h2 id="currentTime">--:--:--</h2>
<h3>Connected to: <span id="wifiSSID" style="color: #03dac6;">-</span></h3>
<h2>Last Event</h2><div class="data-grid">
<span>Time:</span><span id="eventTime">-</span><span>Card UID:</span><span id="eventUID">-</span>
<span>Name:</span><span id="eventName">-</span><span>Action:</span><span id="eventAction">-</span>
</div><p class="footer-nav"><a href="/admin">Admin Panel</a> | <a href="/logs">Activity Logs</a> | <a href="/adduserpage">Add New User</a></p></div>
<script>
function updateTime() { document.getElementById('currentTime').innerText = new Date().toLocaleTimeString('tr-TR'); }
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
<!DOCTYPE html><html><head><title>RFID WiFi Setup</title><meta charset="UTF-8"><link href="https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;700&display=swap" rel="stylesheet"><link href="/style.css" rel="stylesheet" type="text/css"></head>
<body><div class="container" style="min-width: 500px;">
<h1>WiFi Setup</h1><p style="color:#bbb;">Could not connect. Please select a new network.</p>
<button onclick="scanNetworks()">Scan for Available Networks</button><div id="loader" style="display:none; color:#bbb;">Scanning...</div><table id="wifi-list-table" style="display:none;"></table>
<form action="/save" method="POST">
<h3>Enter Credentials</h3>
SSID:<br><input type='text' name='ssid' id='ssid' required style="width: 100%; box-sizing: border-box;"><br><br>
Password:<br><input type='password' name='pass' style="width: 100%; box-sizing: border-box;"><br><br>
<input type='submit' value='Save and Restart'>
</form>
<p style="font-size:0.8em; color:#777; margin-top:30px; border-top:1px solid #444; padding-top:15px;">
  <b>Note:</b> After saving, the device will restart. You must reconnect your computer to the main network to access the dashboard.
</p>
</div>
<script>
function selectSsid(ssid) { document.getElementById('ssid').value = ssid; }
function scanNetworks() {
  const table = document.getElementById('wifi-list-table');
  const loader = document.getElementById('loader');
  table.style.display = 'none';
  const tbody = table.querySelector('tbody');
  if(tbody) tbody.innerHTML = '';
  loader.style.display = 'block';
  fetch('/scan').then(r => r.json()).then(data => {
    loader.style.display = 'none';
    if(data.length > 0) table.style.display = 'table';
    let newTbody = table.getElementsByTagName('tbody')[0];
    if(!newTbody) { 
        newTbody = document.createElement('tbody'); 
        let head = table.createTHead(); let hRow = head.insertRow(0);
        let th1 = document.createElement('th'); let th2 = document.createElement('th');
        th1.innerHTML = "Network Name (SSID)"; th2.innerHTML = "Signal Strength";
        hRow.appendChild(th1); hRow.appendChild(th2);
        table.appendChild(newTbody); 
    }
    data.sort((a, b) => b.rssi - a.rssi);
    data.forEach(net => {
      let row = newTbody.insertRow(); row.style.cursor = 'pointer';
      row.onclick = () => selectSsid(net.ssid);
      let cell1 = row.insertCell(0); let cell2 = row.insertCell(1);
      cell1.innerHTML = net.ssid + (net.secure ? ' &#128274;' : '');
      cell2.innerText = net.rssi + ' dBm'; cell2.style.textAlign = 'right';
    });
  }).catch(e => { loader.style.display = 'none'; console.error(e); });
}
window.onload = scanNetworks;
</script></body></html>
)rawliteral";

const char PAGE_AddUser[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>Add New User</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head>
<body><div class='container'><h1>Add New User</h1><a href='/' class='home-link'>&larr; Back to Dashboard</a>
<div class="uid-reader"><p>Please scan RFID card/tag now...</p><div id="lastUID">N/A</div></div>
<form action='/adduser' method='post'><h3>User Information</h3>
Card UID: <input type='text' id='uid_field' name='uid' required><br>
Name: <input type='text' name='name' required><br><br>
<input type='submit' value='Add User'></form></div>
<script>
function getLastUID() {
 fetch('/getlastuid').then(response => response.text()).then(uid => {
   if (uid && uid !== "N/A" && uid !== lastEventUID) {
     document.getElementById('lastUID').innerText = uid;
     document.getElementById('uid_field').value = uid;
   }
 });
}
var lastEventUID = document.getElementById('lastUID').innerText;
setInterval(getLastUID, 1000); window.onload = getLastUID;
</script></body></html>
)rawliteral";


//=========================================================
// FUNCTION PROTOTYPES
//=========================================================
void writeStringToEEPROM(int addr, const String& str);
String readStringFromEEPROM(int addr, int maxLen);
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
void handleAddUserPage();
void handleGetLastUID();
void handleReboot();
void handleChangePassword();
void handleChangeAddUserPassword();
void listFiles(String& html, const char* dirname, int levels);
void handleNotFound();


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
  if (sdMutex == NULL) { Serial.println("ERROR: Mutex can not be created."); while(1); }

  EEPROM.begin(EEPROM_SIZE);
  admin_pass = readStringFromEEPROM(EEPROM_PASS_ADDR_ADMIN, 60);
  add_user_pass = readStringFromEEPROM(EEPROM_PASS_ADDR_ADD_USER, 60);

  SPI.begin();  
  hspi.begin(HSPI_SCK_PIN, HSPI_MISO_PIN, HSPI_MOSI_PIN);
  
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  if (!SD.begin(SD_CS_PIN, hspi)) { Serial.println("FATAL: SD Card Mount Failed!"); while(1); }
  File userFile = SD.open(USER_DATABASE_FILE);
  if (userFile) {
    lastUserFileSize = userFile.size();
    userFile.close();
  }
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
  Serial.println("[Network Task] Started on Core 1.");
  
  loadCredentials();

  if (wifi_ssid.length() == 0) {
    Serial.println("[Network Task] No WiFi config found. Starting Access Point mode.");
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid);
    IPAddress apIP = WiFi.softAPIP();
    Serial.print("[Network Task] AP Mode enabled. Connect to '");
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
  } else {
    Serial.print("[Network Task] Attempting to connect to WiFi: ");
    Serial.println(wifi_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
    int connection_attempts = 0;
    while (WiFi.status() != WL_CONNECTED && connection_attempts < 40) {
      vTaskDelay(500 / portTICK_PERIOD_MS);
      Serial.print(".");
      connection_attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n[Network Task] WiFi Connected!");
      Serial.print("[Network Task] IP Address: http://"); Serial.println(WiFi.localIP());
      updateDisplayMessage("WiFi Connected", WiFi.localIP().toString());
      setupTime();
      setupDashboardServer();
      server.begin();
      Serial.println("[Network Task] Dashboard Web Server started.");
      syncUserListToSheets(); 
      unsigned long lastUploadTime = 0;
      unsigned long lastUserFileSyncTime = 0;
      for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[Network Task] Connection Lost! Restarting to enter AP Mode...");
            ESP.restart();
        }
        server.handleClient();
        if (millis() - lastUploadTime > UPLOAD_INTERVAL_MS) {
          sendDataToGoogleSheets();
          lastUploadTime = millis();
        }
        if (millis() - lastUserFileSyncTime > USER_SYNC_INTERVAL_MS) {
          xSemaphoreTake(sdMutex, portMAX_DELAY);
          File userFile = SD.open(USER_DATABASE_FILE, FILE_READ);
          if (userFile) {
            if (userFile.size() != lastUserFileSize) {
              Serial.printf("[Network Task] User file change detected (old: %ld, new: %ld). Syncing...\n", lastUserFileSize, userFile.size());
              lastUserFileSize = userFile.size();
              userFile.close();
              xSemaphoreGive(sdMutex); 
              syncUserListToSheets(); 
            } else {
               userFile.close();
               xSemaphoreGive(sdMutex);
            }
          } else {
             xSemaphoreGive(sdMutex);
          }
          lastUserFileSyncTime = millis();
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
      }
    } else {
      Serial.println("\n[Network Task] Could not connect to saved WiFi. Restarting in AP Mode...");
      delay(3000);
      ESP.restart();
    }
  }
}

//=========================================================
// RFID TASK (CORE 0)
//=========================================================
void Task_RFID(void *pvParameters) {
  Serial.println("[RFID Task] Started on Core 0.");
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
      Serial.println("\n[RFID Task] Card Detected! UID: " + uid);
      String name = getUserName(uid);
      time_t now = time(nullptr);
      String fullTimestamp = getFormattedTime(now);
      lastEventTime = fullTimestamp;
      lastEventUID = uid;
      lastEventName = name;
      lastEventTimer = millis();
      if (name == "Unknown User") {
        Serial.println("[RFID Task] UID not found in database. Access DENIED.");
        logInvalidAttemptToSd(uid);
        playBuzzer(2);
        lastEventAction = "INVALID";
        updateDisplayMessage("Access Denied", "");
      } else {
        Serial.printf("[RFID Task] User identified: %s. Processing...\n", name.c_str());
        if (lastScanTime.count(uid) && (millis() - lastScanTime[uid] < CARD_COOLDOWN_SECONDS * 1000)) {
            Serial.println("[RFID Task] Cooldown active for this card. Ignoring scan.");
        } else {
            bool isCurrentlyIn = userStatus[uid];
            if (!isCurrentlyIn) {
              Serial.println("[RFID Task] Action: ENTER");
              logActivityToSd("ENTER", uid, name, 0, now);
              playBuzzer(1);
              userStatus[uid] = true;
              entryTime[uid] = now;
              lastEventAction = "ENTER";
              updateDisplayMessage("Welcome", name);
            } else {
              Serial.println("[RFID Task] Action: EXIT");
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
            lastScanTime[uid] = millis();
        }
      }
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
// WIFI & WEB SERVER FUNCTIONS
//=========================================================
void loadCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  Serial.println("[Config] Loading configuration from EEPROM...");
  wifi_ssid       = readStringFromEEPROM(EEPROM_WIFI_SSID_ADDR, 32);
  wifi_pass       = readStringFromEEPROM(EEPROM_WIFI_PASS_ADDR, 63);
  admin_user      = readStringFromEEPROM(EEPROM_ADMIN_USER_ADDR, 60);
  add_user_user   = readStringFromEEPROM(EEPROM_ADD_USER_USER_ADDR, 60);

  // Passwords are loaded in setup()
  
  if (admin_user.length() == 0) admin_user = "admin";
  if (add_user_user.length() == 0) add_user_user = "user";
  
  Serial.println("--- Configuration Loaded ---");
  Serial.printf("WiFi SSID: '%s'\n", wifi_ssid.c_str());
  Serial.printf("Admin User: '%s'\n", admin_user.c_str());
  Serial.println("--------------------------");
  EEPROM.end();
}

void setupDashboardServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/admin", HTTP_GET, handleAdmin);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/adduserpage", HTTP_GET, handleAddUserPage);
  server.on("/getlastuid", HTTP_GET, handleGetLastUID);
  server.on("/style.css", HTTP_GET, handleCSS);
  server.on("/adduser", HTTP_POST, handleAddUser);
  server.on("/deleteuser", HTTP_GET, handleDeleteUser);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.on("/changepass", HTTP_POST, handleChangePassword);
  server.on("/changeadduserpass", HTTP_POST, handleChangeAddUserPassword);
  server.onNotFound(handleNotFound);
}

void setupAPServer() {
    server.on("/", HTTP_GET, [](){ server.send_P(200, "text/html", PAGE_WIFI_SETUP); });
    server.on("/save", HTTP_POST, [](){
        wifi_ssid = server.arg("ssid");
        wifi_pass = server.arg("pass");
        Serial.printf("[AP Mode] New WiFi credentials received: SSID = %s\n", wifi_ssid.c_str());
        EEPROM.begin(EEPROM_SIZE);
        writeStringToEEPROM(EEPROM_WIFI_SSID_ADDR, wifi_ssid);
        writeStringToEEPROM(EEPROM_WIFI_PASS_ADDR, wifi_pass);
        EEPROM.end();
        server.send(200, "text/html", "<h2>Settings Saved!</h2><p>Device will restart with new settings in 5 seconds.</p>");
        delay(5000);
        ESP.restart();
    });
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
    server.on("/style.css", HTTP_GET, handleCSS);
    server.onNotFound([](){ server.send(404, "text/plain", "Not Found"); });
}

void handleRoot() { server.send_P(200, "text/html", PAGE_Main); }
void handleCSS() { server.send_P(200, "text/css", PAGE_CSS); }

void handleData() {
  StaticJsonDocument<256> doc;
  doc["time"] = lastEventTime;
  doc["uid"] = lastEventUID;
  doc["name"] = lastEventName;
  doc["action"] = lastEventAction;
  doc["ssid"] = wifi_ssid;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleAddUserPage() {
  if (add_user_pass.length() > 0) {
    if (!server.authenticate(add_user_user.c_str(), add_user_pass.c_str())) { 
      return server.requestAuthentication(); 
    }
  }
  lastEventUID = "N/A";
  server.send_P(200, "text/html", PAGE_AddUser);
}

void handleGetLastUID() {
  server.send(200, "text/plain", lastEventUID);
}

void handleAdmin() {
  if (admin_pass.length() > 0) {
    if (!server.authenticate(admin_user.c_str(), admin_pass.c_str())) { 
      return server.requestAuthentication(); 
    }
  }
 String html = "<html><head><title>Admin Panel</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head><body><div class='container'>";
 html += "<h1>Admin Panel</h1><a href='/' class='home-link'>&larr; Back to Dashboard</a>";
 html += "<h2>Security Settings</h2>";
 html += "<h3>Admin Access</h3>";
 html += "<form action='/changepass' method='post'>Admin Username: <input type='text' name='admin_user' value='" + admin_user + "'><br>New Admin Password: <input type='password' name='admin_pass'><br><small>To remove password, leave blank and save.</small><br><input type='submit' value='Save Admin Settings'></form>";
 html += "<h3>'Add User' Page Access</h3>";
 html += "<form action='/changeadduserpass' method='post'>'Add User' Username: <input type='text' name='add_user_user' value='" + add_user_user + "'><br>New 'Add User' Password: <input type='password' name='add_user_pass'><br><small>To remove password, leave blank and save.</small><br><input type='submit' value='Save User Settings'></form>";
 html += "<h2>User Management</h2>";
 html += "<table><tr><th>UID</th><th>Name</th><th>Current Status</th><th>Action</th></tr>";
 for (auto const& [uid, name] : userDatabase) {
   html += "<tr><td>" + uid + "</td><td>" + name + "</td><td>" + (userStatus[uid] ? "<span class='status status-in'>INSIDE</span>" : "<span class='status status-out'>OUTSIDE</span>") + "</td>";
   html += "<td><a href='/deleteuser?uid=" + uid + "' class='btn-delete' onclick='return confirm(\"Are you sure?\");'>Delete</a></td></tr>";
 }
 html += "</table>";
 html += "<h2>Device Management</h2>";
 html += "<form action='/reboot' method='post' onsubmit='return confirm(\"Reboot the device?\");'><button class='btn-reboot'>Reboot Device</button></form>";
 html += "</div></body></html>";
 server.send(200, "text/html", html);
}

void handleLogs() {
  String html = "<html><head><title>Activity Logs</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head><body><div class='container'>";
  html += "<h1>Activity Logs</h1><a href='/' class='home-link'>&larr; Back to Dashboard</a>";
  html += "<h2>SD Card File Manager</h2>";
  html += "<table><tr><th>File Path</th><th>Size</th></tr>";
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  listFiles(html, "/", 0);
  listFiles(html, LOGS_DIRECTORY, 2);
  xSemaphoreGive(sdMutex);
  html += "</table>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleDeleteUser() {
  if (admin_pass.length() > 0) {
    if (!server.authenticate(admin_user.c_str(), admin_pass.c_str())) return;
  }
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
  }
  server.sendHeader("Location", "/admin", true);
  server.send(302, "text/plain", "");
}

void handleAddUser() {
  if (add_user_pass.length() > 0) {
    if (!server.authenticate(add_user_user.c_str(), add_user_pass.c_str())) return;
  }
  if (server.hasArg("uid") && server.hasArg("name")) {
    String uid = server.arg("uid"); String name = server.arg("name");
    uid.trim(); name.trim();
    if(uid.length() > 0 && name.length() > 0) {
        xSemaphoreTake(sdMutex, portMAX_DELAY);
        File file = SD.open(USER_DATABASE_FILE, FILE_APPEND);
        if (file) { 
            file.println(uid + "," + name); 
            file.close(); 
        }
        xSemaphoreGive(sdMutex);
        loadUsersFromSd();
    }
  }
  server.sendHeader("Location", "/adduserpage", true);
  server.send(302, "text/plain", "");
}

void handleReboot() {
  if (admin_pass.length() > 0) {
    if (!server.authenticate(admin_user.c_str(), admin_pass.c_str())) return;
  }
  server.send(200, "text/plain", "Rebooting in 3 seconds...");
  delay(3000);
  ESP.restart();
}

void handleChangePassword() {
  if (admin_pass.length() > 0) {
    if (!server.authenticate(admin_user.c_str(), admin_pass.c_str())) return;
  }
  if (server.hasArg("admin_user")) {
    admin_user = server.arg("admin_user");
    writeStringToEEPROM(EEPROM_ADMIN_USER_ADDR, admin_user);
  }
  if (server.hasArg("admin_pass")) {
    String newPass = server.arg("admin_pass");
    if (newPass.length() == 0 || newPass.length() > 3) {
      writeStringToEEPROM(EEPROM_ADMIN_PASS_ADDR, newPass);
      admin_pass = newPass;
    }
  }
  server.sendHeader("Location", "/admin", true);
  server.send(302, "text/plain", "");
}

void handleChangeAddUserPassword() {
  if (admin_pass.length() > 0) {
    if (!server.authenticate(admin_user.c_str(), admin_pass.c_str())) return;
  }
  if (server.hasArg("add_user_user")) {
    add_user_user = server.arg("add_user_user");
    writeStringToEEPROM(EEPROM_ADD_USER_USER_ADDR, add_user_user);
  }
  if (server.hasArg("add_user_pass")) {
    String newPass = server.arg("add_user_pass");
    if (newPass.length() == 0 || newPass.length() > 3) {
      writeStringToEEPROM(EEPROM_ADD_USER_PASS_ADDR, newPass);
      add_user_pass = newPass;
    }
  }
  server.sendHeader("Location", "/admin", true);
  server.send(302, "text/plain", "");
}

void listFiles(String& html, const char* dirname, int levels) {
    File root = SD.open(dirname);
    if (!root || !root.isDirectory()) return;
    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            if (levels) listFiles(html, file.path(), levels - 1);
        } else {
            html += "<tr><td>" + String(file.path()) + "</td><td>" + String(file.size()) + " B</td></tr>";
        }
        file = root.openNextFile();
    }
}

void handleNotFound() { server.send(404, "text/plain", "404: Not Found"); }

//=========================================================
// HELPER FUNCTIONS
//=========================================================
void writeStringToEEPROM(int addr, const String& str) {
  EEPROM.begin(EEPROM_SIZE);
  int len = str.length();
  for (int i = 0; i < len; i++) {
    EEPROM.write(addr + i, str[i]);
  }
  EEPROM.write(addr + len, '\0');
  EEPROM.commit();
  EEPROM.end();
}

String readStringFromEEPROM(int addr, int maxLen) {
  EEPROM.begin(EEPROM_SIZE);
  char data[maxLen + 1];
  int len = 0;
  byte c;
  while(len < maxLen) {
    c = EEPROM.read(addr + len);
    if (c == '\0' || c == 0xFF) break;
    data[len] = (char)c;
    len++;
  }
  data[len] = '\0';
  EEPROM.end();
  return String(data);
}

void sendDataToGoogleSheets() {
  Serial.println("\n[GSheet] Checking for data to upload...");
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  if (!SD.exists(G_SHEETS_QUEUE_FILE)) { 
    xSemaphoreGive(sdMutex);
    Serial.println("[GSheet] Queue file not found. Nothing to upload.");
    return;
  }
  File queueFile = SD.open(G_SHEETS_QUEUE_FILE, FILE_READ);
  if (!queueFile || queueFile.size() == 0) {
    if(queueFile) queueFile.close();
    SD.remove(G_SHEETS_QUEUE_FILE);
    xSemaphoreGive(sdMutex);
    Serial.println("[GSheet] Queue file is empty. Nothing to upload.");
    return;
  }
  queueFile.close();
  if (SD.exists(G_SHEETS_SENDING_FILE)) { SD.remove(G_SHEETS_SENDING_FILE); }
  Serial.printf("[GSheet] Renaming '%s' to '%s'\n", G_SHEETS_QUEUE_FILE, G_SHEETS_SENDING_FILE);
  bool renamed = SD.rename(G_SHEETS_QUEUE_FILE, G_SHEETS_SENDING_FILE);
  xSemaphoreGive(sdMutex);
  if (!renamed) { Serial.println("ERROR: Could not rename queue file."); return; }
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  File sendingFile = SD.open(G_SHEETS_SENDING_FILE, FILE_READ);
  xSemaphoreGive(sdMutex);
  String payload = "";
  if (sendingFile) {
    while(sendingFile.available()){ payload += sendingFile.readStringUntil('\n') + "\n"; }
    sendingFile.close();
    payload.trim();
    Serial.printf("[GSheet] Payload of %d bytes read from sending file.\n", payload.length());
  } else { Serial.println("ERROR: Could not read sending file."); return; }
  if(payload.length() > 0) {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();
    if (http.begin(client, GOOGLE_SCRIPT_ID)) {
      http.setTimeout(15000);
      http.addHeader("Content-Type", "text/plain");
      Serial.println("[GSheet] Sending payload to Google Script...");
      int httpCode = http.POST(payload);
      if (httpCode > 0) {
        Serial.printf("[GSheet] Upload finished with HTTP code: %d\n", httpCode);
      } else {
        Serial.printf("[GSheet] Upload failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();
    } else { Serial.println("[GSheet] HTTP client failed to begin connection."); }
  }
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  if(SD.exists(G_SHEETS_SENDING_FILE)) { 
    SD.remove(G_SHEETS_SENDING_FILE); 
    Serial.printf("[GSheet] Temporary file '%s' deleted.\n", G_SHEETS_SENDING_FILE);
  }
  xSemaphoreGive(sdMutex);
}

void syncUserListToSheets() {
  Serial.println("[GSheet] Syncing user list to Google Sheets...");
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  File userFile = SD.open(USER_DATABASE_FILE, FILE_READ);
  if (!userFile) { 
    Serial.println("ERROR: Failed to open users.csv for syncing."); 
    xSemaphoreGive(sdMutex); 
    return; 
  }
  String payload = "USER_LIST_UPDATE\n";
  while(userFile.available()) {
    payload += userFile.readStringUntil('\n') + "\n";
  }
  userFile.close();
  xSemaphoreGive(sdMutex);
  payload.trim();
  if(payload.length() > strlen("USER_LIST_UPDATE\n")) {
    Serial.printf("[GSheet] Sending %d bytes of user data.\n", payload.length());
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();
    if (http.begin(client, GOOGLE_SCRIPT_ID)) {
      http.setTimeout(15000);
      http.addHeader("Content-Type", "text/plain");
      int httpCode = http.POST(payload);
      Serial.printf("[GSheet] User list sync finished with HTTP code: %d\n", httpCode);
      http.end();
    }
  } else {
      Serial.println("[GSheet] No users to sync.");
  }
}

void logActivityToSd(String event, String uid, String name, unsigned long duration, time_t event_time) {
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  String current_timestamp_str = getFormattedTime(time(nullptr));
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char logFilePath[40];
    strftime(logFilePath, sizeof(logFilePath), "/logs/%Y/%m/%d.csv", &timeinfo);
    Serial.printf("[SD] Attempting to log event to: %s\n", logFilePath);
    String dirPath = String(logFilePath).substring(0, String(logFilePath).lastIndexOf('/'));
    if(!SD.exists(dirPath)) {
        String root_path = LOGS_DIRECTORY;
        String year_path = root_path + "/" + String(timeinfo.tm_year + 1900);
        if(!SD.exists(root_path)) SD.mkdir(root_path);
        if(!SD.exists(year_path)) SD.mkdir(year_path);
        if(!SD.exists(dirPath)) SD.mkdir(dirPath);
    }
    String fullLogEntry = current_timestamp_str + "," + event + "," + uid + "," + name + "," + String(duration) + "," + formatDuration(duration);
    File localFile = SD.open(logFilePath, FILE_APPEND);
    if(localFile) {
        if(localFile.size() == 0) { localFile.println("Timestamp,Action,UID,Name,Duration_sec,Duration_Formatted"); }
        localFile.println(fullLogEntry);
        localFile.close();
        Serial.println("[SD] Log entry written successfully.");
    } else {
        Serial.println("ERROR: Could not open log file for appending.");
    }
  }
  File queueFile = SD.open(G_SHEETS_QUEUE_FILE, FILE_APPEND);
  if (queueFile) {
    String dataForSheet = "";
    if (event == "ENTER") { dataForSheet = uid + "," + name + "," + getFormattedTime(event_time) + ","; } 
    else if (event == "EXIT") { dataForSheet = uid + "," + name + ",," + getFormattedTime(time(nullptr)); }
    if (dataForSheet != "") { 
      queueFile.println(dataForSheet);
      Serial.println("[SD] Event added to Google Sheets upload queue.");
    }
    queueFile.close();
  }
  xSemaphoreGive(sdMutex);
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
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  File file = SD.open(INVALID_LOGS_FILE, FILE_APPEND);
  if (file) {
    if (file.size() == 0) { file.println("Timestamp,UID"); }
    file.println(getFormattedTime(time(nullptr)) + "," + uid);
    file.close();
  }
  xSemaphoreGive(sdMutex);
}

void checkForDailyReset() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) return;
  if (lastDay != -1 && lastDay != timeinfo.tm_yday) {
    userStatus.clear();
    entryTime.clear();
    lastDay = timeinfo.tm_yday;
    Serial.println("[System] Daily reset performed for user status.");
  }
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
    Serial.printf("[SD] %d users loaded from SD card.\n", userDatabase.size());
  } else { Serial.println("ERROR: Could not find users.csv file."); }
  xSemaphoreGive(sdMutex);
}

void setupTime() {
  Serial.print("[Time] Configuring time from NTP server...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) { Serial.println(" Failed to obtain time."); return; }
  Serial.println(" Done.");
}

String getUIDString(MFRC522::Uid uid) {
  String uidStr = "";
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) {
      uidStr += "0";
    }
    uidStr += String(uid.uidByte[i], HEX);
    if (i < uid.size - 1) {
      uidStr += " ";
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
