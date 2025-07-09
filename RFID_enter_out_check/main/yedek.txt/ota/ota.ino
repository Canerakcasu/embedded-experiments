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
#include <UniversalTelegramBot.h>

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
#define RECONNECT_INTERVAL_MS 60000 // Reconnect attempt every 60 seconds

#define RST_PIN         22
#define SS_PIN          15
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
// TELEGRAM & API SETTINGS
//=========================================================
#define BOT_TOKEN "7725746632:AAEGXEQVS1qTTuLZ5eg6YDoJj9nN87tkbgc"
#define CHAT_ID "5721778314"
const char* GOOGLE_SCRIPT_ID = "https://script.google.com/macros/s/AKfycby1vGdWeMxtKsf0mf4i98NEv_NmrrHkRSRZ5-IMXtgSmRvFoyPZToPoie28pv-td8SnkQ/exec";

//=========================================================
// FILE, API & EEPROM SETTINGS
//=========================================================
#define USER_DATABASE_FILE      "/users.csv"
#define LOGS_DIRECTORY          "/logs"
#define INVALID_LOGS_FILE       "/invalid_logs.csv"
#define G_SHEETS_QUEUE_FILE     "/upload_queue.csv"
#define G_SHEETS_SENDING_FILE   "/upload_sending.csv"
#define TEMP_USER_FILE          "/temp_users.csv"
#define UPLOAD_INTERVAL_MS 60000
#define USER_SYNC_INTERVAL_MS 3600000 // Sync users every hour
#define EEPROM_SIZE 512

#define EEPROM_WIFI_SSID_ADDR       0
#define EEPROM_WIFI_PASS_ADDR       60
#define EEPROM_ADMIN_USER_ADDR      120
#define EEPROM_ADMIN_PASS_ADDR      180
#define EEPROM_ADD_USER_USER_ADDR   240
#define EEPROM_ADD_USER_PASS_ADDR   300

//=========================================================
// WIFI & AUTHENTICATION SETTINGS
//=========================================================
String wifi_ssid;
String wifi_pass;
String admin_user;
String admin_pass;
String add_user_user;
String add_user_pass;

const char* ap_ssid = "RFID-Config-Portal";

//=========================================================
// TIME SETTINGS
//=========================================================
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7200; // Poland Time (GMT+2)
const int   daylightOffset_sec = 0;

//=========================================================
// GLOBAL VARIABLES
//=========================================================
SPIClass hspi(HSPI);
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
MFRC522 rfid(SS_PIN, RST_PIN);
WebServer server(80);

String cachedScanResults = "[]";
unsigned long lastWifiScanTime = 0;
const unsigned long WIFI_SCAN_CACHE_DURATION_MS = 30000;

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
</div><p class="footer-nav"><a href="/admin">Admin Panel</a> | <a href="/activity">Activity Logs</a> | <a href="/adduserpage">Add New User</a></p></div>
<script>
function updateTime() { document.getElementById('currentTime').innerText = new Date().toLocaleTimeString('pl-PL'); }
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
<h1>WiFi Setup</h1><p style="color:#bbb;">Please configure the WiFi connection.</p>
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
      cell1.innerHTML = net.ssid + (net.secure === "true" ? ' &#128274;' : '');
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
let lastKnownUID = "N/A";
function getLastUID() {
 fetch('/getlastuid').then(response => response.text()).then(uid => {
   if (uid && uid !== "N/A" && uid !== lastKnownUID) {
     lastKnownUID = uid;
     document.getElementById('lastUID').innerText = uid;
     document.getElementById('uid_field').value = uid;
   }
 });
}
setInterval(getLastUID, 1000); window.onload = getLastUID;
</script></body></html>
)rawliteral";

const char PAGE_OTA_UPDATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>OTA Update</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head>
<body><div class='container'><h1>Firmware Update</h1><a href='/admin' class='home-link'>&larr; Back to Admin Panel</a>
<form method='POST' action='/update' enctype='multipart/form-data' id='upload_form'>
  <input type='file' name='update' id='file_input' accept='.bin'>
  <input type='submit' value='Update Firmware'>
</form>
<div id='prg_wrap' style='border: 1px solid #03dac6; padding: 5px; margin-top: 20px; display: none;'>
  <div id='prg' style='background-color: #03dac6; width: 0%; height: 20px;'></div>
