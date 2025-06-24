/*****************************************************************************************
 * ESP32 Web Tabanlı Gelişmiş MP3 Çalar ve Dosya Yöneticisi v4 (Final Düzeltme)
 * * Değişiklikler:
 * - .replace() hatası düzeltildi.
 * - Tüm parantez yapıları kontrol edildi.
 *****************************************************************************************/

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SD.h>

// Gerekli ses kütüphanesi bileşenleri
#include "AudioGeneratorMP3.h"
#include "AudioFileSourceSD.h"
#include "AudioOutputI2S.h"

// --- KULLANICI AYARLARI ---
const char* ssid = "Ents_Test";      // Kendi Wi-Fi ağ adınızı yazın
const char* password = "12345678"; // Kendi Wi-Fi şifrenizi yazın

// --- PIN TANIMLAMALARI ---
#define SD_CS    5  // SD Kart Modülü CS pini

#define I2S_BCLK 26 // I2S Amfi BCLK (Bit Clock) pini
#define I2S_LRC  25 // I2S Amfi LRC (Left/Right Clock) pini
#define I2S_DOUT 22 // I2S Amfi DIN (Data In) pini

// --- GLOBAL NESNELER VE DEĞİŞKENLER ---
AsyncWebServer server(80);
File uploadFile;

// Ses bileşenleri
AudioGeneratorMP3 *mp3;
AudioFileSourceSD *file;
AudioOutputI2S *out;

// Durum takibi için değişkenler
String nowPlaying = "Hiçbiri";
bool sdCardReady = false;

