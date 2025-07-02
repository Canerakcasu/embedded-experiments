#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>

#define EEPROM_SIZE 128

// Cihazın durumlarını yönetmek için bir enum tanımlıyoruz
enum DeviceState {
  STATE_AP_MODE,
  STATE_CONNECTING,
  STATE_CONNECTED,
  STATE_DISCONNECTED
};
DeviceState currentState = STATE_DISCONNECTED; // Başlangıç durumu

char ssid[33];
char password[64];
const char* ap_ssid = "ESP-Setup-Device";

WebServer server(80);
unsigned long last_disconnect_time = 0;
unsigned long connection_attempt_start_time = 0;
const unsigned long reconnect_timeout = 30000; // 30 saniye
const unsigned long new_connection_timeout = 20000; // Yeni ağ denemesi için 20 saniye

// Önceki koddan fonksiyonlar (değişiklik yok)
void saveCredentials(const String& new_ssid, const String& new_password);
void loadCredentials();
void handleRoot();
void handleScan();

/**
 * @brief Yeni bilgileri aldıktan sonra AP modunu kapatır ve yeni ağa bağlanmayı başlatır.
 */
void startConnectingToNewWifi() {
  Serial.println("Shutting down AP mode...");
  server.stop(); // Web sunucusunu durdur
  WiFi.softAPdisconnect(true); // Hotspot'u kapat
  WiFi.mode(WIFI_STA); // Modu istemci (Station) olarak ayarla
  
  Serial.print("Attempting to connect to new network: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  connection_attempt_start_time = millis(); // Bağlantı denemesi için zamanlayıcıyı başlat
}

/**
 * @brief Hotspot modunu ve ayar sayfasını sunan web sunucusunu başlatır.
 */
void startAPMode() {
  currentState = STATE_AP_MODE;
  Serial.println("Starting AP mode...");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid);

  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Web sunucusu yönlendirmeleri
  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/save", HTTP_POST, []() {
    String new_ssid = server.arg("ssid");
    String new_password = server.arg("password");
    
    // YENİ MANTIK: Cihazı yeniden başlatmak yerine durumu değiştir
    saveCredentials(new_ssid, new_password);
    currentState = STATE_CONNECTING; // Ana döngüye yeni ağa bağlanmasını söyle
    
    server.send(200, "text/html", "<h2>Settings Saved!</h2><p>Attempting to connect to the new network. Please check the device status.</p>");
  });
  
  server.begin();
  Serial.println("Web server started. Visit http://192.168.4.1 to configure.");
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nDevice starting...");
  loadCredentials();
  
  WiFi.begin(ssid, password);
  currentState = STATE_CONNECTING; // İlk açılışta da bağlanma durumundayız
  connection_attempt_start_time = millis();
}

void loop() {
  // === DURUM MAKİNESİ ===
  switch (currentState) {
    case STATE_CONNECTING:
      // Yeni bir ağa bağlanmayı deniyoruz
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnection successful!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        currentState = STATE_CONNECTED;
      } else if (millis() - connection_attempt_start_time > new_connection_timeout) {
        Serial.println("\nFailed to connect. Starting AP mode to reconfigure.");
        startAPMode(); // Başarısız olursa AP moduna geri dön
      }
      break;

    case STATE_CONNECTED:
      // WiFi'ye bağlıyız, normal çalışma
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost!");
        last_disconnect_time = millis();
        currentState = STATE_DISCONNECTED;
      }
      // --- BURAYA NORMAL ÇALIŞMA KODUNUZU YAZIN ---
      break;

    case STATE_DISCONNECTED:
      // Bağlantı koptu, yeniden bağlanmayı dene veya AP moduna geç
      if (WiFi.reconnect()) { // Yeniden bağlanmayı dene
         Serial.println("Reconnected successfully!");
         currentState = STATE_CONNECTED;
      } else if (millis() - last_disconnect_time > reconnect_timeout) {
         Serial.println("Reconnect failed. Starting AP mode.");
         startAPMode();
      }
      delay(1000); // Sürekli denememek için kısa bir bekleme
      break;

    case STATE_AP_MODE:
      // AP modundayız, web sunucusundan gelecek isteği dinle
      server.handleClient();
      break;
  }
}