</div>
<p id='prg_msg' style='margin-top: 10px; color: #03dac6;'></p>
<script>
  var form = document.getElementById('upload_form');
  var fileInput = document.getElementById('file_input');
  var prgWrap = document.getElementById('prg_wrap');
  var prg = document.getElementById('prg');
  var prgMsg = document.getElementById('prg_msg');

  form.addEventListener('submit', function(e) {
    e.preventDefault();
    if (!fileInput.files.length) {
      prgMsg.innerHTML = 'Please select a firmware file (.bin) to upload.';
      return;
    }
    prgWrap.style.display = 'block';
    prg.style.width = '0%';
    prgMsg.innerHTML = 'Uploading...';

    var file = fileInput.files[0];
    var formData = new FormData();
    formData.append('update', file);

    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/update', true);

    xhr.upload.addEventListener('progress', function(e) {
      if (e.lengthComputable) {
        var percent = Math.round((e.loaded / e.total) * 100);
        prg.style.width = percent + '%';
        prgMsg.innerHTML = 'Uploading: ' + percent + '%';
      }
    });

    xhr.onreadystatechange = function() {
      if (xhr.readyState === 4) {
        if (xhr.status === 200) {
          prg.style.width = '100%';
          prgMsg.innerHTML = 'Update successful! Device rebooting...';
          setTimeout(function() { window.location.href = '/'; }, 5000);
        } else {
          prg.style.width = '0%';
          prgMsg.innerHTML = 'Update failed! Error: ' + xhr.responseText;
        }
      }
    };
    xhr.send(formData);
  });
</script></body></html>
)rawliteral";

const char PAGE_WIFI_CONFIG[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>WiFi Configuration</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head>
<body><div class='container'><h1>WiFi Configuration</h1><a href='/admin' class='home-link'>&larr; Back to Admin Panel</a>
<form action='/savewificonfig' method='POST'>
<h3>Enter New WiFi Credentials</h3>
SSID:<br><input type='text' name='ssid' required style='width: 100%; box-sizing: border-box;'><br><br>
Password:<br><input type='password' name='pass' style='width: 100%; box-sizing: border-box;'><br><br>
<input type='submit' value='Save and Restart'>
</form>
<p style='font-size:0.8em; color:#777; margin-top:30px; border-top:1px solid #444; padding-top:15px;'>
  <b>Note:</b> After saving, the device will restart and attempt to connect to the new network.
</p>
</div></body></html>
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
void handleData();
void handleAdmin();
void handleDeleteUser();
void handleAddUser();
void handleAddUserPage();
void handleGetLastUID();
void handleReboot();
void handleChangePassword();
void handleChangeAddUserPassword();
void handleNotFound();
void handleFileManager();
void handleActivityLogs();
void handleDownload();
void startAPMode();
void listDownloadableFiles(File dir, String currentPath);
void sendTelegramNotification(String uid, String name, String action);
void sendSystemAlertToTelegram(String message);
void handleUpdatePage();
void handleWifiConfigPage();
void handleSaveWifiConfig();


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
  lcd.print("System Loading..");
  delay(100);

  sdMutex = xSemaphoreCreateMutex();
  if (sdMutex == NULL) { Serial.println("ERROR: Mutex can not be created."); while(1); }

  SPI.begin();
  hspi.begin(HSPI_SCK_PIN, HSPI_MISO_PIN, HSPI_MOSI_PIN);

  xSemaphoreTake(sdMutex, portMAX_DELAY);
  if (!SD.begin(SD_CS_PIN, hspi)) { Serial.println("FATAL: SD Card Mount Failed!"); lcd.clear(); lcd.print("SD Card Error!"); while(1); }
  xSemaphoreGive(sdMutex);

  rfid.PCD_Init();

  loadCredentials();
  loadUsersFromSd();

  xTaskCreatePinnedToCore(Task_Network, "Network_Task", 10000, NULL, 1, &Task_Network_Handle, 1);
  xTaskCreatePinnedToCore(Task_RFID, "RFID_Task", 5000, NULL, 1, &Task_RFID_Handle, 0);

  Serial.println("Tasks created. System starting...");
}


