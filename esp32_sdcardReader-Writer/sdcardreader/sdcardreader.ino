#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SD.h>

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

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 MP3 Çalar</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    /* --- KARANLIK TEMA BAŞLANGICI --- */
    body { 
      font-family: Arial, Helvetica, sans-serif; 
      background-color: #121212; /* Koyu arka plan */
      color: #e0e0e0; /* Açık renk yazı */
      margin: 0; 
      padding: 20px;
    }
    .container { 
      max-width: 800px; 
      margin: auto; 
      background: #1e1e1e; /* Konteyner için biraz daha açık bir koyu renk */
      padding: 20px; 
      border-radius: 8px; 
      box-shadow: 0 2px 8px rgba(0,0,0,0.5); /* Gölgeyi belirginleştirdik */
      border: 1px solid #333; /* İnce bir çerçeve */
    }
    h1, h2 { 
      color: #bb86fc; /* Morumsu bir tema rengi */
      border-bottom: 2px solid #bb86fc;
      padding-bottom: 5px;
    }
    .file-list { 
      list-style: none; 
      padding: 0; 
    }
    .file-item { 
      display: flex; 
      align-items: center; 
      justify-content: space-between; 
      padding: 12px; 
      border-bottom: 1px solid #333; /* Ayırıcı çizgi rengi */
    }
    .file-item:last-child { 
      border-bottom: none; 
    }
    .file-name { 
      font-weight: bold; 
      font-size: 16px;
    }
    .btn { 
      padding: 8px 15px; 
      border: none; 
      border-radius: 5px; 
      color: white; 
      cursor: pointer; 
      text-decoration: none; 
      font-size: 14px;
      transition: opacity 0.2s;
    }
    .btn:hover {
      opacity: 0.8;
    }
    .btn-play { background-color: #03dac6; color: #121212; font-weight: bold; } /* Canlı turkuaz */
    .btn-delete { background-color: #cf6679; margin-left: 10px; } /* Yumuşak kırmızı */
    .btn-stop { background-color: #fdd835; color: black; display: block; width: 100px; text-align: center; margin: 20px auto; }
    .upload-form { 
      margin-top: 30px; 
      padding: 20px; 
      background: #2a2a2a; /* Form arka planı */
      border-radius: 8px; 
    }
    input[type="submit"] { 
      background-color: #bb86fc; /* Tema rengiyle uyumlu */
      color: white; 
      padding: 10px 20px; 
      border: none; 
      border-radius: 5px; 
      cursor: pointer;
      transition: background-color 0.2s;
    }
    input[type="submit"]:hover { 
      background-color: #a362fa; 
    }
    #status { 
      margin-top: 15px; 
      font-style: italic; 
      color: #aaa; /* Durum yazısı rengi */
    }
    /* Dosya seçme butonunu tema için özelleştirme */
    input[type="file"]::file-selector-button {
      background-color: #333;
      color: #e0e0e0;
      border: 1px solid #555;
      padding: 8px 12px;
      border-radius: 5px;
      cursor: pointer;
      transition: background-color 0.2s;
    }
    input[type="file"]::file-selector-button:hover {
      background-color: #444;
    }
    /* --- KARANLIK TEMA SONU --- */
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 Web MP3 Çalar</h1>
    <h2>SD Karttaki Dosyalar</h2>
    <ul class="file-list">
      %FILE_LIST%
    </ul>
    <a href="#" onclick="stopPlayback()" class="btn btn-stop">Durdur</a>
    <div class="upload-form">
      <h2>Yeni Ses Dosyası Yükle</h2>
      <form method="POST" action="" enctype="multipart/form-data">
        <input type="file" name="upload">
        <input type="submit" value="Yükle">
      </form>
      <div id="status"></div>
    </div>
  </div>
  <script>
    function playFile(filename) {
      document.getElementById('status').innerHTML = 'Çalınıyor: ' + filename;
      fetch('/play?file=' + filename);
    }
    function deleteFile(filename) {
      if (confirm(filename + ' dosyasını silmek istediğinizden emin misiniz?')) {
        document.getElementById('status').innerHTML = 'Siliniyor: ' + filename;
        fetch('/delete?file=' + filename)
          .then(response => {
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
        document.getElementById('status').innerHTML = 'Durduruldu.';
        fetch('/stop');
    }
  </script>
</body>
</html>
)rawliteral";
String listFiles() {
  String fileList = "";
  File root = SD.open("/");
  if (!root) {
    return "<p>SD Kart kök dizini açılamadı!</p>";
  }
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String fileName = String(file.name());
      if (fileName.endsWith(".mp3") || fileName.endsWith(".MP3")) {
          fileList += "<li class='file-item'><span class='file-name'>" + fileName + "</span>";
          fileList += "<div><a href='#' onclick='playFile(\"" + fileName + "\")' class='btn btn-play'>Çal</a>";
          fileList += "<a href='#' onclick='deleteFile(\"" + fileName + "\")' class='btn btn-delete'>Sil</a></div></li>";
      }
    }
    file = root.openNextFile();
  }
  root.close();
  if (fileList == "") {
    return "<p>SD Kartta hiç .mp3 dosyası bulunamadı.</p>";
  }
  return fileList;
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  String path = "/" + filename;
  if (index == 0) {
    Serial.printf("Yükleme başladı: %s\n", filename.c_str());
    uploadFile = SD.open(path, FILE_WRITE);
  }
  if(len){
    uploadFile.write(data, len);
  }
  if (final) {
    uploadFile.close();
    Serial.printf("Yükleme tamamlandı: %s, Boyut: %u byte\n", filename.c_str(), index + len);
    request->send(200, "text/plain", "Dosya Yüklendi");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- ESP32 MP3 Çalar Başlatılıyor (Yeni Kod) ---");

  if (!SD.begin(SD_CS)) {
    Serial.println("SD Kart başlatılamadı!");
    return;
  }
  Serial.println("SD Kart başarıyla başlatıldı.");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nBağlantı başarılı!");
  Serial.print("ESP32 IP Adresi: ");
  Serial.println(WiFi.localIP());

  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  out->SetGain(1.0); // 0.0 - 4.0 arası ses ayarı
  mp3 = new AudioGeneratorMP3();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String fileList = listFiles();
    String html = index_html;
    html.replace("%FILE_LIST%", fileList);
    request->send(200, "text/html", html);
  });

  server.on("", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200);
  }, handleUpload);

  server.on("/play", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("file")) {
      if (mp3->isRunning()) { mp3->stop(); }
      if (file) { delete file; file = nullptr; }
      
      String fileName = request->getParam("file")->value();
      String path = "/" + fileName;
      
      file = new AudioFileSourceSD(path.c_str());
      mp3->begin(file, out);
      Serial.printf("Çalınıyor: %s\n", path.c_str());
      request->send(200, "text/plain", "Çalınıyor...");
    } else {
      request->send(400, "text/plain", "Dosya adı belirtilmedi.");
    }
  });

  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("file")) {
      if (mp3->isRunning()) { mp3->stop(); }

      String fileName = request->getParam("file")->value();
      String path = "/" + fileName;

      if (SD.remove(path)) {
        Serial.printf("Silindi: %s\n", path.c_str());
        request->send(200, "text/plain", "Dosya silindi.");
      } else {
        request->send(500, "text/plain", "Dosya silinemedi.");
      }
    } else {
      request->send(400, "text/plain", "Dosya adı belirtilmedi.");
    }
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    if (mp3->isRunning()) {
      mp3->stop();
      Serial.println("Durduruldu.");
      request->send(200, "text/plain", "Durduruldu.");
    } else {
      request->send(200, "text/plain", "Zaten çalmıyor.");
    }
  });

  server.begin();
  Serial.println("Web Sunucusu başlatıldı.");
}

void loop() {
  if (mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
      Serial.println("MP3 bitti.");
    }
  }
}