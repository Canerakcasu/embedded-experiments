#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "AudioGeneratorMP3.h"
#include "AudioFileSourceSD.h"
#include "AudioOutputI2S.h"

const char* ssid = "Ents_Test";
const char* password = "12345678";

#define SD_CS    5
#define I2S_BCLK 26
#define I2S_LRC  25
#define I2S_DOUT 22

AsyncWebServer server(80);
File uploadFile;
AudioGeneratorMP3 *mp3;
AudioFileSourceSD *file;
AudioOutputI2S *out;
String nowPlaying = "Hiçbiri";
bool sdCardInitialized = false;

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

String listFiles() {
  String fileList = "";
  if (!sdCardInitialized) { return "<p>SD Kart başlatılmadı.</p>"; }
  File root = SD.open("/");
  if (!root) { return "<p>SD Kart kök dizini açılamadı! Lütfen kartı formatlayın.</p>"; }
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      const char* fileNameWithPath = file.name(); 
      String tempStrCheck = String(fileNameWithPath);
      if (tempStrCheck.endsWith(".mp3") || tempStrCheck.endsWith(".MP3")) {
          const char* fileNameOnly = fileNameWithPath + 1; 
          fileList += "<li class='file-item'><span class='file-name'>" + String(fileNameOnly) + "</span>";
          fileList += "<div><a href='#' onclick='playFile(\"" + String(fileNameOnly) + "\")' class='btn btn-play'>Çal</a>";
          fileList += "<a href='#' onclick='renameFile(\"" + String(fileNameOnly) + "\")' class='btn btn-rename'>Adlandır</a>";
          fileList += "<a href='#' onclick='deleteFile(\"" + String(fileNameOnly) + "\")' class='btn btn-delete'>Sil</a></div></li>";
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
    if (mp3 && mp3->isRunning()) { mp3->stop(); }
    if (file) { delete file; file = nullptr; }
    nowPlaying = "Hiçbiri";
    uploadFile = SD.open(path, FILE_WRITE);
  }
  if(uploadFile && len){ uploadFile.write(data, len); }
  if (final) {
    if(uploadFile) uploadFile.close();
    request->send(200, "text/plain", "Dosya Başarıyla Yüklendi!");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- ESP32 Gelişmiş MP3 Çalar (Nihai Sürüm) ---");

  if (SD.begin(SD_CS)) {
    sdCardInitialized = true;
    Serial.println("SD Kart (VSPI) başarıyla başlatıldı.");
  } else {
    sdCardInitialized = false;
    Serial.println("SD Kart (VSPI) başlatılamadı!");
  }

  WiFi.begin(ssid, password);
  Serial.print("Wi-Fi'ye bağlanılıyor...");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
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

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){ request->send(200); }, handleUpload);

  server.on("/play", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("file") && sdCardInitialized) {
      if (mp3->isRunning()) { mp3->stop(); }
      if (file) { delete file; file = nullptr; }
      nowPlaying = request->getParam("file")->value();
      String path = "/" + nowPlaying;
      file = new AudioFileSourceSD(path.c_str());
      if (!file->isOpen()) {
        nowPlaying = "HATA: Dosya açılamadı";
        delete file; file = nullptr;
        request->send(404);
        return;
      }
      if (mp3->begin(file, out)) {
        request->send(200);
      } else {
        nowPlaying = "HATA: MP3 başlatılamadı.";
        if (file) { delete file; file = nullptr; }
        request->send(500);
      }
    } else { request->send(400); }
  });

  server.on("/rename", HTTP_GET, [](AsyncWebServerRequest *request){
    if (mp3->isRunning()) { mp3->stop(); }
    if (file) { delete file; file = nullptr; }
    nowPlaying = "Hiçbiri";
    if (request->hasParam("old") && request->hasParam("new")) {
      String oldPath = "/" + request->getParam("old")->value();
      String newPath = "/" + request->getParam("new")->value();
      if (SD.rename(oldPath, newPath)) { request->send(200); } else { request->send(500); }
    } else { request->send(400); }
  });

  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
    if (mp3->isRunning()) { mp3->stop(); }
    if (file) { delete file; file = nullptr; }
    nowPlaying = "Hiçbiri";
    if (request->hasParam("file")) {
      String path = "/" + request->getParam("file")->value();
      if (SD.remove(path)) { request->send(200); } else { request->send(500); }
    } else { request->send(400); }
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    if (mp3->isRunning()) { mp3->stop(); }
    if (file) { delete file; file = nullptr; }
    nowPlaying = "Hiçbiri";
    request->send(200);
  });
  
  server.on("/volume", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("level")){
      out->SetGain(request->getParam("level")->value().toFloat());
      request->send(200);
    } else { request->send(400); }
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    String sdStatus;
    File root = SD.open("/");
    if (root) {
      sdStatus = "Hazır";
      root.close();
    } else {
      sdStatus = "Takılı Değil / Hata";
    }
    if (!mp3->isRunning() && nowPlaying != "Hiçbiri") {
      nowPlaying = "Hiçbiri";
    }
    request->send(200, "application/json", "{\"sd_status\":\"" + sdStatus + "\", \"now_playing\":\"" + nowPlaying + "\"}");
  });

  server.begin();
  Serial.println("Web Sunucusu başlatıldı.");
}

void loop() {
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
      if (file) {
        delete file;
        file = nullptr;
      }
      nowPlaying = "Hiçbiri";
    }
  }
}