//=========================================================
// NETWORK TASK (CORE 1) - Rewritten for stability
//=========================================================
void Task_Network(void *pvParameters) {
  Serial.println("[Network Task] Started on Core 1.");
  bool sta_mode_initialized = false;
  unsigned long lastUploadTime = 0;
  unsigned long lastUserFileSyncTime = 0;
  unsigned long lastReconnectAttempt = 0;

  // Initial Check: If no SSID, go straight to AP mode
  if (wifi_ssid.length() == 0) {
    startAPMode();
  } else {
    // Attempt to connect to saved WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
    Serial.print("[Network Task] Trying to connect to ");
    Serial.println(wifi_ssid);
    updateDisplayMessage("Connecting to", wifi_ssid);
  }

  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!sta_mode_initialized) {
        // --- This block runs only once upon successful connection ---
        Serial.println("\n[Network Task] WiFi Connected!");
        Serial.print("[Network Task] IP Address: http://"); Serial.println(WiFi.localIP());
        updateDisplayMessage("WiFi Connected", WiFi.localIP().toString());

        String alertMessage = "✅ *System Connected to WiFi* ✅\n\n";
        alertMessage += "*SSID:* " + wifi_ssid + "\n";
        alertMessage += "*IP Address:* `" + WiFi.localIP().toString() + "`";
        sendSystemAlertToTelegram(alertMessage);

        setupTime();
        setupDashboardServer();
        server.begin();
        Serial.println("[Network Task] Web Server started with Dashboard pages.");
        syncUserListToSheets(); // Initial sync on connect
        sta_mode_initialized = true;
      }

      // --- Regular operations in connected (STA) mode ---
      server.handleClient();

      if (millis() - lastUploadTime > UPLOAD_INTERVAL_MS) {
        sendDataToGoogleSheets();
        lastUploadTime = millis();
      }
      if (millis() - lastUserFileSyncTime > USER_SYNC_INTERVAL_MS) {
        syncUserListToSheets();
        lastUserFileSyncTime = millis();
      }
    } else {
      // --- Handle disconnection or connection failure ---
      if (sta_mode_initialized) { // If it was previously connected
        Serial.println("\n[Network Task] Connection Lost!");
        sendSystemAlertToTelegram("❌ *System WiFi Connection Lost!* ❌\n\n_Attempting to reconnect..._");
        sta_mode_initialized = false; // Reset state
        lastReconnectAttempt = 0;
      }
      
      // Try to reconnect every `RECONNECT_INTERVAL_MS`
      if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL_MS) {
        Serial.println("[Network Task] Retrying WiFi connection...");
        updateDisplayMessage("Reconnecting...", wifi_ssid);
        WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
        lastReconnectAttempt = millis();

        // Give it 10 seconds to connect
        int wait_count = 0;
        while(WiFi.status() != WL_CONNECTED && wait_count < 20) {
            vTaskDelay(500 / portTICK_PERIOD_MS);
            Serial.print(".");
            wait_count++;
        }

        if(WiFi.status() != WL_CONNECTED) {
            Serial.println("\n[Network Task] Reconnect failed. Starting AP mode.");
            sendSystemAlertToTelegram("❌ *Reconnect Failed!* ❌\n\n_Starting Access Point for manual configuration._");
            startAPMode();
        }
      }
    }
    // Yield to other tasks
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

