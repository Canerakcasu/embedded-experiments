// Bu dosya, RAM'den tasarruf etmek için tüm web içeriğini PROGMEM'de saklar
// ve web sunucusu isteklerini yöneten fonksiyonları içerir.

#include <ArduinoJson.h>

//=========================================================
// YENİ GLOBAL DEĞİŞKENLER (Yeni Kullanıcı Tarama Özelliği İçin)
//=========================================================
// volatile anahtar kelimesi, bu değişkenlerin birden fazla görev (Task) tarafından
// güvenli bir şekilde değiştirilebileceğini belirtir.
volatile bool scan_for_new_user = false;
String new_card_uid = "";


//=========================================================
// WEB SAYFASI İÇERİKLERİ (PROGMEM)
//=========================================================

// --- Modern ve Tutarlı CSS ---
const char PAGE_STYLE_CSS[] PROGMEM = R"rawliteral(
:root { --primary-color: #007bff; --secondary-color: #6c757d; --background-color: #f8f9fa; --card-bg-color: #ffffff; --text-color: #212529; --border-color: #dee2e6; --success-color: #28a745; --danger-color: #dc3545;}
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif; background-color: var(--background-color); margin: 0; padding: 15px; color: var(--text-color); }
.container { max-width: 900px; margin: 0 auto; }
.card { background-color: var(--card-bg-color); border: 1px solid var(--border-color); border-radius: 0.75rem; box-shadow: 0 0.125rem 0.25rem rgba(0,0,0,0.075); margin-bottom: 20px; padding: 25px; }
h1, h2, h3 { color: var(--text-color); font-weight: 500; }
h1 { text-align: center; color: var(--primary-color); margin-bottom: 30px; }
h2 { border-bottom: 1px solid var(--border-color); padding-bottom: 10px; margin-top: 30px; margin-bottom: 20px; font-size: 1.5em; }
nav { display: flex; gap: 10px; justify-content: center; margin-bottom: 30px; }
.btn { display: inline-block; font-weight: 600; color: #fff; text-align: center; vertical-align: middle; cursor: pointer; background-color: var(--primary-color); border: 1px solid transparent; padding: 0.5rem 1rem; font-size: 1rem; border-radius: 0.25rem; text-decoration: none; transition: all 0.2s ease-in-out; }
.btn-secondary { background-color: var(--secondary-color); }
.btn-danger { background-color: var(--danger-color); font-size: 0.8em; padding: 0.3rem 0.6rem;}
.btn:hover { opacity: 0.85; }
.grid-container { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 20px; }
.status-card p { margin: 0; }
.status-card .label { font-weight: 600; color: var(--secondary-color); margin-bottom: 5px; display: block; }
.status-card .value { font-size: 1.2em; }
table { width: 100%; border-collapse: collapse; margin-top: 20px; }
th, td { padding: 12px; border: 1px solid var(--border-color); text-align: left; vertical-align: middle; }
th { background-color: #f2f2f2; }
form { margin-top: 20px; }
input[type=text], input[type=password] { width: 100%; padding: 10px; margin-bottom: 15px; border: 1px solid var(--border-color); border-radius: 4px; box-sizing: border-box; font-size: 16px; }
.form-group { margin-bottom: 15px; }
.form-group label { display: block; margin-bottom: 5px; font-weight: 600; }
.form-row { display: flex; gap: 15px; align-items: flex-end; }
.form-row .form-group { flex-grow: 1; }
.scan-info { font-size: 0.9em; color: var(--secondary-color); margin-top: 5px; }
)rawliteral";

// --- Ana Panel Sayfası ---
const char PAGE_MAIN_DASHBOARD[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>RFID Control System</title><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><link href="/style.css" rel="stylesheet" type="text/css"></head>
<body><div class="container"><h1>RFID Access Control</h1><nav><a href="/" class="btn">Dashboard</a><a href="/admin" class="btn btn-secondary">Admin Panel</a><a href="/logs" class="btn btn-secondary">Activity Logs</a></nav>
<div class="card"><h2>System Status</h2><div class="grid-container">
<div class="status-card"><p class="label">WiFi Status</p><p class="value" id="wifiStatus">Connected</p></div>
<div class="status-card"><p class="label">IP Address</p><p class="value" id="ipAddress">--.--.--.--</p></div>
<div class="status-card"><p class="label">Current Time</p><p class="value" id="currentTime">--:--:--</p></div>
</div></div>
<div class="card"><h2>Last Event</h2><div class="grid-container">
<div class="status-card"><p class="label">Time</p><p class="value" id="eventTime">-</p></div>
<div class="status-card"><p class="label">Card UID</p><p class="value" id="eventUID">-</p></div>
<div class="status-card"><p class="label">Name</p><p class="value" id="eventName">-</p></div>
<div class="status-card"><p class="label">Action</p><p class="value" id="eventAction">-</p></div>
</div></div></div>
<script>
function updateTime() { document.getElementById('currentTime').innerText = new Date().toLocaleTimeString('en-GB'); }
function fetchData() { fetch('/data').then(r=>r.json()).then(d=>{
    document.getElementById('eventTime').innerText = d.time;
    document.getElementById('eventUID').innerText = d.uid;
    document.getElementById('eventName').innerText = d.name;
    document.getElementById('eventAction').innerText = d.action;
    document.getElementById('ipAddress').innerText = d.ip;
});}
setInterval(updateTime, 1000); setInterval(fetchData, 2000); window.onload=()=>{updateTime();fetchData();};
</script></body></html>
)rawliteral";


//=========================================================
// WEB SUNUCUSU HANDLER FONKSİYONLARI
//=========================================================

void handleRoot() { server.send_P(200, "text/html", PAGE_MAIN_DASHBOARD); }
void handleCSS() { server.send_P(200, "text/css", PAGE_STYLE_CSS); }

void handleData() {
  StaticJsonDocument<256> doc;
  doc["time"] = lastEventTime;
  doc["uid"] = lastEventUID;
  doc["name"] = lastEventName;
  doc["action"] = lastEventAction;
  doc["ip"] = WiFi.localIP().toString();
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleAdmin() {
  if (!server.authenticate(ADMIN_USER, ADMIN_PASS)) { return server.requestAuthentication(); }
  
  String html = "<html><head><title>Admin Panel</title><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><link href='/style.css' rel='stylesheet'></head><body><div class='container'>";
  html += "<h1>User Management</h1><nav><a href='/' class='btn btn-secondary'>&larr; Back to Dashboard</a></nav>";
  
  // --- Yeni Kullanıcı Ekleme Formu ---
  html += "<div class='card'><h2>Add New User</h2><form action='/adduser' method='post'>";
  html += "<div class='form-row'><div class='form-group' style='flex-grow: 2;'><label for='uid'>Card UID</label><input type='text' name='uid' id='uidInput' required></div>";
  html += "<div class='form-group'><label>&nbsp;</label><button type='button' class='btn btn-secondary' id='scanBtn' onclick='startScan()'>Scan Card</button></div></div>";
  html += "<p class='scan-info' id='scanStatus'></p>";
  html += "<div class='form-group'><label for='name'>Name</label><input type='text' name='name' required></div>";
  html += "<input type='submit' class='btn' value='Add User'></form></div>";

  // --- Mevcut Kullanıcılar Tablosu ---
  html += "<div class='card'><h2>Registered Users</h2><table><tr><th>UID</th><th>Name</th><th>Status</th><th>Action</th></tr>";
  if (userDatabase.empty()) {
    html += "<tr><td colspan='4'>No users registered.</td></tr>";
  } else {
    for (auto const& [uid, name] : userDatabase) {
      html += "<tr><td>" + uid + "</td><td>" + name + "</td><td>" + (userStatus[uid] ? "INSIDE" : "OUTSIDE") + "</td>";
      html += "<td><a href='/deleteuser?uid=" + uid + "' class='btn btn-danger' onclick='return confirm(\"Delete " + name + "?\");'>Delete</a></td></tr>";
    }
  }
  html += "</table></div>";
  
  // --- JavaScript for Scan Feature ---
  html += R"rawliteral(<script>
    let scanInterval;
    function startScan() {
      const scanBtn = document.getElementById('scanBtn');
      const scanStatus = document.getElementById('scanStatus');
      scanBtn.disabled = true;
      scanStatus.innerText = 'Please present a new card to the reader within 15 seconds...';
      fetch('/enable-scan-mode');
      
      let attempts = 0;
      scanInterval = setInterval(() => {
        fetch('/get-scanned-uid').then(r=>r.json()).then(data => {
          if(data.uid) {
            document.getElementById('uidInput').value = data.uid;
            scanStatus.innerText = 'Card UID captured successfully!';
            stopScan();
          }
        });
        attempts++;
        if (attempts > 15) {
            scanStatus.innerText = 'Scan timed out. Please try again.';
            stopScan();
        }
      }, 1000);
    }
    function stopScan() {
      clearInterval(scanInterval);
      document.getElementById('scanBtn').disabled = false;
    }
  </script>)rawliteral";

  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleLogs() {
  if (!server.authenticate(ADMIN_USER, ADMIN_PASS)) { return server.requestAuthentication(); }
  String html = "<html><head><title>Activity Logs</title><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><link href='/style.css' rel='stylesheet'></head><body><div class='container'>";
  html += "<h1>Activity Logs</h1><nav><a href='/' class='btn btn-secondary'>&larr; Back to Dashboard</a></nav>";
  
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){ 
    html += "<div class='card'><h3>Error: Could not get time.</h3></div>"; 
  } else {
    char logFilePath[40];
    strftime(logFilePath, sizeof(logFilePath), "/logs/%Y/%m/%d.csv", &timeinfo);
    
    xSemaphoreTake(sdMutex, portMAX_DELAY);
    File file = SD.open(logFilePath);
    xSemaphoreGive(sdMutex);
    
    if(file && file.size() > 0){
      html += "<div class='card'><h2>Daily Summary</h2><table><tr><th>Name</th><th>Total Time Inside</th></tr>";
      std::map<String, unsigned long> dailyTotals;
      
      xSemaphoreTake(sdMutex, portMAX_DELAY);
      file.seek(0);
      if(file.available()) file.readStringUntil('\n'); // Skip header
      while(file.available()){
        String line = file.readStringUntil('\n');
        // Simple CSV parsing
        if(line.indexOf("EXIT") != -1) {
          int firstComma = line.indexOf(',');
          int secondComma = line.indexOf(',', firstComma + 1);
          int thirdComma = line.indexOf(',', secondComma + 1);
          int fourthComma = line.indexOf(',', thirdComma + 1);
          int fifthComma = line.indexOf(',', fourthComma + 1);
          String name = line.substring(thirdComma + 1, fourthComma);
          unsigned long duration = line.substring(fourthComma + 1, fifthComma).toInt();
          dailyTotals[name] += duration;
        }
      }
      xSemaphoreGive(sdMutex);

      if(dailyTotals.empty()){ html += "<tr><td colspan='2'>No completed sessions for today.</td></tr>"; } 
      else { 
        for (auto const& [name, totalDuration] : dailyTotals) { 
          html += "<tr><td>" + name + "</td><td>" + formatDuration(totalDuration) + "</td></tr>"; 
        } 
      }
      html += "</table></div>";

      html += "<div class='card'><h2>Detailed Log</h2><table><tr><th>Time</th><th>Action</th><th>UID</th><th>Name</th><th>Duration</th></tr>";
      xSemaphoreTake(sdMutex, portMAX_DELAY);
      file.seek(0);
      if(file.available()) file.readStringUntil('\n'); // Skip header
      while(file.available()){
        String line = file.readStringUntil('\n');
        line.trim();
        if(line.length() > 0){
          String parts[6];
          int lastIndex = -1;
          for(int i = 0; i < 6; i++) {
              int commaIndex = line.indexOf(',', lastIndex + 1);
              if (commaIndex == -1) {
                  parts[i] = line.substring(lastIndex + 1);
                  break;
              }
              parts[i] = line.substring(lastIndex + 1, commaIndex);
              lastIndex = commaIndex;
          }
          html += "<tr><td>" + parts[0] + "</td><td>" + parts[1] + "</td><td>" + parts[2] + "</td><td>" + parts[3] + "</td><td>" + parts[5] + "</td></tr>";
        }
      }
      file.close();
      xSemaphoreGive(sdMutex);
      html += "</table></div>";
    } else { 
      html += "<div class='card'><h3>No log file found for today.</h3></div>"; 
    }
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
            if (!line.startsWith(uidToDelete + ",")) {
                tempFile.print(line);
            }
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
    String uid = server.arg("uid");
    String name = server.arg("name");
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
        syncUserListToSheets();
    }
  }
  server.sendHeader("Location", "/admin", true);
  server.send(302, "text/plain", "");
}

// --- Yeni Handler'lar (UID Tarama için) ---
void handleEnableScanMode() {
  scan_for_new_user = true;
  new_card_uid = ""; // Önceki taranan kartı temizle
  server.send(200, "text/plain", "Scan mode enabled.");
}

void handleGetScannedUid() {
  if (new_card_uid != "") {
    server.send(200, "application/json", "{\"uid\":\"" + new_card_uid + "\"}");
    new_card_uid = ""; // UID gönderildikten sonra temizle
  } else {
    server.send(200, "application/json", "{\"uid\":null}");
  }
}

void handleNotFound() { server.send(404, "text/plain", "404: Not Found"); }
void handleStatus() {}