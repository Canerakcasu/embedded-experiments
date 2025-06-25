#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "AudioGeneratorMP3.h"
#include "AudioFileSourceSD.h"
#include "AudioOutputI2S.h"

// --- KULLANICI AYARLARI ---
const char* ssid = "Ents_Test";
const char* password = "12345678";

// --- PIN TANIMLAMALARI ---
#define SD_CS   5
#define I2S_BCLK 26
#define I2S_LRC  25
#define I2S_DOUT 22

// --- GLOBAL NESNELER & DEĞİŞKENLER ---
AsyncWebServer server(80);
File uploadFile;
AudioGeneratorMP3 *mp3;
AudioFileSourceSD *file;
AudioOutputI2S *out;
String nowPlaying = "None";
bool sdCardInitialized = false;

// --- Çekirdekler Arası İletişim İçin ---
enum CommandType { CMD_NONE, CMD_PLAY, CMD_STOP, CMD_RENAME, CMD_DELETE };
CommandType command = CMD_NONE;
String commandParam1 = "";
String commandParam2 = "";

SemaphoreHandle_t commandMutex;
SemaphoreHandle_t nowPlayingMutex;

// --- WEB ARAYÜZÜ (HTML) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 Audio Player</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background-color: #121212; color: #e0e0e0; margin: 0; padding: 20px;}
    .container { max-width: 800px; margin: auto; background: #1e1e1e; padding: 20px; border-radius: 8px; box-shadow: 0 2px 8px rgba(0,0,0,0.5); border: 1px solid #333; }
    h1, h2 { color: #FFFFFF; border-bottom: 2px solid #555; padding-bottom: 10px; }
    h1 { text-align: center; border-bottom: none; }
    .status-panel { background-color: #2a2a2a; padding: 15px; border-radius: 8px; margin-bottom: 20px; text-align: center;}
    .status-panel p { margin: 8px 0; font-size: 16px; }
    .status-panel .label { font-weight: bold; color: #aaa; }
    .status-panel .value { color: #03dac6; font-weight: bold; }
    .file-list { list-style: none; padding: 0; }
    .file-item { display: flex; align-items: center; justify-content: space-between; padding: 12px; border-bottom: 1px solid #333; }
    .file-item:last-child { border-bottom: none; }
    .file-name { font-weight: bold; font-size: 16px; }
    .btn { padding: 8px 15px; border: none; border-radius: 5px; color: white; cursor: pointer; text-decoration: none; font-size: 14px; transition: opacity 0.2s; margin-left: 10px; }
    .btn:hover { opacity: 0.8; }
    .btn-play { background-color: #1D744D; color: white; font-weight: bold; margin-left: 0; }
    .btn-rename { background-color: #D4AC0D; }
    .btn-delete { background-color: #B03A2E; }
    .btn-stop { background-color: #616161; color: white; padding: 10px 30px; }
    .volume-control { display: flex; align-items: center; justify-content: center; margin: 25px 0; }
    .volume-control label { font-size: 16px; margin-right: 10px; }
    .volume-control input[type="range"] { width: 50%; cursor: pointer; }
    .upload-form { margin-top: 30px; padding: 20px; background: #2a2a2a; border-radius: 8px; }
    input[type="submit"] { background-color: #424242; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; transition: background-color 0.2s; }
    input[type="submit"]:hover { background-color: #555555; }
    #status { margin-top: 15px; font-style: italic; color: #aaa; text-align: center; }
    input[type="file"]::file-selector-button { background-color: #333; color: #e0e0e0; border: 1px solid #555; padding: 8px 12px; border-radius: 5px; cursor: pointer; transition: background-color 0.2s; }
    input[type="file"]::file-selector-button:hover { background-color: #444; }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 Web Audio Player</h1>
    <div class="status-panel">
      <h2>Status Panel</h2>
      <p><span class="label">SD Card:</span> <span id="sd-status" class="value">Unknown</span></p>
      <p><span class="label">Now Playing:</span> <span id="now-playing" class="value">None</span></p>
      <a href="#" onclick="stopPlayback()" class="btn btn-stop" style="margin-top: 15px;">Stop Playback</a>
    </div>
    <div class="volume-control">
      <label for="volume">Volume</label>
      <input type="range" id="volume" name="volume" min="0.0" max="4.0" step="0.1" value="1.0" onchange="setVolume(this.value)">
    </div>
    <h2>Files on SD Card</h2>
    <ul class="file-list">
      %FILE_LIST%
    </ul>
    <div class="upload-form">
      <h2>Upload New Audio File</h2>
      <form id="upload-form" method="POST" enctype="multipart/form-data">
        <input type="file" name="upload" accept=".mp3">
        <input type="submit" value="Upload">
      </form>
    </div>
    <div id="status"></div>
  </div>
  <script>
    function playFile(filename) {
      document.getElementById('status').innerHTML = 'Sending play request for: ' + filename;
      fetch('/play?file=' + encodeURIComponent(filename)).then(() => setTimeout(updateStatus, 500));
    }
    function renameFile(oldName) {
      let newName = prompt("Enter a new name for the file:", oldName);
      if (newName && newName !== "" && newName !== oldName) {
        if (!newName.toLowerCase().endsWith('.mp3')) { newName += '.mp3'; }
        document.getElementById('status').innerHTML = 'Renaming ' + oldName + ' to ' + newName + '...';
        fetch(`/rename?old=${encodeURIComponent(oldName)}&new=${encodeURIComponent(newName)}`)
          .then(response => {
            if (response.ok) {
              document.getElementById('status').innerHTML = 'File successfully renamed.';
              setTimeout(() => { window.location.reload(); }, 1000);
            } else {
              document.getElementById('status').innerHTML = 'Error: Could not rename file.';
            }
          });
      } else {
        document.getElementById('status').innerHTML = 'Rename cancelled.';
      }
    }
    function deleteFile(filename) {
      if (confirm('Are you sure you want to delete ' + filename + '?')) {
        document.getElementById('status').innerHTML = 'Deleting ' + filename + '...';
        fetch('/delete?file=' + encodeURIComponent(filename)).then(response => {
          if (response.ok) {
            document.getElementById('status').innerHTML = filename + ' was successfully deleted.';
            setTimeout(() => { window.location.reload(); }, 1000);
          } else {
            document.getElementById('status').innerHTML = 'Error: Could not delete file.';
          }
        });
      }
    }
    function stopPlayback(){
      document.getElementById('status').innerHTML = 'Sending stop request...';
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
        .catch(error => console.error('Error updating status:', error));
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
        statusDiv.innerHTML = 'Please select a file first.';
        return;
      }
      statusDiv.innerHTML = 'Uploading, please wait...';
      fetch('/upload', { method: 'POST', body: formData })
      .then(response => response.text())
      .then(result => {
        statusDiv.innerHTML = result;
        form.reset();
        setTimeout(() => { window.location.reload(); }, 2000);
      })
      .catch(error => { statusDiv.innerHTML = 'Upload Error: ' + error; });
    });
  </script>
</body>
</html>
)rawliteral";

String listFiles() {
  String fileList = "";
  if (!sdCardInitialized) { return "<p>SD Card not initialized.</p>"; }
  
  // DEĞİŞİKLİK: Kök dizin yerine /mp3 klasörünü aç
  File root = SD.open("/mp3");
  if (!root) {
    Serial.println("Failed to open /mp3 directory");
    return "<p>Could not open /mp3 directory!</p>";
  }
  
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String tempStrCheck = String(file.name());
      if (tempStrCheck.endsWith(".mp3") || tempStrCheck.endsWith(".MP3")) {
        const char* displayName = file.name(); 
        fileList += "<li class='file-item'><span class='file-name'>" + String(displayName) + "</span>";
        fileList += "<div><a href='#' onclick='playFile(\"" + String(displayName) + "\")' class='btn btn-play'>Play</a>";
        fileList += "<a href='#' onclick='renameFile(\"" + String(displayName) + "\")' class='btn btn-rename'>Rename</a>";
        fileList += "<a href='#' onclick='deleteFile(\"" + String(displayName) + "\")' class='btn btn-delete'>Delete</a></div></li>";
      }
    }
    file = root.openNextFile();
  }
  
  root.close();
  if (fileList == "") { return "<p>No .mp3 files found in /mp3 folder.</p>"; }
  return fileList;
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  xSemaphoreTake(commandMutex, portMAX_DELAY);
  Serial.printf("[Core 0] Handling file upload chunk for %s\n", filename.c_str());
  
  // DEĞİŞİKLİK: Dosya yolunu /mp3 klasörünün içine ayarla
  String path = "/mp3/" + filename;

  if (index == 0) {
    if (mp3 && mp3->isRunning()) mp3->stop();
    if(file) delete file;
    file = nullptr;
    xSemaphoreTake(nowPlayingMutex, portMAX_DELAY);
    nowPlaying = "None";
    xSemaphoreGive(nowPlayingMutex);
    uploadFile = SD.open(path, FILE_WRITE);
  }
  if (uploadFile && len) {
    uploadFile.write(data, len);
  }
  if (final) {
    if(uploadFile) uploadFile.close();
    request->send(200, "text/plain", "File Uploaded Successfully!");
    Serial.printf("[Core 0] Finished file upload for %s\n", filename.c_str());
  }
  xSemaphoreGive(commandMutex);
}

void AudioFileTask(void *pvParameters) {
  Serial.printf("Audio and File Task started on core %d\n", xPortGetCoreID());

  for (;;) {
    CommandType localCommand = CMD_NONE;
    String localParam1, localParam2;

    xSemaphoreTake(commandMutex, portMAX_DELAY);
    if (command != CMD_NONE) {
      localCommand = command;
      localParam1 = commandParam1;
      localParam2 = commandParam2;
      command = CMD_NONE;
    }
    xSemaphoreGive(commandMutex);
    
    if (localCommand != CMD_NONE) {
      if (mp3 && mp3->isRunning()) mp3->stop();
      if (file) delete file;
      file = nullptr;

      xSemaphoreTake(nowPlayingMutex, portMAX_DELAY);
      nowPlaying = "None";
      xSemaphoreGive(nowPlayingMutex);
        
      if (localCommand == CMD_PLAY) {
        // DEĞİŞİKLİK: Dosya yolunu /mp3 klasörünün içine ayarla
        String path = "/mp3/" + localParam1;
        Serial.printf("[Core 0] Processing PLAY command. Attempting to open: %s\n", path.c_str());
        file = new AudioFileSourceSD(path.c_str());
        if (!file || !file->isOpen()) {
            Serial.printf("[Core 0] Error: File not found at path: %s\n", path.c_str());
            xSemaphoreTake(nowPlayingMutex, portMAX_DELAY);
            nowPlaying = "ERROR: File not found";
            xSemaphoreGive(nowPlayingMutex);
            if(file) delete file; 
            file = nullptr;
        } else {
            if (!mp3->begin(file, out)) {
                Serial.printf("[Core 0] Error: Could not start MP3 decoder for: %s\n", path.c_str());
                xSemaphoreTake(nowPlayingMutex, portMAX_DELAY);
                nowPlaying = "ERROR: Could not start MP3";
                xSemaphoreGive(nowPlayingMutex);
                if(file) delete file;
                file = nullptr;
            } else {
                 Serial.printf("[Core 0] Playback started for: %s\n", path.c_str());
                 xSemaphoreTake(nowPlayingMutex, portMAX_DELAY);
                 nowPlaying = localParam1;
                 xSemaphoreGive(nowPlayingMutex);
            }
        }
      } else if (localCommand == CMD_STOP) {
          Serial.printf("[Core 0] Processing STOP command.\n");
      } else if (localCommand == CMD_RENAME) {
          // DEĞİŞİKLİK: Dosya yollarını /mp3 klasörünün içine ayarla
          String oldPath = "/mp3/" + localParam1;
          String newPath = "/mp3/" + localParam2;
          Serial.printf("[Core 0] Processing RENAME command: from %s to %s\n", oldPath.c_str(), newPath.c_str());
          SD.rename(oldPath, newPath);
      } else if (localCommand == CMD_DELETE) {
          // DEĞİŞİKLİK: Dosya yolunu /mp3 klasörünün içine ayarla
          String path = "/mp3/" + localParam1;
          Serial.printf("[Core 0] Processing DELETE command for: %s\n", path.c_str());
          SD.remove(path);
      }
    }

    if (mp3 && mp3->isRunning()) {
      if (!mp3->loop()) {
        mp3->stop();
        if (file) delete file;
        file = nullptr;
        xSemaphoreTake(nowPlayingMutex, portMAX_DELAY);
        nowPlaying = "None";
        xSemaphoreGive(nowPlayingMutex);
        Serial.printf("[Core 0] Playback finished.\n");
      }
    }
    vTaskDelay(10);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf("Setup function running on core %d\n", xPortGetCoreID());

  commandMutex = xSemaphoreCreateMutex();
  nowPlayingMutex = xSemaphoreCreateMutex();

  if (SD.begin(SD_CS)) {
    sdCardInitialized = true;
    Serial.println("SD Card (VSPI) initialized successfully.");
    
    // DEĞİŞİKLİK: /mp3 klasörü yoksa oluştur
    if (!SD.exists("/mp3")) {
      Serial.println("Creating /mp3 directory...");
      if (SD.mkdir("/mp3")) {
        Serial.println("Directory created.");
      } else {
        Serial.println("mkdir failed");
      }
    }

  } else {
    sdCardInitialized = false;
    Serial.println("SD Card (VSPI) failed to initialize!");
  }

  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  out->SetGain(1.0);
  mp3 = new AudioGeneratorMP3();

  xTaskCreatePinnedToCore(
      AudioFileTask,
      "AudioFile Task",
      10000,
      NULL,
      2,
      NULL,
      0);

  WiFi.begin(ssid, password);
  Serial.printf("[Core %d] Connecting to Wi-Fi...\n", xPortGetCoreID());
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf("\n[Core %d] Connected successfully!\n", xPortGetCoreID());
  Serial.printf("[Core %d] ESP32 IP Address: http://", xPortGetCoreID());
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.printf("[Core 1] Handling GET request for /\n", xPortGetCoreID());
    String htmlPage = index_html;
    htmlPage.replace("%FILE_LIST%", listFiles());
    request->send(200, "text/html", htmlPage);
  });
  
  server.on("/upload", HTTP_POST, 
    [](AsyncWebServerRequest *request){
       request->send(200);
    },
    handleUpload
  );
  
  server.on("/play", HTTP_GET, [](AsyncWebServerRequest *request){
    xSemaphoreTake(commandMutex, portMAX_DELAY);
    Serial.printf("[Core 1] Received PLAY command for %s\n", request->getParam("file")->value().c_str());
    command = CMD_PLAY;
    commandParam1 = request->getParam("file")->value();
    xSemaphoreGive(commandMutex);
    request->send(200);
  });

  server.on("/rename", HTTP_GET, [](AsyncWebServerRequest *request){
    xSemaphoreTake(commandMutex, portMAX_DELAY);
    Serial.printf("[Core 1] Received RENAME command.\n");
    command = CMD_RENAME;
    commandParam1 = request->getParam("old")->value();
    commandParam2 = request->getParam("new")->value();
    xSemaphoreGive(commandMutex);
    request->send(200);
  });

  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
    xSemaphoreTake(commandMutex, portMAX_DELAY);
    Serial.printf("[Core 1] Received DELETE command for %s\n", request->getParam("file")->value().c_str());
    command = CMD_DELETE;
    commandParam1 = request->getParam("file")->value();
    xSemaphoreGive(commandMutex);
    request->send(200);
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    xSemaphoreTake(commandMutex, portMAX_DELAY);
    Serial.printf("[Core 1] Received STOP command.\n");
    command = CMD_STOP;
    xSemaphoreGive(commandMutex);
    request->send(200);
  });
  
  server.on("/volume", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.printf("[Core 1] Received VOLUME command.\n");
    if(request->hasParam("level")){
      out->SetGain(request->getParam("level")->value().toFloat());
    }
    request->send(200);
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    String sdStatus;
    File rootTest = SD.open("/");
    if (rootTest) {
      sdStatus = "Ready";
      rootTest.close();
    } else {
      sdStatus = "Not Detected / Error";
    }
    
    String nowPlayingCopy;
    xSemaphoreTake(nowPlayingMutex, portMAX_DELAY);
    nowPlayingCopy = nowPlaying;
    xSemaphoreGive(nowPlayingMutex);
    
    request->send(200, "application/json", "{\"sd_status\":\"" + sdStatus + "\", \"now_playing\":\"" + nowPlayingCopy + "\"}");
  });

  server.begin();
  Serial.printf("[Core 1] Web Server started.\n");
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}