//=========================================================
// RFID TASK (CORE 0) - No major changes needed, already efficient
//=========================================================
void Task_RFID(void *pvParameters) {
  Serial.println("[RFID Task] Started on Core 0.");
  struct tm timeinfo;
  getLocalTime(&timeinfo, 10000); // Wait up to 10s for time sync
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
      lastEventUID = "N/A";
      lastEventName = "-";
      lastEventAction = "-";
      lastEventTime = "-";
      lastEventTimer = 0;
    }

    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      lastCardActivityTime = millis();
      currentDisplayState = SHOWING_MESSAGE;
      
      String uid = getUIDString(rfid.uid);
      Serial.println("\n[RFID Task] Card Detected! UID: " + uid);
      
      String name = getUserName(uid);
      time_t now = time(nullptr);
      
      lastEventTime = getFormattedTime(now);
      lastEventUID = uid;
      lastEventName = name;
      lastEventTimer = millis();
      
      if (name == "Unknown User") {
        Serial.println("[RFID Task] UID not found in database. Access DENIED.");
        logInvalidAttemptToSd(uid);
        playBuzzer(2); // Denied sound
        lastEventAction = "INVALID";
        updateDisplayMessage("Access Denied", "");
        sendTelegramNotification(uid, "Unknown User", "DENIED");

      } else {
        Serial.printf("[RFID Task] User identified: %s. Processing...\n", name.c_str());
        
        if (lastScanTime.count(uid) && (millis() - lastScanTime[uid] < CARD_COOLDOWN_SECONDS * 1000)) {
            Serial.println("[RFID Task] Cooldown active for this card. Ignoring scan.");
            updateDisplayMessage("Please Wait", name);
        } else {
            bool isCurrentlyIn = userStatus.count(uid) ? userStatus[uid] : false;

            if (!isCurrentlyIn) {
              Serial.println("[RFID Task] Action: ENTER");
              logActivityToSd("ENTER", uid, name, 0, now);
              playBuzzer(1); // Enter sound
              userStatus[uid] = true;
              entryTime[uid] = now;
              lastEventAction = "ENTER";
              updateDisplayMessage("Welcome", name);
              sendTelegramNotification(uid, name, "ENTER");

            } else {
              Serial.println("[RFID Task] Action: EXIT");
              unsigned long duration = entryTime.count(uid) ? (now - entryTime[uid]) : 0;
              time_t entry_time_val = entryTime.count(uid) ? entryTime[uid] : now;
              
              logActivityToSd("EXIT", uid, name, duration, entry_time_val);
              playBuzzer(0); // Exit sound
              userStatus[uid] = false;
              if (entryTime.count(uid)) entryTime.erase(uid);
              lastEventAction = "EXIT";
              updateDisplayMessage("Goodbye", name);
              sendTelegramNotification(uid, name, "EXIT");
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
  vTaskDelete(NULL); // FreeRTOS handles everything, main loop is not needed.
}


//=========================================================
// WIFI & WEB SERVER FUNCTIONS
//=========================================================
void loadCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  Serial.println("[Config] Loading all settings from EEPROM...");
  wifi_ssid       = readStringFromEEPROM(EEPROM_WIFI_SSID_ADDR, 32);
  wifi_pass       = readStringFromEEPROM(EEPROM_WIFI_PASS_ADDR, 63);
  admin_user      = readStringFromEEPROM(EEPROM_ADMIN_USER_ADDR, 60);
  admin_pass      = readStringFromEEPROM(EEPROM_ADMIN_PASS_ADDR, 60);
  add_user_user   = readStringFromEEPROM(EEPROM_ADD_USER_USER_ADDR, 60);
  add_user_pass   = readStringFromEEPROM(EEPROM_ADD_USER_PASS_ADDR, 60);

  if (admin_user.length() == 0) admin_user = "admin";
  if (add_user_user.length() == 0) add_user_user = "user";
  
  Serial.println("--- Configuration Loaded ---");
  Serial.printf("WiFi SSID:  '%s'\n", wifi_ssid.c_str());
  Serial.printf("Admin User: '%s'\n", admin_user.c_str());
  Serial.println("--------------------------");
  EEPROM.end();
}

void setupDashboardServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/admin", HTTP_GET, handleAdmin);
  server.on("/adduserpage", HTTP_GET, handleAddUserPage);
  server.on("/getlastuid", HTTP_GET, handleGetLastUID);
  server.on("/style.css", HTTP_GET, [](){ server.send_P(200, "text/css", PAGE_CSS); });
  server.on("/adduser", HTTP_POST, handleAddUser);
  server.on("/deleteuser", HTTP_GET, handleDeleteUser);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.on("/changepass", HTTP_POST, handleChangePassword);
  server.on("/changeadduserpass", HTTP_POST, handleChangeAddUserPassword);
  server.on("/filemanager", HTTP_GET, handleFileManager); // Corrected sServer typo here
  server.on("/activity", HTTP_GET, handleActivityLogs);
  server.on("/download", HTTP_GET, handleDownload);

  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { Update.printError(Serial); }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) { Update.printError(Serial); }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { Serial.printf("Update Success: %u bytes\n", upload.totalSize); }
      else { Update.printError(Serial); }
    }
  });

  server.on("/wificonfig", HTTP_GET, handleWifiConfigPage);
  server.on("/savewificonfig", HTTP_POST, handleSaveWifiConfig);
  
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
        EEPROM.commit();
        EEPROM.end();

        server.send(200, "text/html", "<h2>Settings Saved!</h2><p>Device will restart with new settings in 5 seconds.</p>");
        delay(5000);
        ESP.restart();
    });
    server.on("/scan", HTTP_GET, [](){
        if (millis() - lastWifiScanTime > WIFI_SCAN_CACHE_DURATION_MS) {
            Serial.println("[AP Scan] Performing new WiFi scan...");
            int n = WiFi.scanNetworks();
            String json = "[";
            for (int i = 0; i < n; ++i) {
                if (i > 0) json += ",";
                json += R"({"rssi":)" + String(WiFi.RSSI(i)) + R"(,"ssid":")" + WiFi.SSID(i) + R"(","secure":")" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN) + R"("})";
            }
            json += "]";
            cachedScanResults = json;
            lastWifiScanTime = millis();
        } else {
            Serial.println("[AP Scan] Serving cached WiFi scan results.");
        }
        server.send(200, "application/json", cachedScanResults);
    });
    server.on("/style.css", HTTP_GET, [](){ server.send_P(200, "text/css", PAGE_CSS); });
}

void handleRoot() { server.send_P(200, "text/html", PAGE_Main); }

