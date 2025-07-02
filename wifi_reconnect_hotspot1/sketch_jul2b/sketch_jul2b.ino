#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>

// EEPROM'da verileri saklamak için ayıracağımız alan
#define EEPROM_SIZE 128

//======================================================================
// GLOBAL VARIABLES FOR WIFI CREDENTIALS
// These variables act as the single source of truth for the application.
// They are loaded from EEPROM at boot.
char ssid[33];
char password[64];
//======================================================================

// Web Server on port 80
WebServer server(80);

// Constants for the Access Point (Hotspot) mode
const char* ap_ssid = "ESP-Setup-Device";

// Other global variables and timers
bool is_in_ap_mode = false;
const unsigned long reconnect_timeout = 30000; // 30 seconds
unsigned long last_disconnect_time = 0;

/**
 * @brief Saves the new WiFi credentials to the permanent EEPROM storage.
 * @param new_ssid The new network SSID to save.
 * @param new_password The new network password to save.
 */
void saveCredentials(const String& new_ssid, const String& new_password) {
  Serial.println("Saving new credentials to EEPROM...");
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.writeString(0, new_ssid);
  EEPROM.writeString(64, new_password);
  if (EEPROM.commit()) {
    Serial.println("EEPROM successfully committed.");
  } else {
    Serial.println("ERROR: EEPROM commit failed!");
  }
  EEPROM.end();

  // Also update the global variables immediately
  strncpy(ssid, new_ssid.c_str(), sizeof(ssid));
  strncpy(password, new_password.c_str(), sizeof(password));
}

/**
 * @brief Loads WiFi credentials from EEPROM into the global variables at boot.
 * If EEPROM is empty, it loads hard-coded default values.
 */
void loadCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  String esid = EEPROM.readString(0);
  EEPROM.end();
  
  if (esid.length() > 0 && esid.length() < 33) {
    EEPROM.begin(EEPROM_SIZE);
    String epass = EEPROM.readString(64);
    EEPROM.end();
    strncpy(ssid, esid.c_str(), sizeof(ssid));
    strncpy(password, epass.c_str(), sizeof(password));
    Serial.println("Loaded credentials from EEPROM.");
  } else {
    strcpy(ssid, "default_ssid");
    strcpy(password, "default_pass");
    Serial.println("EEPROM is empty. Loaded default credentials.");
  }
}

/**
 * @brief Scans for nearby WiFi networks and returns them as a JSON string.
 * This is called by the JavaScript on the web page.
 */
void handleScan() {
  Serial.println("Scan request received.");
  int n = WiFi.scanNetworks();
  Serial.print(n);
  Serial.println(" networks found.");
  
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"rssi\":" + String(WiFi.RSSI(i));
    json += ",\"ssid\":\"" + WiFi.SSID(i) + "\"";
    // Check if network is encrypted
    json += ",\"secure\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true");
    json += "}";
  }
  json += "]";
  
  server.send(200, "application/json", json);
}

