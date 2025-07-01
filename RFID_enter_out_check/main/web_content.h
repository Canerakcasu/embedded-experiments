#pragma once

const char PAGE_CSS[] PROGMEM = R"rawliteral(
body { font-family: 'Roboto', sans-serif; background-color: #121212; color: #e0e0e0; text-align: center; margin: 0; padding: 30px; }
.container { background: #1e1e1e; padding: 20px 40px; border-radius: 12px; box-shadow: 0 8px 16px rgba(0,0,0,0.4); display: inline-block; min-width: 700px; border: 1px solid #333; }
h1, h2, h3 { color: #ffffff; font-weight: 300; text-align: center; }
h1 { font-size: 2.2em; } h2 { border-bottom: 2px solid #03dac6; padding-bottom: 10px; font-weight: 400; margin-top: 30px;} h3 { margin-top: 30px; color: #bb86fc; }
.data-grid { display: grid; grid-template-columns: 150px 1fr; gap: 12px; text-align: left; margin-top: 25px; } .data-grid span { padding: 10px; border-radius: 5px; }
.data-grid span:nth-child(odd) { background-color: #333; font-weight: bold; color: #03dac6; } .data-grid span:nth-child(even) { background-color: #2c2c2c; }
.footer-nav { font-size:0.9em; color:#bbb; margin-top:30px; } .footer-nav a { color: #bb86fc; text-decoration: none; padding: 0 10px; }
table { width: 100%; border-collapse: collapse; margin-top: 20px; } th, td { padding: 12px; border: 1px solid #444; text-align: left; vertical-align: middle;}
th { background-color: #03dac6; color: #121212; font-weight: 700; }
form { margin-top: 20px; padding: 20px; border: 1px solid #333; border-radius: 8px; background-color: #2c2c2c; }
input[type=text], input[type=password] { width: calc(50% - 24px); padding: 10px; margin: 5px; border-radius: 4px; border: 1px solid #555; background: #333; color: #e0e0e0; }
input[type=submit], button { padding: 10px 20px; border: none; border-radius: 4px; background: #03dac6; color: #121212; font-weight: bold; cursor: pointer; margin-top: 10px; }
button.btn-reboot { background-color: #cf6679; } .btn-delete { color: #cf6679; text-decoration: none; font-weight: bold; }
a.btn-download { color: #81d4fa; text-decoration: none; } .home-link { margin-bottom: 20px; display: inline-block; color: #bb86fc; font-size: 1.1em; }
.status { padding: 5px 10px; border-radius: 15px; font-size: 0.85em; text-align: center; color: white; font-weight: bold; }
.status-in { background-color: #28a745; } .status-out { background-color: #6c757d; }
.uid-reader { background-color: #252525; border: 1px dashed #555; padding: 20px; margin-top: 15px; border-radius: 8px; }
.uid-reader p { margin: 0; font-size: 1.1em; color: #aaa; }
.uid-reader #lastUID { font-family: 'Courier New', monospace; font-size: 1.5em; color: #03dac6; font-weight: bold; margin-top: 10px; min-height: 28px;}
)rawliteral";

const char PAGE_Main[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>RFID Control System</title><meta charset="UTF-8"><link href="https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;700&display=swap" rel="stylesheet"><link href="/style.css" rel="stylesheet" type="text/css"></head>
<body><div class="container"><h1>RFID Access Control</h1><h2 id="currentTime">--:--:--</h2><h2>Last Event</h2>
<div class="data-grid"><span>Time:</span><span id="eventTime">-</span><span>Card UID:</span><span id="eventUID">-</span><span>Name:</span><span id="eventName">-</span><span>Action:</span><span id="eventAction">-</span></div>
<p class="footer-nav"><a href="/admin">Admin Panel</a> | <a href="/logs">Access Logs</a> | <a href="/adduserpage">Add New User</a></p></div>
<script>
function updateTime() { document.getElementById('currentTime').innerText = new Date().toLocaleTimeString('en-US'); }
function fetchData() { fetch('/data').then(response => response.json()).then(data => {
document.getElementById('eventTime').innerText = data.time; document.getElementById('eventUID').innerText = data.uid;
document.getElementById('eventName').innerText = data.name; document.getElementById('eventAction').innerText = data.action;});}
setInterval(updateTime, 1000); setInterval(fetchData, 2000); window.onload = () => { updateTime(); fetchData(); };
</script></body></html>
)rawliteral";

const char PAGE_AddUser[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>Add New User</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head><body><div class='container'>
<h1>Add New User</h1><a href='/' class='home-link'>&larr; Back to Dashboard</a><div class="uid-reader"><p>Please scan RFID card/tag now...</p><div id="lastUID">N/A</div></div>
<form action='/adduser' method='post'><h3>User Information</h3>Card UID: <input type='text' id='uid_field' name='uid' required><br>Name: <input type='text' name='name' required><br><br><input type='submit' value='Add User'></form></div>
<script>
function getLastUID() { fetch('/getlastuid').then(response => response.text()).then(uid => { if (uid && uid !== "N/A") {
document.getElementById('lastUID').innerText = uid; document.getElementById('uid_field').value = uid; } }); }
setInterval(getLastUID, 1000); window.onload = getLastUID;
</script></body></html>
)rawliteral";

const char PAGE_Update[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>Firmware Update</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head><body><div class='container'>
<h1>OTA Firmware Update</h1><a href='/admin' class='home-link'>&larr; Back to Admin Panel</a>
<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update' accept='.bin'><input type='submit' value='Update Firmware'></form>
<p>Update may take a few minutes. The device will reboot automatically after a successful update.</p></div></body></html>
)rawliteral";

void handleRoot() { server.send_P(200, "text/html", PAGE_Main); }
void handleCSS() { server.send_P(200, "text/css", PAGE_CSS); }

void handleData() {
  StaticJsonDocument<256> doc;
  doc["time"] = lastEventTime; doc["uid"] = lastEventUID;
  doc["name"] = lastEventName; doc["action"] = lastEventAction;
  String json; serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleAddUserPage() {
  if (!server.authenticate(ADD_USER_AUTH_USER, ADD_USER_PASS.c_str())) { return server.requestAuthentication(); }
  server.send_P(200, "text/html", PAGE_AddUser);
}

void handleGetLastUID() { server.send(200, "text/plain", lastEventUID); }

void handleReboot() {
  if (!server.authenticate(ADMIN_USER.c_str(), ADMIN_PASS.c_str())) return;
  server.send(200, "text/plain", "Device rebooting in 3 seconds...");
  delay(3000); ESP.restart();
}

void handleChangePassword() {
  if (!server.authenticate(ADMIN_USER.c_str(), ADMIN_PASS.c_str())) return;
  if (server.hasArg("newpass")) {
    String newPass = server.arg("newpass"); newPass.trim();
    if (newPass.length() > 3 && newPass.length() < 32) {
      writeStringToEEPROM(0, newPass); ADMIN_PASS = newPass;
      server.send(200, "text/plain", "Admin password changed successfully.");
    } else { server.send(400, "text/plain", "Password must be between 4 and 31 characters."); }
  } else { server.send(400, "text/plain", "No new password provided."); }
}

void handleChangeAddUserPassword() {
  if (!server.authenticate(ADMIN_USER.c_str(), ADMIN_PASS.c_str())) return;
  if (server.hasArg("newpass")) {
    String newPass = server.arg("newpass"); newPass.trim();
    if (newPass.length() > 2 && newPass.length() < 32) {
      writeStringToEEPROM(32, newPass); ADD_USER_PASS = newPass;
      server.send(200, "text/plain", "'Add User' password changed successfully.");
    } else { server.send(400, "text/plain", "Password must be between 3 and 31 characters."); }
  } else { server.send(400, "text/plain", "No new password provided."); }
}

void listFiles(String& html, const char* dirname, int levels) {
    File root = SD.open(dirname);
    if (!root || !root.isDirectory()) return;
    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            if (levels) listFiles(html, file.path(), levels - 1);
        } else {
            String filePath = String(file.path());
            html += "<tr><td>" + filePath + "</td><td>" + String(file.size()) + " B</td>";
            html += "<td><a href='/download?file=" + filePath + "' class='btn-download'>Download</a></td></tr>";
        }
        file = root.openNextFile();
    }
}

void handleFileDownload() {
    if (!server.authenticate(ADMIN_USER.c_str(), ADMIN_PASS.c_str())) return;
    if (server.hasArg("file")) {
        String path = server.arg("file");
        if (path.indexOf("..") != -1 || !path.startsWith("/")) {
            server.send(400, "text/plain", "Invalid file path."); return;
        }
        xSemaphoreTake(sdMutex, portMAX_DELAY);
        File file = SD.open(path, FILE_READ);
        xSemaphoreGive(sdMutex);
        if (file) {
            server.sendHeader("Content-Disposition", "attachment; filename=\"" + path.substring(path.lastIndexOf('/') + 1) + "\"");
            server.streamFile(file, "application/octet-stream");
            file.close();
        } else { server.send(404, "text/plain", "File not found."); }
    } else { server.send(400, "text/plain", "File parameter missing."); }
}

void handleUpdate() {
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", (Update.hasError()) ? "UPDATE FAILED" : "UPDATE OK. Rebooting...");
  ESP.restart();
}

void handleUpdateUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) { if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial); }
  else if (upload.status == UPLOAD_FILE_WRITE) { if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial); }
  else if (upload.status == UPLOAD_FILE_END) { if (!Update.end(true)) Update.printError(Serial); }
}

void handleUpdatePage() {
    if (!server.authenticate(ADMIN_USER.c_str(), ADMIN_PASS.c_str())) { return server.requestAuthentication(); }
    server.send_P(200, "text/html", PAGE_Update);
}

void handleAdmin() {
  if (!server.authenticate(ADMIN_USER.c_str(), ADMIN_PASS.c_str())) { return server.requestAuthentication(); }
  String html = "<html><head><title>Admin Panel</title><meta charset='UTF-8'><link href='/style.css' rel='stylesheet' type='text/css'></head><body><div class='container'>";
  html += "<h1>Admin Panel</h1><a href='/' class='home-link'>&larr; Back to Dashboard</a>";
  html += "<h2>Security Settings</h2>";
  html += "<h3>Admin Password</h3>";
  html += "<form action='/changepass' method='post'>New Admin Password: <input type='password' name='newpass' required><br><input type='submit' value='Change'></form>";
  html += "<h3>'Add User' Page Password (user: user)</h3>";
  html += "<form action='/changeadduserpass' method='post'>New 'Add User' Password: <input type='password' name='newpass' required><br><input type='submit' value='Change'></form>";
  html += "<h2>User Management</h2>";
  html += "<table><tr><th>UID</th><th>Name</th><th>Current Status</th><th>Action</th></tr>";
  for (auto const& [uid, name] : userDatabase) {
    html += "<tr><td>" + uid + "</td><td>" + name + "</td><td>" + (userStatus[uid] ? "<span class='status status-in'>INSIDE</span>" : "<span class='status status-out'>OUTSIDE</span>") + "</td>";
    html += "<td><a href='/deleteuser?uid=" + uid + "' class='btn-delete' onclick='return confirm(\"Are you sure?\");'>Delete</a></td></tr>";
  }
  html += "</table>";
  html += "<h2>Device Management</h2>";
  html += "<form action='/reboot' method='post' onsubmit='return confirm(\"Reboot the device?\");'><button class='btn-reboot'>Reboot Device</button></form>";
  html += "<h2>Firmware Update</h2>";
  html += "<p>Update firmware via OTA. Upload a .bin file.</p>";
  html += "<form action='/update' method='get'><button>Go to Update Page</button></form>";
  html += "<h2>SD Card File Manager</h2>";
  html += "<table><tr><th>File Path</th><th>Size</th><th>Action</th></tr>";
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  listFiles(html, "/", 0);
  listFiles(html, LOGS_DIRECTORY, 2);
  xSemaphoreGive(sdMutex);
  html += "</table>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void handleLogs() {
  if (!server.authenticate(ADMIN_USER.c_str(), ADMIN_PASS.c_str())) { return server.requestAuthentication(); }
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
          int lastIdx = -1;
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
  if (!server.authenticate(ADMIN_USER.c_str(), ADMIN_PASS.c_str())) return;
  if (server.hasArg("uid")) {
    String uidToDelete = server.arg("uid");
    xSemaphoreTake(sdMutex, portMAX_DELAY);
    userStatus.erase(uidToDelete); entryTime.erase(uidToDelete);
    File originalFile = SD.open(USER_DATABASE_FILE, FILE_READ);
    File tempFile = SD.open(TEMP_USER_FILE, FILE_WRITE);
    if (!originalFile || !tempFile) { xSemaphoreGive(sdMutex); server.send(500, "text/plain", "File error."); return; }
    while (originalFile.available()) {
      String line = originalFile.readStringUntil('\n'); line.trim();
      if (line.length() > 0 && !line.startsWith(uidToDelete + ",")) { tempFile.println(line); }
    }
    originalFile.close(); tempFile.close();
    SD.remove(USER_DATABASE_FILE); SD.rename(TEMP_USER_FILE, USER_DATABASE_FILE);
    xSemaphoreGive(sdMutex);
    loadUsersFromSd(); syncUserListToSheets();
  }
  server.sendHeader("Location", "/admin", true);
  server.send(302, "text/plain", "");
}
void handleAddUser() {
  if (!server.authenticate(ADD_USER_AUTH_USER, ADD_USER_PASS.c_str())) return;
  if (server.hasArg("uid") && server.hasArg("name")) {
    String uid = server.arg("uid"); String name = server.arg("name");
    uid.trim(); name.trim();
    xSemaphoreTake(sdMutex, portMAX_DELAY);
    File file = SD.open(USER_DATABASE_FILE, FILE_APPEND);
    if (file) {
      file.println(uid + "," + name); file.close();
      xSemaphoreGive(sdMutex);
      loadUsersFromSd(); syncUserListToSheets();
    } else { xSemaphoreGive(sdMutex); }
  }
  server.sendHeader("Location", "/admin", true);
  server.send(302, "text/plain", "");
}
void handleNotFound() { server.send(404, "text/plain", "404: Not Found"); }