// This file contains all web content handlers.
// This version generates static pages for maximum stability.
// The live-update features (timers, etc.) are removed to reduce complexity.

// --- Web Page Handlers ---

void handleRoot() {
  String html = F("<!DOCTYPE html><html><head><title>RFID System</title><meta charset='UTF-8'>"
                "<style>body{font-family:sans-serif; background-color:#1e1e1e; color:#e0e0e0; text-align:center; padding: 30px;} .container{background:#2c2c2c; padding:20px; border-radius:8px; display:inline-block;}</style>"
                "<meta http-equiv='refresh' content='5'></head>"
                "<body><div class='container'><h1>RFID Access Control - Stable</h1>"
                "<h2>Last Event</h2>"
                "<p>Time: ");
  html += lastEventTime;
  html += F("</p><p>Card UID: ");
  html += lastEventUID;
  html += F("</p><p>Name: ");
  html += lastEventName;
  html += F("</p><p>Action: ");
  html += lastEventAction;
  html += F("</p><hr><a href='/admin'>Admin Panel</a> | <a href='/logs'>Activity Logs</a></div></body></html>");
  server.send(200, "text/html", html);
}

void handleAdmin() {
  if (!server.authenticate(ADMIN_USER, ADMIN_PASS)) {
    return server.requestAuthentication();
  }
  String html = F("<!DOCTYPE html><html><head><title>Admin Panel</title><meta charset='UTF-8'><style>body{font-family:sans-serif; background-color:#1e1e1e; color:#e0e0e0;} table,th,td{border:1px solid #555; border-collapse:collapse; padding:8px;} th{background-color:#03dac6; color:#121212;} a{color:#bb86fc;}</style></head>"
                "<body><h1>User Management</h1><a href='/'>Back to Dashboard</a> | <a href='/logs'>View Logs</a>");
  html += F("<table style='margin-top:20px;'><tr><th>UID</th><th>Name</th><th>Current Status</th><th>Action</th></tr>");
  
  for (auto const& [uid, name] : userDatabase) {
    html += "<tr><td>" + uid + "</td><td>" + name + "</td><td>" + (userStatus[uid] ? "<span style='color:green;'>INSIDE</span>" : "<span style='color:gray;'>OUTSIDE</span>") + "</td>";
    html += "<td><a href='/deleteuser?uid=" + uid + "' onclick='return confirm(\"Delete this user?\");'>Delete</a></td></tr>";
  }
  
  html += F("</table><h2>Add New User</h2><form action='/adduser' method='post'>"
            "Card UID: <input type='text' name='uid' required><br>"
            "Name: <input type='text' name='name' required><br><br>"
            "<input type='submit' value='Add User'></form></body></html>");
  server.send(200, "text/html", html);
}

void handleLogs() {
  if (!server.authenticate(ADMIN_USER, ADMIN_PASS)) {
    return server.requestAuthentication();
  }
  String html = F("<!DOCTYPE html><html><head><title>Activity Logs</title><meta charset='UTF-8'><style>body{font-family:sans-serif; background-color:#1e1e1e; color:#e0e0e0;} table,th,td{border:1px solid #555; border-collapse:collapse; padding:8px;} th{background-color:#03dac6; color:#121212;} a{color:#bb86fc;}</style></head>"
                "<body><h1>Activity Logs</h1><a href='/'>Back to Dashboard</a>");
  
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    html += F("<h3>Error: Could not get time to find log file.</h3>");
  } else {
    char logFilePath[40];
    strftime(logFilePath, sizeof(logFilePath), "/logs/%Y/%m/%d.csv", &timeinfo);
    File file = SD.open(logFilePath);
    if(file && file.size() > 0){
      // Daily Summary Calculation
      html += F("<h2>Daily Summary</h2><table style='width:50%;'><tr><th>Name</th><th>Total Time Inside</th></tr>");
      std::map<String, unsigned long> dailyTotals;
      file.seek(0);
      if(file.available()) file.readStringUntil('\n'); // Skip header
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
      html += F("</table>");

      // Detailed Log Display
      html += F("<h2>Detailed Log</h2><pre style='text-align:left; background-color:#2c2c2c; padding:10px; border:1px solid #ccc; white-space:pre-wrap;'>");
      file.seek(0);
      while(file.available()){
        html += file.readStringUntil('\n') + "\n";
      }
      html += F("</pre>");
      file.close();
    } else {
      html += F("<h3>No log file for today.</h3>");
    }
  }
  html += F("</body></html>");
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

// These handlers are not used in this simplified version but are declared.
void handleCSS() {}
void handleData() {}
void handleStatus() {}