// --- Diğer Fonksiyonlar (Önceki koddan kopyalanabilir, değişiklik yok) ---

void saveCredentials(const String& new_ssid, const String& new_password) {
  Serial.println("Saving new credentials to EEPROM...");
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.writeString(0, new_ssid);
  EEPROM.writeString(64, new_password);
  EEPROM.commit();
  EEPROM.end();
  strncpy(ssid, new_ssid.c_str(), sizeof(ssid));
  strncpy(password, new_password.c_str(), sizeof(password));
}

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
  } else {
    strcpy(ssid, "default_ssid");
    strcpy(password, "default_pass");
  }
}

void handleRoot() {
   String html = R"rawliteral(
      <!DOCTYPE HTML>
      <html><head><title>ESP WiFi Setup</title><meta name="viewport" content="width=device-width, initial-scale=1">
      <style>:root{--primary-color:#007bff;--background-color:#f4f6f8;--card-bg-color:#ffffff;--text-color:#333;--border-color:#dee2e6}body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif;background-color:var(--background-color);margin:0;padding:20px;color:var(--text-color)}.container{background-color:var(--card-bg-color);max-width:500px;margin:20px auto;padding:25px;border-radius:8px;box-shadow:0 4px 8px rgba(0,0,0,.1)}h2{text-align:center;color:var(--primary-color);margin-top:0}label{display:block;margin-bottom:8px;font-weight:600}input[type=text],input[type=password]{width:100%;padding:12px;margin-bottom:20px;border:1px solid var(--border-color);border-radius:4px;box-sizing:border-box;font-size:16px}button,input[type=submit]{background-color:var(--primary-color);color:#fff;padding:12px 20px;border:none;border-radius:4px;cursor:pointer;width:100%;font-size:16px;font-weight:600}button:hover,input[type=submit]:hover{background-color:#0056b3}.scan-btn{background-color:#6c757d;margin-bottom:20px}.scan-btn:hover{background-color:#5a6268}#wifi-list{list-style:none;padding:0;max-height:200px;overflow-y:auto;border:1px solid var(--border-color);border-radius:4px}#wifi-list li{padding:10px 15px;border-bottom:1px solid var(--border-color);cursor:pointer;display:flex;justify-content:space-between;align-items:center}#wifi-list li:last-child{border-bottom:none}#wifi-list li:hover{background-color:#e9ecef}#wifi-list .ssid{font-weight:700}#wifi-list .rssi{color:#6c757d}.loader{text-align:center;padding:15px;display:none}</style>
      </head><body><div class="container"><h2>WiFi Network Setup</h2><form action="/save" method="POST"><label for="ssid">Network Name (SSID)</label><input type="text" id="ssid" name="ssid" required><label for="password">Password</label><input type="password" id="password" name="password"><input type="submit" value="Save and Connect"></form><hr style="border:none;border-top:1px solid var(--border-color);margin:25px 0"><h3>Available Networks</h3><button class="scan-btn" onclick="scanNetworks()">Scan for Networks</button><div class="loader" id="loader">Scanning...</div><ul id="wifi-list"></ul></div>
      <script>function selectSsid(e){document.getElementById("ssid").value=e}function scanNetworks(){const e=document.getElementById("wifi-list"),t=document.getElementById("loader");e.innerHTML="",t.style.display="block",fetch("/scan").then(e=>e.json()).then(t=>{t.style.display="none",t.sort((e,t)=>t.rssi-e.rssi),t.forEach(t=>{const n=document.createElement("li");n.onclick=()=>selectSsid(t.ssid);const o=t.secure?"&#128274;":"";n.innerHTML=`<span class="ssid">${t.ssid}</span> <span class="rssi">(${t.rssi} dBm) ${o}</span>`,e.appendChild(n)})}).catch(e=>{t.style.display="none",console.error("Error scanning networks:",e)})}window.onload=scanNetworks;</script>
      </body></html>
   )rawliteral";
   server.send(200, "text/html", html);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    json += "{\"rssi\":" + String(WiFi.RSSI(i)) + ",\"ssid\":\"" + WiFi.SSID(i) + "\",\"secure\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}