/**
 * @brief Serves the main HTML page with modern UI and JavaScript.
 */
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE HTML>
<html><head>
<title>ESP WiFi Setup</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  :root { --primary-color: #007bff; --background-color: #f4f6f8; --card-bg-color: #ffffff; --text-color: #333; --border-color: #dee2e6; }
  body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif; background-color: var(--background-color); margin: 0; padding: 20px; color: var(--text-color); }
  .container { background-color: var(--card-bg-color); max-width: 500px; margin: 20px auto; padding: 25px; border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
  h2 { text-align: center; color: var(--primary-color); margin-top: 0; }
  label { display: block; margin-bottom: 8px; font-weight: 600; }
  input[type='text'], input[type='password'] { width: 100%; padding: 12px; margin-bottom: 20px; border: 1px solid var(--border-color); border-radius: 4px; box-sizing: border-box; font-size: 16px; }
  button, input[type='submit'] { background-color: var(--primary-color); color: white; padding: 12px 20px; border: none; border-radius: 4px; cursor: pointer; width: 100%; font-size: 16px; font-weight: 600; }
  button:hover, input[type='submit']:hover { background-color: #0056b3; }
  .scan-btn { background-color: #6c757d; margin-bottom: 20px; }
  .scan-btn:hover { background-color: #5a6268; }
  #wifi-list { list-style: none; padding: 0; max-height: 200px; overflow-y: auto; border: 1px solid var(--border-color); border-radius: 4px; }
  #wifi-list li { padding: 10px 15px; border-bottom: 1px solid var(--border-color); cursor: pointer; display: flex; justify-content: space-between; align-items: center; }
  #wifi-list li:last-child { border-bottom: none; }
  #wifi-list li:hover { background-color: #e9ecef; }
  #wifi-list .ssid { font-weight: bold; }
  #wifi-list .rssi { color: #6c757d; }
  .loader { text-align: center; padding: 15px; display: none; }
</style>
</head><body>
<div class="container">
  <h2>WiFi Network Setup</h2>
  <form action="/save" method="POST">
    <label for="ssid">Network Name (SSID)</label>
    <input type="text" id="ssid" name="ssid" required>
    <label for="password">Password</label>
    <input type="password" id="password" name="password">
    <input type="submit" value="Save and Connect">
  </form>
  <hr style="border:none; border-top:1px solid var(--border-color); margin: 25px 0;">
  <h3>Available Networks</h3>
  <button class="scan-btn" onclick="scanNetworks()">Scan for Networks</button>
  <div class="loader" id="loader">Scanning...</div>
  <ul id="wifi-list"></ul>
</div>

<script>
  function selectSsid(ssid) {
    document.getElementById('ssid').value = ssid;
  }

  function scanNetworks() {
    const list = document.getElementById('wifi-list');
    const loader = document.getElementById('loader');
    list.innerHTML = '';
    loader.style.display = 'block';

    fetch('/scan')
      .then(response => response.json())
      .then(data => {
        loader.style.display = 'none';
        data.sort((a, b) => b.rssi - a.rssi); // Sort by signal strength
        data.forEach(network => {
          const li = document.createElement('li');
          li.onclick = () => selectSsid(network.ssid);
          const lock_icon = network.secure ? '&#128274;' : ''; // Lock emoji for secure networks
          li.innerHTML = `<span class="ssid">${network.ssid}</span> <span class="rssi">(${network.rssi} dBm) ${lock_icon}</span>`;
          list.appendChild(li);
        });
      })
      .catch(error => {
        loader.style.display = 'none';
        console.error('Error scanning networks:', error);
      });
  }
  // Scan automatically on page load
  window.onload = scanNetworks;
</script>
</body></html>
  )rawliteral";
  server.send(200, "text/html", html);
}

/**
 * @brief Starts the AP (Hotspot) mode and the web server for configuration.
 */
void startAPMode() {
  if (is_in_ap_mode) return;
  is_in_ap_mode = true;
  Serial.println("Starting AP mode...");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid);

  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Define web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan); // New route for scanning
  server.on("/save", HTTP_POST, []() {
    String new_ssid = server.arg("ssid");
    String new_password = server.arg("password");
    saveCredentials(new_ssid, new_password);
    server.send(200, "text/html", "<h2>Settings Saved!</h2><p>The device will restart and try to connect to the new network in 5 seconds.</p>");
    delay(5000);
    ESP.restart();
  });
  
  server.begin();
  Serial.println("Web server started. Visit http://192.168.4.1 to configure.");
}

/**
 * @brief Connects to WiFi using the credentials from the global variables.
 */
void connectToWiFi() {
  is_in_ap_mode = false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi.");
    if (last_disconnect_time == 0) last_disconnect_time = millis();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nDevice starting...");
  loadCredentials();
  connectToWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (!is_in_ap_mode) {
      if (last_disconnect_time == 0) {
        Serial.println("WiFi connection lost!");
        last_disconnect_time = millis();
      }
      if (millis() - last_disconnect_time > reconnect_timeout) {
        startAPMode();
      }
    }
  } else {
    if (is_in_ap_mode) ESP.restart();
    last_disconnect_time = 0;
    // This part only runs when connected to WiFi.
  }

  if (is_in_ap_mode) {
    server.handleClient();
  }
}