// Bu dosya, RAM'den tasarruf etmek için tüm web içeriğini PROGMEM'de saklar.

#include <ArduinoJson.h> // Hatanın çözümü için bu kütüphane buraya eklendi.

// --- PROGMEM Strings for CSS and Main Page HTML ---
const char PAGE_CSS[] PROGMEM = R"rawliteral(
body { 
  font-family: 'Roboto', sans-serif; 
  background-color: #121212; 
  color: #e0e0e0;
  text-align: center; 
  margin: 0;
  padding: 30px;
}
.container { 
  background: #1e1e1e; 
  padding: 20px 40px; 
  border-radius: 12px; 
  box-shadow: 0 8px 16px rgba(0,0,0,0.4); 
  display: inline-block; 
  min-width: 600px; 
  border: 1px solid #333;
}
h1, h2, h3 {
  color: #ffffff;
  font-weight: 300;
  text-align: center;
}
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
input[type=text] { width: calc(50% - 10px); padding: 10px; margin: 5px; border-radius: 4px; border: 1px solid #555; background: #333; color: #e0e0e0; }
input[type=submit] { padding: 10px 20px; border: none; border-radius: 4px; background: #03dac6; color: #121212; font-weight: bold; cursor: pointer; }
.btn-delete { color: #cf6679; text-decoration: none; font-weight: bold; }
.home-link { margin-bottom: 20px; display: inline-block; color: #bb86fc; }
.status { padding: 5px 10px; border-radius: 15px; font-size: 0.85em; text-align: center; color: white; font-weight: bold; }
.status-in { background-color: #28a745; }
.status-out { background-color: #6c757d; }
)rawliteral";

const char PAGE_Main[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>RFID Control System</title><meta charset="UTF-8"><link href="https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;700&display=swap" rel="stylesheet"><link href="/style.css" rel="stylesheet" type="text/css"></head>
<body>
  <div class="container">
    <h1>RFID Access Control</h1>
    <h2 id="currentTime">--:--:--</h2>
    <h2>Last Event</h2>
    <div class="data-grid">
      <span>Time:</span><span id="eventTime">-</span>
      <span>Card UID:</span><span id="eventUID">-</span>
      <span>Name:</span><span id="eventName">-</span>
      <span>Action:</span><span id="eventAction">-</span>
    </div>
    <p class="footer-nav"><a href="/admin">Admin Panel</a> | <a href="/logs">Activity Logs</a></p>
  </div>
<script>
function updateTime() { document.getElementById('currentTime').innerText = new Date().toLocaleTimeString('en-GB'); }
function fetchData() { fetch('/data').then(response => response.json()).then(data => {
      document.getElementById('eventTime').innerText = data.time;
      document.getElementById('eventUID').innerText = data.uid;
      document.getElementById('eventName').innerText = data.name;
      document.getElementById('eventAction').innerText = data.action;
    });
}
setInterval(updateTime, 1000);
setInterval(fetchData, 1000);
window.onload = () => { updateTime(); fetchData(); };
</script>
</body>
</html>
)rawliteral";

// --- Web Page Handlers ---

void handleRoot() { server.send_P(200, "text/html", PAGE_Main); }
void handleCSS() { server.send_P(200, "text/css", PAGE_CSS); }

void handleData() {
  StaticJsonDocument<200> doc;
  doc["time"] = lastEventTime;
  doc["uid"] = lastEventUID;
  doc["name"] = lastEventName;
  doc["action"] = lastEventAction;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleAdmin() {
  if (!server.authenticate(ADMIN_USER, ADMIN_PASS)) return server.requestAuthentication();
  String html = "<html><head><title>Admin Panel</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head><body><div class='container'>";
  html += "<h1>User Management</h1><a href='/' class='home-link'>&larr; Back to Dashboard</a> | <a href='/logs'>View Logs</a>";
  html += "<table style='margin-top:20px;'><tr><th>UID</th><th>Name</th><th>Current Status</th><th>Action</th></tr>";
  for (auto const& [uid, name] : userDatabase) {
    html += "<tr><td>" + uid + "</td><td>" + name + "</td><td>" + (userStatus[uid] ? "<span class='status status-in'>INSIDE</span>" : "<span class='status status-out'>OUTSIDE</span>") + "</td>";
    html += "<td><a href='/deleteuser?uid=" + uid + "' onclick='return confirm(\"Delete this user?\");'>Delete</a></td></tr>";
  }
  html += "</table>";
  html += "<h2>Add New User</h2><form action='/adduser' method='post'>"
          "Card UID: <input type='text' name='uid' required><br>"
          "Name: <input type='text' name='name' required><br><br>"
          "<input type='submit' value='Add User'></form></body></html>";
  server.send(200, "text/html", html);
}

void handleLogs() {
  if (!server.authenticate(ADMIN_USER, ADMIN_PASS)) return server.requestAuthentication();
  String html = "<html><head><title>Activity Logs</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head><body><div class='container'>";
  html += "<h1>Activity Logs</h1><a href='/' class='home-link'>&larr; Back to Dashboard</a>";
  
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    html += "<h3>Error: Could not get time to find log file.</h3>";
  } else {
    char logFilePath[40];
    strftime(logFilePath, sizeof(logFilePath), "/logs/%Y/%m/%d.csv", &timeinfo);
    File file = SD.open(logFilePath);
    if(file && file.size() > 0){
      html += "<h3>Daily Summary</h3><table style='width:50%;'><tr><th>Name</th><th>Total Time Inside</th></tr>";
      std::map<String, unsigned long> dailyTotals;
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
       if(dailyTotals.empty()){
         html += "<tr><td colspan='2'>No completed sessions for today.</td></tr>";
      } else {
        for (auto const& [name, totalDuration] : dailyTotals) {
          html += "<tr><td>" + name + "</td><td>" + formatDuration(totalDuration) + "</td></tr>";
        }
      }
      html += "</table>";

      html += "<h3>Detailed Log</h3><table><tr><th>Time</th><th>Action</th><th>UID</th><th>Name</th><th>Duration</th></tr>";
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
          if (lastIndex != -1 && lastIndex < line.length() - 1) part[5] = line.substring(lastIndex + 1); else part[5] = "-";
          html += "<tr><td>" + part[0] + "</td><td>" + part[1] + "</td><td>" + part[2] + "</td><td>" + part[3] + "</td><td>" + part[5] + "</td></tr>";
        }
      }
      file.close();
    } else {
      html += "<h3>No log file for today.</h3>";
    }
  }
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleDeleteUser() {
  if (!server.authenticate(ADMIN_USER, ADMIN_PASS)) return;
  if (server.hasArg("uid")) {
    String uidToDelete = server.arg("uid");
    userStatus.erase(uidToDelete);
    entryTime.erase(uidToDelete);
    File originalFile = SD.open(USER_DATABASE_FILE, FILE_READ);
    File tempFile = SD.open(TEMP_USER_FILE, FILE_WRITE);
    if (!originalFile || !tempFile) { server.send(500, "text/plain", "File error."); return; }
    while (originalFile.available()) {
      String line = originalFile.readStringUntil('\n'); line.trim();
      if (line.length() > 0 && !line.startsWith(uidToDelete + ",")) { tempFile.println(line); }
    }
    originalFile.close(); tempFile.close();
    SD.remove(USER_DATABASE_FILE);
    SD.rename(TEMP_USER_FILE, USER_DATABASE_FILE);
    loadUsersFromSd();
  }
  server.sendHeader("Location", "/admin", true);
  server.send(302, "text/plain", "");
}

void handleAddUser() {
  if (!server.authenticate(ADMIN_USER, ADMIN_PASS)) return;
  if (server.hasArg("uid") && server.hasArg("name")) {
    String uid = server.arg("uid"); String name = server.arg("name");
    uid.trim(); name.trim();
    File file = SD.open(USER_DATABASE_FILE, FILE_APPEND);
    if (file) { file.println(uid + "," + name); file.close(); loadUsersFromSd(); }
  }
  server.sendHeader("Location", "/admin", true);
  server.send(302, "text/plain", "");
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

// These handlers are not used in this version but are declared for completeness.
void handleStatus() {}