void handleData() {
  StaticJsonDocument<256> doc;
  doc["time"] = lastEventTime;
  doc["uid"] = lastEventUID;
  doc["name"] = lastEventName;
  doc["action"] = lastEventAction;
  doc["ssid"] = (WiFi.status() == WL_CONNECTED) ? wifi_ssid : "DISCONNECTED";

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
     if (!server.authenticate(admin_user.c_str(), admin_pass.c_str())) { return server.requestAuthentication(); }
  }

  // OPTIMIZED: Send response in chunks to avoid large String allocation
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  String head;
  head.reserve(1024);
  head = "<html><head><title>Admin Panel</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head><body><div class='container'>";
  head += "<h1>Admin Panel</h1><a href='/' class='home-link'>&larr; Back to Dashboard</a>";
  head += "<h2>Security Settings</h2>";
  head += "<h3>Admin Access</h3>";
  head += "<form action='/changepass' method='post'>Admin Username: <input type='text' name='admin_user' value='" + admin_user + "'><br>New Admin Password: <input type='password' name='admin_pass'><br><small>To remove password, leave blank and save.</small><br><input type='submit' value='Save Admin Settings'></form>";
  head += "<h3>'Add User' Page Access</h3>";
  head += "<form action='/changeadduserpass' method='post'>'Add User' Username: <input type='text' name='add_user_user' value='" + add_user_user + "'><br>New 'Add User' Password: <input type='password' name='add_user_pass'><br><small>To remove password, leave blank and save.</small><br><input type='submit' value='Save User Settings'></form>";
  server.send(200, "text/html", head);

  String table_header;
  table_header.reserve(256);
  table_header += "<h2>User Management</h2>";
  table_header += "<table><tr><th>UID</th><th>Name</th><th>Current Status</th><th>Action</th></tr>";
  server.sendContent(table_header);

  for (auto const& [uid, name] : userDatabase) {
    String row;
    row.reserve(256);
    row = "<tr><td>" + uid + "</td><td>" + name + "</td><td>" + (userStatus.count(uid) && userStatus[uid] ? "<span class='status status-in'>INSIDE</span>" : "<span class='status status-out'>OUTSIDE</span>") + "</td>";
    row += "<td><a href='/deleteuser?uid=" + uid + "' class='btn-delete' onclick='return confirm(\"Are you sure?\");'>Delete</a></td></tr>";
    server.sendContent(row);
  }

  String footer;
  footer.reserve(1024);
  footer += "</table>";
  footer += "<h2>Device Management</h2>";
  footer += "<p style='margin-top:15px;'><a href='/filemanager' style='background-color:#03dac6; color:#121212; padding: 10px 20px; text-decoration: none; border-radius: 4px; font-weight: bold;'>File Manager</a></p>";
  footer += "<p style='margin-top:15px;'><a href='/wificonfig' style='background-color:#03dac6; color:#121212; padding: 10px 20px; text-decoration: none; border-radius: 4px; font-weight: bold;'>WiFi Configuration</a></p>";
  footer += "<p style='margin-top:15px;'><a href='/update' style='background-color:#03dac6; color:#121212; padding: 10px 20px; text-decoration: none; border-radius: 4px; font-weight: bold;'>Firmware Update (OTA)</a></p>";
  footer += "<form action='/reboot' method='post' onsubmit='return confirm(\"Reboot the device?\");' style='margin-top:40px;'><button class='btn-reboot'>Reboot Device</button></form>";
  footer += "</div></body></html>";
  server.sendContent(footer);
  server.sendContent(""); // Finalize the response
}

void listDownloadableFiles(File dir, String currentPath) {
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;

        String entryName = entry.name();
        String fullPath = currentPath + entryName;

        if (entry.isDirectory()) {
            listDownloadableFiles(entry, fullPath + "/");
        } else {
            String lowerCasePath = fullPath;
            lowerCasePath.toLowerCase();
            if (lowerCasePath.endsWith(".csv") || lowerCasePath.endsWith(".txt")) {
                String row;
                row.reserve(256);
                row = "<tr><td>" + fullPath + "</td>";
                row += "<td>" + String(entry.size()) + "</td>";
                row += "<td><a href='/download?file=" + fullPath + "' style='color: #03dac6; font-weight:bold;'>Download</a></td></tr>";
                server.sendContent(row); // Send row by row
            }
        }
        entry.close();
    }
}

void handleFileManager() {
  if (admin_pass.length() > 0) {
    if (!server.authenticate(admin_user.c_str(), admin_pass.c_str())) { return server.requestAuthentication(); }
  }
  // OPTIMIZED: Stream response
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  String head;
  head.reserve(512);
  head = "<html><head><title>File Manager</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head><body><div class='container'>";
  head += "<h1>File Manager (csv/txt)</h1><a href='/admin' class='home-link'>&larr; Back to Admin Panel</a>";
  head += "<h2>Downloadable Files</h2>";
  head += "<table><tr><th>File Path</th><th>Size (Bytes)</th><th>Action</th></tr>";
  server.send(200, "text/html", head);
  
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  File root = SD.open("/");
  if(root){
    listDownloadableFiles(root, "/");
    root.close();
  }
  xSemaphoreGive(sdMutex);
  
  server.sendContent("</table></div></body></html>");
  server.sendContent("");
}