// --- WEB ARAYÜZÜ (HTML, CSS, JavaScript) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 MP3 Çalar</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background-color: #121212; color: #e0e0e0; margin: 0; padding: 20px;}
    .container { max-width: 800px; margin: auto; background: #1e1e1e; padding: 20px; border-radius: 8px; box-shadow: 0 2px 8px rgba(0,0,0,0.5); border: 1px solid #333; }
    h1, h2 { color: #FFFFFF; border-bottom: 2px solid #FFFFFF; padding-bottom: 5px; }
    .status-panel { background-color: #2a2a2a; padding: 15px; border-radius: 8px; margin-bottom: 20px; }
    .status-panel p { margin: 5px 0; font-size: 16px; }
    .status-panel .label { font-weight: bold; color: #FFFFFF; }
    .status-panel .value { color: #e0e0e0; }
    .file-list { list-style: none; padding: 0; }
    .file-item { display: flex; align-items: center; justify-content: space-between; padding: 12px; border-bottom: 1px solid #333; }
    .file-item:last-child { border-bottom: none; }
    .file-name { font-weight: bold; font-size: 16px; }
    .btn { padding: 8px 15px; border: none; border-radius: 5px; color: white; cursor: pointer; text-decoration: none; font-size: 14px; transition: opacity 0.2s; margin-left: 10px; }
    .btn:hover { opacity: 0.8; }
    .btn-play { background-color: #1D744D; color: white; font-weight: bold; margin-left: 0; }
    .btn-rename { background-color: #D4AC0D; }
    .btn-delete { background-color: #B03A2E; }
    .btn-stop { background-color: #616161; color: white; display: block; width: 100px; text-align: center; margin: 20px auto; }
    .volume-control { display: flex; align-items: center; justify-content: center; margin: 20px 0; }
    .volume-control label { font-size: 16px; margin-right: 10px; }
    .volume-control input[type="range"] { width: 50%; }
    .upload-form { margin-top: 30px; padding: 20px; background: #2a2a2a; border-radius: 8px; }
    input[type="submit"] { background-color: #424242; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; transition: background-color 0.2s; }
    input[type="submit"]:hover { background-color: #555555; }
    #status { margin-top: 15px; font-style: italic; color: #aaa; }
    input[type="file"]::file-selector-button { background-color: #333; color: #e0e0e0; border: 1px solid #555; padding: 8px 12px; border-radius: 5px; cursor: pointer; transition: background-color 0.2s; }
    input[type="file"]::file-selector-button:hover { background-color: #444; }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 Web MP3 Çalar</h1>
    <div class="status-panel">
      <h2>Durum Paneli</h2>
      <p><span class="label">SD Kart:</span> <span id="sd-status" class="value">Bilinmiyor</span></p>
      <p><span class="label">Şimdi Çalıyor:</span> <span id="now-playing" class="value">Hiçbiri</span></p>
    </div>
    <div class="volume-control">
      <label for="volume">Ses Seviyesi</label>
      <input type="range" id="volume" name="volume" min="0.0" max="4.0" step="0.1" value="1.0" onchange="setVolume(this.value)">
    </div>
    <h2>SD Karttaki Dosyalar</h2>
    <ul class="file-list">
      %FILE_LIST%
    </ul>
    <a href="#" onclick="stopPlayback()" class="btn btn-stop">Durdur</a>
    <div class="upload-form">
      <h2>Yeni Ses Dosyası Yükle</h2>
      <form id="upload-form" method="POST" enctype="multipart/form-data">
        <input type="file" name="upload">
        <input type="submit" value="Yükle">
      </form>
      <div id="status"></div>
    </div>
  </div>
  <script>
    function playFile(filename) {
      document.getElementById('status').innerHTML = 'Çalma isteği gönderildi: ' + filename;
      fetch('/play?file=' + encodeURIComponent(filename)).then(() => setTimeout(updateStatus, 500));
    }
    function renameFile(oldName) {
      let newName = prompt("Dosya için yeni bir ad girin:", oldName);
      if (newName && newName !== "" && newName !== oldName) {
        if (!newName.toLowerCase().endsWith('.mp3')) { newName += '.mp3'; }
        document.getElementById('status').innerHTML = oldName + ' -> ' + newName + ' olarak değiştiriliyor...';
        fetch(`/rename?old=${encodeURIComponent(oldName)}&new=${encodeURIComponent(newName)}`)
          .then(response => {
            if (response.ok) {
              document.getElementById('status').innerHTML = 'Dosya başarıyla yeniden adlandırıldı.';
              setTimeout(() => { window.location.reload(); }, 1000);
            } else {
              document.getElementById('status').innerHTML = 'Hata: Dosya yeniden adlandırılamadı.';
            }
          });
      } else {
        document.getElementById('status').innerHTML = 'Yeniden adlandırma iptal edildi.';
      }
    }
    function deleteFile(filename) {
      if (confirm(filename + ' dosyasını silmek istediğinizden emin misiniz?')) {
        document.getElementById('status').innerHTML = 'Siliniyor: ' + filename;
        fetch('/delete?file=' + encodeURIComponent(filename)).then(response => {
          if (response.ok) {
            document.getElementById('status').innerHTML = filename + ' silindi.';
            setTimeout(() => { window.location.reload(); }, 1000);
          } else {
            document.getElementById('status').innerHTML = 'Hata: ' + filename + ' silinemedi.';
          }
        });
      }
    }
    function stopPlayback(){
      document.getElementById('status').innerHTML = 'Durdurma isteği gönderildi.';
      fetch('/stop').then(() => setTimeout(updateStatus, 500));
    }
    function setVolume(value){
      fetch(`/volume?level=${value}`);
    }
    function updateStatus() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          document.getElementById('sd-status').textContent = data.sd_status;
          document.getElementById('now-playing').textContent = data.now_playing;
        })
        .catch(error => console.error('Durum güncellenirken hata:', error));
    }
    window.onload = function() {
      updateStatus();
      setInterval(updateStatus, 5000); 
    };
    const form = document.getElementById('upload-form');
    form.addEventListener('submit', function(event) {
      event.preventDefault();
      const statusDiv = document.getElementById('status');
      const formData = new FormData(form);
      const fileInput = form.querySelector('input[type="file"]');
      if (!fileInput.files || fileInput.files.length === 0) {
        statusDiv.innerHTML = 'Lütfen önce bir dosya seçin.';
        return;
      }
      statusDiv.innerHTML = 'Yükleniyor, lütfen bekleyin...';
      fetch('/upload', { method: 'POST', body: formData })
      .then(response => response.text())
      .then(result => {
        statusDiv.innerHTML = result;
        form.reset();
        setTimeout(() => { window.location.reload(); }, 2000);
      })
      .catch(error => { statusDiv.innerHTML = 'Yükleme hatası: ' + error; });
    });
  </script>
</body>
</html>
)rawliteral";

// --- YARDIMCI FONKSİYONLAR ---
String listFiles() {
  String fileList = "";
  File root = SD.open("/");
  if (!root) { return "<p>SD Kart kök dizini açılamadı!</p>"; }
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String fileNameWithPath = String(file.name());
      if (fileNameWithPath.endsWith(".mp3") || fileNameWithPath.endsWith(".MP3")) {
          // file.name() /file.mp3 gibi tam yol verir. Sadece dosya adını almak için baştaki / karakterini kaldıralım.
          String fileName = fileNameWithPath.substring(1); 
          fileList += "<li class='file-item'><span class='file-name'>" + fileName + "</span>";
          fileList += "<div><a href='#' onclick='playFile(\"" + fileName + "\")' class='btn btn-play'>Çal</a>";
          fileList += "<a href='#' onclick='renameFile(\"" + fileName + "\")' class='btn btn-rename'>Adlandır</a>";
          fileList += "<a href='#' onclick='deleteFile(\"" + fileName + "\")' class='btn btn-delete'>Sil</a></div></li>";
      }
    }
    file = root.openNextFile();
  }
  root.close();
  if (fileList == "") { return "<p>SD Kartta hiç .mp3 dosyası bulunamadı.</p>"; }
  return fileList;
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  String path = "/" + filename;
  if (index == 0) {
    if (mp3->isRunning()) { mp3->stop(); nowPlaying = "Hiçbiri"; }
    Serial.printf("Yükleme başladı: %s\n", filename.c_str());
    uploadFile = SD.open(path, FILE_WRITE);
    if(!uploadFile){ Serial.println("Dosya oluşturulamadı."); return; }
  }
  if(len){ uploadFile.write(data, len); }
  if (final) {
    uploadFile.close();
    Serial.printf("Yükleme tamamlandı: %s, Boyut: %u byte\n", filename.c_str(), index + len);
    request->send(200, "text/plain", "Dosya Başarıyla Yüklendi!");
  }
}

// --- ANA KURULUM VE DÖNGÜ ---
void setup() {
  Serial.begin(115200);
  Serial.println("\n--- ESP32 Gelişmiş MP3 Çalar Başlatılıyor (v4 Final) ---");

  if (SD.begin(SD_CS)) {
    sdCardReady = true;
    Serial.println("SD Kart başarıyla başlatıldı.");
  } else {
    sdCardReady = false;
    Serial.println("SD Kart başlatılamadı!");
  }

  WiFi.begin(ssid, password);
  Serial.print("Wi-Fi'ye bağlanılıyor...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nBağlantı başarılı!");
  Serial.print("ESP32 IP Adresi: http://");
  Serial.println(WiFi.localIP());

  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  out->SetGain(1.0);
  mp3 = new AudioGeneratorMP3();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String htmlPage = index_html;
    htmlPage.replace("%FILE_LIST%", listFiles());
    request->send(200, "text/html", htmlPage);
  });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200);
  }, handleUpload);

  server.on("/play", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("file") && sdCardReady) {
      if (mp3->isRunning()) { mp3->stop(); }
      if (file) { delete file; file = nullptr; }
      nowPlaying = request->getParam("file")->value();
      String path = "/" + nowPlaying;
      file = new AudioFileSourceSD(path.c_str());
      if (!file->isOpen()) {
        nowPlaying = "HATA: Dosya açılamadı";
        delete file; file = nullptr;
        request->send(404, "text/plain", "Dosya bulunamadı");
        return;
      }
      mp3->begin(file, out);
      Serial.printf("Çalınıyor: %s\n", path.c_str());
      request->send(200, "text/plain", "Çalınıyor...");
    } else {
      request->send(400, "text/plain", "Hata.");
    }
  });

  server.on("/rename", HTTP_GET, [](AsyncWebServerRequest *request){
    if (mp3->isRunning()) { mp3->stop(); nowPlaying = "Hiçbiri"; }
    if (request->hasParam("old") && request->hasParam("new")) {
      String oldPath = "/" + request->getParam("old")->value();
      String newPath = "/" + request->getParam("new")->value();
      if (SD.rename(oldPath, newPath)) { request->send(200); } else { request->send(500); }
    } else { request->send(400); }
  });

  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
    if (mp3->isRunning()) { mp3->stop(); nowPlaying = "Hiçbiri"; }
    if (request->hasParam("file")) {
      String path = "/" + request->getParam("file")->value();
      if (SD.remove(path)) { request->send(200); } else { request->send(500); }
    } else { request->send(400); }
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    if (mp3->isRunning()) { mp3->stop(); }
    nowPlaying = "Hiçbiri";
    request->send(200, "text/plain", "Durduruldu.");
  });
  
  server.on("/volume", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("level")){
      out->SetGain(request->getParam("level")->value().toFloat());
      request->send(200);
    } else {
      request->send(400);
    }
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    String sdStatus = sdCardReady ? "Hazır" : "Takılı Değil / Hata";
    if (!mp3->isRunning() && nowPlaying != "Hiçbiri") {
      nowPlaying = "Hiçbiri";
    }
    request->send(200, "application/json", "{\"sd_status\":\"" + sdStatus + "\", \"now_playing\":\"" + nowPlaying + "\"}");
  });

  server.begin();
  Serial.println("Web Sunucusu başlatıldı.");
} // setup() fonksiyonunun kapanış parantezi

void loop() {
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
    }
  }
}