void handleActivityLogs() {
  if (admin_pass.length() > 0) {
    if (!server.authenticate(admin_user.c_str(), admin_pass.c_str())) { return server.requestAuthentication(); }
  }

  // OPTIMIZED: Stream response
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  String head;
  head.reserve(512);
  head = "<html><head><title>Today's Activity</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head><body><div class='container'>";
  head += "<h1>Today's Activity Log</h1><a href='/' class='home-link'>&larr; Back to Dashboard</a>";
  server.send(200, "text/html", head);

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 2000)) {
    server.sendContent("<h2>Error: System time not set. Cannot retrieve logs.</h2>");
  } else {
    char logFilePath[40];
    strftime(logFilePath, sizeof(logFilePath), "/logs/%Y-%m-%d.csv", &timeinfo);

    xSemaphoreTake(sdMutex, portMAX_DELAY);
    File logFile = SD.open(logFilePath, FILE_READ);
    xSemaphoreGive(sdMutex);

    if (logFile && logFile.size() > 0) {
        server.sendContent("<h2>Log for: " + String(logFilePath) + "</h2><table>");
        bool isHeader = true;
        while(logFile.available()) {
            String line = logFile.readStringUntil('\n');
            line.trim();
            if (line.length() == 0) continue;

            String row_html;
            row_html.reserve(256);
            row_html += "<tr>";
            int lastIndex = 0;
            int commaIndex = line.indexOf(',');
            while(commaIndex != -1) {
                row_html += (isHeader ? "<th>" : "<td>") + line.substring(lastIndex, commaIndex) + (isHeader ? "</th>" : "</td>");
                lastIndex = commaIndex + 1;
                commaIndex = line.indexOf(',', lastIndex);
            }
            row_html += (isHeader ? "<th>" : "<td>") + line.substring(lastIndex) + (isHeader ? "</th>" : "</td>");
            row_html += "</tr>";
            server.sendContent(row_html);
            isHeader = false;
        }
        server.sendContent("</table>");
        logFile.close();
    } else {
        server.sendContent("<h2>No activity has been recorded today.</h2>");
    }
  }
  server.sendContent("</div></body></html>");
  server.sendContent("");
}


void handleDownload() {
  if (admin_pass.length() > 0) {
    if (!server.authenticate(admin_user.c_str(), admin_pass.c_str())) { return; }
  }
  if (server.hasArg("file")) {
    String filePath = server.arg("file");
    if (filePath.indexOf("..") != -1) {
        server.send(400, "text/plain", "Bad Request: Invalid file path.");
        return;
    }

    xSemaphoreTake(sdMutex, portMAX_DELAY);
    File file = SD.open(filePath, FILE_READ);
    xSemaphoreGive(sdMutex);

    if (file) {
        String fileName = filePath.substring(filePath.lastIndexOf('/') + 1);
        server.sendHeader("Content-Disposition", "attachment; filename=" + fileName);
        server.streamFile(file, "application/octet-stream");
        file.close();
    } else {
        server.send(404, "text/plain", "File Not Found");
    }
  } else {
    server.send(400, "text/plain", "Bad Request: No file specified.");
  }
}

void handleDeleteUser() {
  if (admin_pass.length() > 0) {
    if (!server.authenticate(admin_user.c_str(), admin_pass.c_str())) return;
  }
  if (server.hasArg("uid")) {
    String uidToDelete = server.arg("uid");
    String nameOfDeletedUser = getUserName(uidToDelete);

    xSemaphoreTake(sdMutex, portMAX_DELAY);
    userDatabase.erase(uidToDelete);
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

    sendTelegramNotification(uidToDelete, nameOfDeletedUser, "USER_DELETED");
    syncUserListToSheets();
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
        userDatabase[uid] = name; // Update map immediately for responsiveness
        xSemaphoreTake(sdMutex, portMAX_DELAY);
        File file = SD.open(USER_DATABASE_FILE, FILE_APPEND);
        if (file) {
            file.println(uid + "," + name);
            file.close();
        }
        xSemaphoreGive(sdMutex);

        sendTelegramNotification(uid, name, "USER_ADDED");
        syncUserListToSheets();
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
  
  EEPROM.begin(EEPROM_SIZE);
  if (server.hasArg("admin_user")) {
    admin_user = server.arg("admin_user");
    writeStringToEEPROM(EEPROM_ADMIN_USER_ADDR, admin_user);
  }
  if (server.hasArg("admin_pass")) {
    String newPass = server.arg("admin_pass");
    writeStringToEEPROM(EEPROM_ADMIN_PASS_ADDR, newPass);
    admin_pass = newPass;
  }
  EEPROM.commit();
  EEPROM.end();
  
  server.sendHeader("Location", "/admin", true);
  server.send(302, "text/plain", "");
}

void handleChangeAddUserPassword() {
  if (admin_pass.length() > 0) {
    if (!server.authenticate(admin_user.c_str(), admin_pass.c_str())) return;
  }
  
  EEPROM.begin(EEPROM_SIZE);
  if (server.hasArg("add_user_user")) {
    add_user_user = server.arg("add_user_user");
    writeStringToEEPROM(EEPROM_ADD_USER_USER_ADDR, add_user_user);
  }
  if (server.hasArg("add_user_pass")) {
    String newPass = server.arg("add_user_pass");
    writeStringToEEPROM(EEPROM_ADD_USER_PASS_ADDR, newPass);
    add_user_pass = newPass;
  }
  EEPROM.commit();
  EEPROM.end();

  server.sendHeader("Location", "/admin", true);
  server.send(302, "text/plain", "");
}

void handleNotFound() { server.send(404, "text/plain", "404: Not Found"); }

void handleUpdatePage() {
  if (admin_pass.length() > 0) {
    if (!server.authenticate(admin_user.c_str(), admin_pass.c_str())) { return server.requestAuthentication(); }
  }
  server.send_P(200, "text/html", PAGE_OTA_UPDATE);
}

void handleWifiConfigPage() {
  if (admin_pass.length() > 0) {
    if (!server.authenticate(admin_user.c_str(), admin_pass.c_str())) { return server.requestAuthentication(); }
  }
  server.send_P(200, "text/html", PAGE_WIFI_CONFIG);
}

void handleSaveWifiConfig() {
  if (admin_pass.length() > 0) {
    if (!server.authenticate(admin_user.c_str(), admin_pass.c_str())) { return; }
  }
  if (server.hasArg("ssid") && server.hasArg("pass")) {
    String new_ssid = server.arg("ssid");
    String new_pass = server.arg("pass");
    
    EEPROM.begin(EEPROM_SIZE);
    writeStringToEEPROM(EEPROM_WIFI_SSID_ADDR, new_ssid);
    writeStringToEEPROM(EEPROM_WIFI_PASS_ADDR, new_pass);
    EEPROM.commit();
    EEPROM.end();
    
    server.send(200, "text/html", "<h2>Settings Saved!</h2><p>Device will restart with new settings in 5 seconds.</p>");
    delay(5000);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Bad Request: Missing SSID or Password.");
  }
}

//=========================================================
// HELPER FUNCTIONS
//=========================================================
void writeStringToEEPROM(int addr, const String& str) {
  int len = str.length();
  for (int i = 0; i < len; i++) {
    EEPROM.write(addr + i, str[i]);
  }
  EEPROM.write(addr + len, '\0');
}

String readStringFromEEPROM(int addr, int maxLen) {
  char data[maxLen + 1];
  int len = 0;
  byte c = EEPROM.read(addr + len);
  while (c != '\0' && c != 0xFF && len < maxLen) {
    data[len] = (char)c;
    len++;
    c = EEPROM.read(addr + len);
  }
  data[len] = '\0';
  return String(data);
}

void sendDataToGoogleSheets() {
  Serial.println("\n[GSheet] Checking for data to upload...");
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  if (!SD.exists(G_SHEETS_QUEUE_FILE)) {
    xSemaphoreGive(sdMutex);
    return;
  }
  File queueFile = SD.open(G_SHEETS_QUEUE_FILE, FILE_READ);
  if (!queueFile || queueFile.size() == 0) {
    if(queueFile) queueFile.close();
    SD.remove(G_SHEETS_QUEUE_FILE);
    xSemaphoreGive(sdMutex);
    return;
  }
  queueFile.close();
  if (SD.exists(G_SHEETS_SENDING_FILE)) { SD.remove(G_SHEETS_SENDING_FILE); }
  bool renamed = SD.rename(G_SHEETS_QUEUE_FILE, G_SHEETS_SENDING_FILE);
  xSemaphoreGive(sdMutex);
  if (!renamed) { Serial.println("[GSheet] ERROR: Could not rename queue file."); return; }

  xSemaphoreTake(sdMutex, portMAX_DELAY);
  File sendingFile = SD.open(G_SHEETS_SENDING_FILE, FILE_READ);
  xSemaphoreGive(sdMutex);

  if (sendingFile) {
    WiFiClientSecure localClient;
    localClient.setInsecure();
    HTTPClient http;

    if (http.begin(localClient, GOOGLE_SCRIPT_ID)) {
      http.setTimeout(15000);
      http.addHeader("Content-Type", "text/plain");
      int httpCode = http.sendRequest("POST", &sendingFile, sendingFile.size());
      if(httpCode > 0) {
          Serial.printf("[GSheet] Upload successful, code: %d\n", httpCode);
          xSemaphoreTake(sdMutex, portMAX_DELAY);
          if(SD.exists(G_SHEETS_SENDING_FILE)) { SD.remove(G_SHEETS_SENDING_FILE); }
          xSemaphoreGive(sdMutex);
      } else {
          Serial.printf("[GSheet] Upload failed, error: %s\n", http.errorToString(httpCode).c_str());
          xSemaphoreTake(sdMutex, portMAX_DELAY);
          SD.rename(G_SHEETS_SENDING_FILE, G_SHEETS_QUEUE_FILE); // Rename back to try again later
          xSemaphoreGive(sdMutex);
      }
      http.end();
    }
    sendingFile.close();
  } else { Serial.println("[GSheet] ERROR: Could not read sending file."); }
}

void syncUserListToSheets() {
  Serial.println("[GSheet] Syncing user list to Google Sheets...");
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  File userFile = SD.open(USER_DATABASE_FILE, FILE_READ);
  if (!userFile) {
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

  if(payload.length() > strlen("USER_LIST_UPDATE")) {
    WiFiClientSecure localClient;
    localClient.setInsecure();
    HTTPClient http;
    if (http.begin(localClient, GOOGLE_SCRIPT_ID)) {
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
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char logFilePath[40];
    strftime(logFilePath, sizeof(logFilePath), "/logs/%Y-%m-%d.csv", &timeinfo);
    
    if(!SD.exists(LOGS_DIRECTORY)){ SD.mkdir(LOGS_DIRECTORY); }
    
    File localFile = SD.open(logFilePath, FILE_APPEND);
    if(localFile) {
        if(localFile.size() == 0) {
            localFile.println("Timestamp,Action,UID,Name,Duration_sec,Duration_Formatted");
        }
        String entry = getFormattedTime(time(nullptr)) + "," + event + "," + uid + "," + name + "," + String(duration) + "," + formatDuration(duration);
        localFile.println(entry);
        localFile.close();
        Serial.printf("[SD] Log entry written to %s.\n", logFilePath);
    } else {
        Serial.printf("[SD] ERROR: Could not open %s for writing.\n", logFilePath);
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
    lcd.setCursor(0, 0); lcd.print("Date: " + String(dateStr));
    lcd.setCursor(0, 1); lcd.print("Time: " + String(timeStr));
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
    sendSystemAlertToTelegram("☀️ *Good Morning!* ☀️\n\n_All user statuses have been reset for the new day._");
  }
}

void playBuzzer(int status) {
  if (status == 1) { // ENTER
    tone(BUZZER_PIN, BEEP_FREQUENCY, 150); delay(180); tone(BUZZER_PIN, BEEP_FREQUENCY, 400);
  } else if (status == 0) { // EXIT
    tone(BUZZER_PIN, BEEP_FREQUENCY, 100); delay(120); tone(BUZZER_PIN, BEEP_FREQUENCY, 100); delay(120); tone(BUZZER_PIN, BEEP_FREQUENCY, 100);
  } else if (status == 2) { // DENIED
    tone(BUZZER_PIN, BEEP_FREQUENCY, 1000);
  }
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
  } else { Serial.println("[SD] ERROR: Could not find users.csv file."); }
  xSemaphoreGive(sdMutex);
}

void setupTime() {
  Serial.print("[Time] Configuring time from NTP server...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10000)) { Serial.println(" Failed to obtain time."); return; }
  Serial.println(" Done.");
}

String getUIDString(MFRC522::Uid uid) {
  String uidStr = "";
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) uidStr += "0";
    uidStr += String(uid.uidByte[i], HEX);
    if (i < uid.size - 1) uidStr += " ";
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
  char buf[20];
  snprintf(buf, sizeof(buf), "%02ldh %02ldm %02lds", hours, minutes, seconds);
  return String(buf);
}

void startAPMode() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid);
  IPAddress apIP = WiFi.softAPIP();

  Serial.println("\n[Network Task] Starting Access Point mode.");
  Serial.print("[Network Task] Connect to SSID: '"); Serial.print(ap_ssid);
  Serial.print("' and go to http://"); Serial.println(apIP);

  updateDisplayMessage(ap_ssid, apIP.toString());
  
  setupAPServer();
  server.begin();
  Serial.println("[Network Task] Web Server configured for WiFi Setup.");
}

void sendSystemAlertToTelegram(String message) {
  if (WiFi.status() != WL_CONNECTED) return;
  
  WiFiClientSecure localClient;
  localClient.setInsecure();
  UniversalTelegramBot localBot(BOT_TOKEN, localClient);

  Serial.println("[Telegram] Sending system alert...");
  if (!localBot.sendMessage(CHAT_ID, message, "Markdown")) {
    Serial.println("[Telegram] Failed to send system alert.");
  }
}

void sendTelegramNotification(String uid, String name, String action) {
  if (WiFi.status() != WL_CONNECTED) return;
  
  WiFiClientSecure localClient;
  localClient.setInsecure();
  UniversalTelegramBot localBot(BOT_TOKEN, localClient);

  String message = "";
  String formattedTime = getFormattedTime(time(nullptr));

  if (action == "ENTER") {
    message = "✅ *ENTERED* ✅\n\n*Name:* " + name + "\n*UID:* `" + uid + "`\n*Time:* " + formattedTime;
  } else if (action == "EXIT") {
    message = "🚪 *JUST LEFT* 🚪\n\n*Name:* " + name + "\n*UID:* `" + uid + "`\n*Time:* " + formattedTime;
  } else if (action == "DENIED") {
    message = "❌ *INVALID USER!!* ❌\n\n*UID:* `" + uid + "`\n*Time:* " + formattedTime;
  } else if (action == "USER_ADDED") {
    message = "👤 *NEW USER ADDED* 👤\n\n*Name:* " + name + "\n*UID:* `" + uid + "`";
  } else if (action == "USER_DELETED") {
    message = "🗑️ *USER DELETED* 🗑️\n\n*Name:* " + name + "\n*UID:* `" + uid + "`";
  } else {
    return;
  }

  Serial.println("[Telegram] Sending notification...");
  if (!localBot.sendMessage(CHAT_ID, message, "Markdown")) {
    Serial.println("[Telegram] Failed to send notification.");
  }
}