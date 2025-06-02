// --- BÖLÜM 1: KÜTÜPHANE DAHİL ETME ---
#include <Arduino.h>         // Temel Arduino fonksiyonları için.
#include <WiFi.h>            // ESP32 Wi-Fi bağlantısı için.
#include <WebServer.h>       // ESP32 üzerinde web sunucusu oluşturmak için.
#include <ArduinoJson.h>     // JSON verilerini işlemek (oluşturmak ve ayrıştırmak) için. Web ve MQTT için kullanılır.
#include <Adafruit_NeoPixel.h> // NeoPixel LED şeritlerini kontrol etmek için.
#include <HardwareSerial.h>  // Donanımsal seri portları kullanmak için (DFPlayer için).
#include "DFRobotDFPlayerMini.h" // DFPlayer Mini MP3 modülünü kontrol etmek için.
#include <IRremote.h>        // Kızılötesi (IR) uzaktan kumanda sinyallerini almak için.
#include <AiEsp32RotaryEncoder.h> // Rotary encoder'ı (döner kodlayıcı) okumak için.
#include <Preferences.h>     // ESP32'nin flash belleğinde kalıcı veri saklamak için (bu kodda aktif kullanımı görünmüyor ama dahil edilmiş).
#include <PubSubClient.h>    // MQTT (Message Queuing Telemetry Transport) protokolü ile iletişim için.

// --- BÖLÜM 2: GLOBAL SABİTLER VE DEĞİŞKENLER ---

// WiFi Ayarları
const char* ssid = "Ents_Test";       // Kendi Wi-Fi ağ adınızı yazın
const char* password = "12345678"; // Kendi Wi-Fi şifrenizi yazın
WebServer server(80); // Web sunucusunu 80 portunda oluşturur.

// MQTT Ayarları (EKSİK TANIMLAMALAR - BUNLARI EKLEMELİSİNİZ)
const char* mqtt_server = "YOUR_MQTT_BROKER_IP_OR_HOSTNAME"; // MQTT broker adresinizi yazın
const int mqtt_port = 1883;                                  // MQTT broker portu (genellikle 1883)
const char* mqtt_client_id = "esp32AdvancedControl";         // ESP32 için benzersiz MQTT istemci ID'si
WiFiClient espClient;                 // MQTT için Wi-Fi istemci nesnesi
PubSubClient mqttClient(espClient);   // MQTT istemci nesnesi
unsigned long lastMqttPublishTime = 0; // Son MQTT yayın zamanı
unsigned long mqttPublishInterval = 5000; // MQTT yayın aralığı (milisaniye) - örneğin 5 saniye

// NeoPixel LED Ayarları
#define LED_PIN       2    // NeoPixel LED şeridinin DATA pininin bağlı olduğu ESP32 pini.
#define NUM_LEDS      15   // LED şeridindeki toplam LED sayısı.
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800); // NeoPixel nesnesi oluşturulur.
int globalBrightness = 120; // LED'lerin genel parlaklık değeri (0-255).
uint8_t currentLedR = 255, currentLedG = 0, currentLedB = 0; // Mevcut LED rengi (RGB).
bool ledsOn = true; // LED'lerin açık/kapalı durumu.

// Rotary Encoder Ayarları
#define ROTARY_ENCODER_A_PIN      33 // Encoder CLK pini.
#define ROTARY_ENCODER_B_PIN      27 // Encoder DT pini.
#define ROTARY_ENCODER_BUTTON_PIN 14 // Encoder SW (buton) pini.
// Encoder nesnesi: A pini, B pini, Buton pini, VCC pini (-1 ise dahili pull-up), bir tık başına adım.
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, -1, 4);
int currentEncoderMode = 0; // Rotary encoder'ın mevcut çalışma modu (0: Parlaklık, 1: Renk, 2: Ses).
const char* encoderModes[] = {"LED Brightness", "LED Color Select", "DFPlayer Volume"}; // Mod isimleri.
// Önceden tanımlanmış LED renkleri (NeoPixel formatında).
uint32_t predefinedColors[] = {
    strip.Color(255, 0, 0), strip.Color(0, 255, 0), strip.Color(0, 0, 255),   // Kırmızı, Yeşil, Mavi
    strip.Color(255, 255, 0), strip.Color(0, 255, 255), strip.Color(255, 0, 255), // Sarı, Camgöbeği, Macenta
    strip.Color(255, 255, 255) // Beyaz
};
const int num_predefined_colors = sizeof(predefinedColors) / sizeof(predefinedColors[0]); // Ön tanımlı renk sayısı.
int currentPredefinedColorIndex = 0; // Mevcut seçili ön tanımlı renk indeksi.

// DFPlayer Mini Ayarları
HardwareSerial myDFPlayerSerial(2); // DFPlayer için donanımsal seri port 2'yi kullan (RX2, TX2).
DFRobotDFPlayerMini myDFPlayer;     // DFPlayer Mini nesnesi.
bool dfPlayerAvailable = false;     // DFPlayer modülünün kullanılabilir olup olmadığını belirtir.
bool sdCardOnline = false;          // SD kartın takılı ve çalışır durumda olup olmadığını belirtir.
int currentDFPlayerVolume = 20;     // Mevcut ses seviyesi (0-30).
int previousDFPlayerVolume = 20;    // Sessize almadan önceki ses seviyesini saklamak için.
bool dfPlayerMuted = false;         // DFPlayer'ın sessizde olup olmadığını belirtir.
int currentTrackNumber = 0;         // Çalmakta olan parçanın numarası (0 ise durmuş).
const int totalTracks = 5;          // SD karttaki toplam parça sayısı (kendinize göre ayarlayın).
// Parça isimleri (web arayüzü ve MQTT'de göstermek için).
const char* trackNames[totalTracks + 1] = {
    "Unknown/Stopped", "Track 1 (e.g. Intro)", "Track 2 (e.g. Birds)", 
    "Track 3 (e.g. Melody)", "Track 4 (e.g. Effect)", "Track 5 (e.g. Ambiance)"
};
// DFPlayer'ın ESP32'ye bağlantı pinleri (HardwareSerial için).
#define DFPLAYER_RX_PIN 16 // DFPlayer TX -> ESP32 RX2
#define DFPLAYER_TX_PIN 17 // DFPlayer RX -> ESP32 TX2

// Harici Buton Ayarı
#define EXTERNAL_BUTTON_D12_PIN 12 // Harici bir buton için kullanılacak pin.

// IR Alıcı Ayarları
#define IR_RECEIVE_PIN 18  // IR alıcısının DATA pininin bağlı olduğu ESP32 pini.
int irDpadMode = 0;        // IR kumanda D-Pad'inin hangi işlevi kontrol edeceği (0: LED, 1: DFPlayer).
const char* irDpadModes[] = {"LED D-Pad Control", "DFPlayer D-Pad Control"}; // D-Pad mod isimleri.

// IR Aksiyon İsimleri (String sabitleri, komutları tanımlamak için)
const char* IR_ACTION_MUTE = "mute";
const char* IR_ACTION_POWER = "power";
const char* IR_ACTION_MUSIC = "music_btn";
const char* IR_ACTION_PLAYSTOP = "play_stop";
const char* IR_ACTION_UP = "up";
const char* IR_ACTION_RIGHT = "right";
const char* IR_ACTION_LEFT = "left";
const char* IR_ACTION_DOWN = "down";
const char* IR_ACTION_ENTER = "enter";
const char* IR_ACTION_VOL_UP = "vol_up";
const char* IR_ACTION_VOL_DOWN = "vol_down";
const char* IR_ACTION_FFWD = "ffwd"; // İleri sarma (DFPlayer için sonraki parça olarak kullanılabilir)
const char* IR_ACTION_REW = "rew";   // Geri sarma (DFPlayer için önceki parça olarak kullanılabilir)
const char* IR_ACTION_NEXT_TRACK = "next_track"; // Sonraki parça (bazı kumandalarda özel tuş)
const char* IR_ACTION_BACK_TRACK = "back_track"; // Önceki parça (bazı kumandalarda özel tuş)

// JSON Dokümanları (Veri yapılandırma için önceden bellek ayrılır)
StaticJsonDocument<1024> jsonDocHttp; // Web sunucusu yanıtları için JSON dokümanı.
StaticJsonDocument<512> jsonDocMqtt;  // MQTT mesajları için JSON dokümanı (bu kodda doğrudan kullanılmıyor ama ileride gerekebilir).

// Web Arayüzü HTML Kodu
// PROGMEM: Bu büyük string'in ESP32'nin flash belleğinde saklanmasını sağlar, RAM'i tüketmez.
// R"rawliteral(...)rawliteral"; : C++ raw string literal, HTML içindeki özel karakterlerle uğraşmayı kolaylaştırır.
const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><title>ESP32 Advanced Control</title><meta name="viewport" content="width=device-width, initial-scale=1"><style>body{font-family:'Roboto',Arial,sans-serif;background-color:#121212;color:#e0e0e0;padding:15px;display:flex;flex-direction:column;align-items:center;margin:0}h1{color:#e53935;text-align:center;text-shadow:1px 1px 3px #000;margin-bottom:25px}h3{color:#ff7961;text-align:left;border-bottom:2px solid #b71c1c;padding-bottom:8px;margin-top:20px;margin-bottom:15px}.grid-container{display:grid;grid-template-columns:repeat(auto-fit, minmax(320px, 1fr));gap:25px;width:100%;max-width:1200px}.card{background-color:#1e1e1e;border:1px solid #c21807;border-radius:10px;padding:20px;box-shadow:0 6px 12px rgba(0,0,0,0.5)}button{background-color:#b71c1c;color:white;border:none;padding:10px 15px;margin:5px;border-radius:6px;cursor:pointer;font-size:1em;font-weight:500;transition:background-color .3s,transform .1s;box-shadow:0 2px 4px rgba(0,0,0,0.3)}button:hover{background-color:#f4511e;transform:translateY(-1px)}.status-item{margin-bottom:12px;font-size:1em;line-height:1.6}.status-label{font-weight:700;color:#ffab91}.track-buttons button,.ir-sim-buttons button{min-width:48px;height:48px;margin:4px}.color-button{width:38px;height:38px;margin:5px;border:2px solid #121212;border-radius:50%;padding:0;cursor:pointer}.color-preview{width:50px;height:25px;border:2px solid #ffab91;display:inline-block;vertical-align:middle;margin-left:10px;border-radius:4px}input[type=color]{width:50px;height:35px;border:1px solid #555;border-radius:4px;padding:2px;cursor:pointer;vertical-align:middle;margin-left:10px}input[type=range]{width:calc(100% - 180px);margin:0 10px;vertical-align:middle;cursor:pointer}label{vertical-align:middle;margin-right:5px}.controls-row{display:flex;flex-wrap:wrap;align-items:center;margin-bottom:12px;gap:10px}</style></head><body><h1>ESP32 Advanced Control Panel</h1><div class="grid-container"><div class="card"><h3>General Status</h3><div class="status-item"><span class="status-label">WiFi Network: </span><span id="wifiSSID">Loading...</span></div><div class="status-item"><span class="status-label">MQTT Status: </span><span id="mqttStatus">Loading...</span></div><div class="status-item"><span class="status-label">Active Encoder Mode: </span><span id="encoderModeStatus">Loading...</span></div><div class="status-item"><span class="status-label">Encoder Button (GPIO14): </span><span id="encoderButtonStatus">Not Pressed</span></div><div class="status-item"><span class="status-label">External Button (GPIO12): </span><span id="externalButtonD12Status">Not Pressed</span></div><div class="status-item"><span class="status-label">IR D-Pad Mode: </span><span id="irDpadModeStatus">LED Control</span></div><div class="status-item"><span class="status-label">SD Card: </span><span id="sdStatus">Loading...</span></div></div><div class="card"><h3>LED Control</h3><div class="controls-row"><button id="ledToggleBtn" onclick="sendIrAction('power')">Toggle LEDs (IR Power)</button></div><div class="controls-row"><label for="brightnessSlider" class="status-label">Brightness:</label><input type="range" id="brightnessSlider" min="0" max="255" value="120" oninput="updateBrightnessValue(this.value)" onchange="setBrightness(this.value)"><span id="brightnessValue">120</span></div><div class="status-item controls-row"><span class="status-label">Current Color:</span><input type="color" id="rgbColorPicker" onchange="setRGBColor(this.value)"><div id="currentColorPreview" class="color-preview"></div></div></div><div class="card"><h3>DFPlayer Control</h3><div class="status-item"><span class="status-label">Volume Level: </span><span id="volumeStatus">Loading...</span> (<span id="dfPlayerMutedStatus"></span>)</div><div class="status-item"><span class="status-label">Playing Track: </span><span id="trackStatus">Loading...</span> (<span id="trackNameStatus"></span>)</div><div class="controls-row"><label for="dfVolumeSlider" class="status-label">Set Volume:</label><input type="range" id="dfVolumeSlider" min="0" max="30" value="20" oninput="updateDfVolumeValue(this.value)" onchange="setDFVolume(this.value)"><span id="dfVolumeValue">20</span></div><div class="controls-row button-group"><button onclick="sendIrAction('play_stop')">Play/Pause (IR)</button><button onclick="sendIrAction('next_track')">Next (IR)</button><button onclick="sendIrAction('back_track')">Prev (IR)</button><button onclick="sendIrAction('mute')">Mute/Unmute (IR)</button></div><div><p class="status-label">Select Track (Direct):</p><span id="trackButtonsContainer"></span></div></div><div class="card"><h3>Simulate IR Remote</h3><div class="ir-sim-buttons"><button onclick="sendIrAction('power')">Power</button><button onclick="sendIrAction('mute')">Mute</button><button onclick="sendIrAction('music_btn')">Music</button><button onclick="sendIrAction('play_stop')">Play/Stop</button><br><button onclick="sendIrAction('up')">Up</button><button onclick="sendIrAction('down')">Down</button><button onclick="sendIrAction('left')">Left</button><button onclick="sendIrAction('right')">Right</button><button onclick="sendIrAction('enter')">Enter</button><br><button onclick="sendIrAction('vol_up')">Vol+</button><button onclick="sendIrAction('vol_down')">Vol-</button><button onclick="sendIrAction('ffwd')">FFWD</button><button onclick="sendIrAction('rew')">REW</button><button onclick="sendIrAction('next_track')">Next Trk</button><button onclick="sendIrAction('back_track')">Back Trk</button></div></div></div><script>const totalTracks=5;function sendCommand(url){fetch(url).then(response=>{if(!response.ok)console.error('Command Error:',response.status);return response.text()}).then(data=>console.log(url+'->'+data)).catch(error=>console.error('Fetch Error:',error));setTimeout(updateStatus,300)}function sendIrAction(actionName){sendCommand('/ir_action?cmd='+actionName)}function setBrightness(value){sendCommand('/set_brightness?value='+value)}function updateBrightnessValue(value){document.getElementById('brightnessValue').textContent=value}function setRGBColor(hexColor){const r=parseInt(hexColor.substr(1,2),16);const g=parseInt(hexColor.substr(3,2),16);const b=parseInt(hexColor.substr(5,2),16);sendCommand(`/set_led_rgb?r=${r}&g=${g}&b=${b}`)}function setDFVolume(volume){sendCommand('/set_df_volume?value='+volume)}function updateDfVolumeValue(value){document.getElementById('dfVolumeValue').textContent=value}function playTrack(trackNum){sendCommand('/play_track?track='+trackNum)}function updateStatus(){fetch('/status').then(response=>response.json()).then(data=>{document.getElementById('wifiSSID').textContent=data.wifiSSID||"Not Connected";document.getElementById('mqttStatus').textContent=data.mqttConnected?"Connected":"Disconnected";document.getElementById('encoderModeStatus').textContent=data.encoderMode;document.getElementById('encoderButtonStatus').textContent=data.encoderButtonPressed?"Pressed":"Not Pressed";document.getElementById('externalButtonD12Status').textContent=data.externalButtonD12Pressed?"Pressed":"Not Pressed";document.getElementById('irDpadModeStatus').textContent=data.irDpadMode;document.getElementById('sdStatus').textContent=data.sdCardOnline?"Online":"Offline/Error";document.getElementById('ledToggleBtn').textContent=data.ledsOn?"Turn LEDs Off":"Turn LEDs On";const brightnessSlider=document.getElementById('brightnessSlider');if(document.activeElement!==brightnessSlider){brightnessSlider.value=data.brightness}document.getElementById('brightnessValue').textContent=data.brightness;const colorHex='#'+[data.ledR,data.ledG,data.ledB].map(c=>parseInt(c).toString(16).padStart(2,'0')).join('');document.getElementById('currentColorPreview').style.backgroundColor=colorHex;const colorPicker=document.getElementById('rgbColorPicker');if(document.activeElement!==colorPicker)colorPicker.value=colorHex;document.getElementById('volumeStatus').textContent=data.dfVolume!==-1?data.dfVolume:"N/A";document.getElementById('dfPlayerMutedStatus').textContent=data.dfPlayerMuted?"MUTED":"";const dfVolumeSlider=document.getElementById('dfVolumeSlider');if(document.activeElement!==dfVolumeSlider){dfVolumeSlider.value=data.dfVolume!==-1?data.dfVolume:20}document.getElementById('dfVolumeValue').textContent=data.dfVolume!==-1?data.dfVolume:20;document.getElementById('trackStatus').textContent=data.dfTrack!==-1&&data.dfTrack!==0?data.dfTrack:"Stopped";document.getElementById('trackNameStatus').textContent=data.dfTrackName||"";console.log("Status updated:",data)}).catch(error=>console.error('Could not get status:',error))}function createTrackButtons(){const container=document.getElementById('trackButtonsContainer');if(container){container.innerHTML='';for(let i=1;i<=totalTracks;i++){const btn=document.createElement('button');btn.textContent=i;btn.onclick=function(){playTrack(i)};container.appendChild(btn)}}}window.onload=()=>{createTrackButtons();updateStatus();setInterval(updateStatus,2200)};</script></body></html>
)rawliteral";

// --- BÖLÜM 3: FONKSİYON PROTOTOTİPLERİ ---
// Bu, derleyiciye bu fonksiyonların daha sonra tanımlanacağını bildirir (kodun okunurluğunu artırır).
void setupHardware();
void setupWiFi();
void setupMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void reconnectMQTT();
void publishStatusMQTT(bool forcePublish = false);
void setupWebServer();
void handleRotaryEncoder();
void updateLeds();
void handleRoot();
void handleStatus();
void handleToggleLed();
void handleSetBrightness();
void handleSetLedRGB();
void handleSetDFVolume();
void handlePlayTrack();
void handleDFCommand(const char* cmd);
void handleDFMuteToggle();
void handleIrActionFromWeb(); // Web'den IR simülasyonu için yeni handler.
void handleNotFound();
void processIRCode(uint32_t irCode); // Alınan IR kodunu işler.
uint32_t getHexForIrAction(String actionName); // IR aksiyon ismine karşılık gelen HEX kodu döndürür.


// --- BÖLÜM 4: setup() FONKSİYONU ---
// ESP32 ilk açıldığında veya resetlendiğinde sadece bir kez çalışır.
void setup() {
    Serial.begin(115200); // Seri iletişimi başlat (hata ayıklama için).
    Serial.println("\n\n--- ESP32 Advanced Control Panel (IR Sim + MQTT) ---");

    setupHardware();    // Donanım bileşenlerini (LED, Encoder, DFPlayer, IR) başlat.
    setupWiFi();        // Wi-Fi bağlantısını kur.
    setupMQTT();        // MQTT istemcisini ayarla ve bağlanmayı dene.
    setupWebServer();   // Web sunucusunu başlat ve endpoint'leri tanımla.

    Serial.println("Setup Complete. IP Address:");
    Serial.println(WiFi.localIP()); // ESP32'nin aldığı IP adresini yazdır.
    Serial.println("MQTT Broker: " + String(mqtt_server)); // MQTT broker adresini yazdır.
    Serial.println("IR D-Pad Mode: " + String(irDpadModes[irDpadMode])); // Başlangıç IR D-Pad modunu yazdır.
    Serial.println("------------------------------------");
}

// --- BÖLÜM 5: loop() FONKSİYONU ---
// setup() bittikten sonra bu fonksiyon sürekli olarak tekrarlanır.
void loop() {
    // Wi-Fi ve MQTT bağlantı yönetimi
    if (WiFi.status() == WL_CONNECTED) { // Eğer Wi-Fi'ye bağlıysa
        if (!mqttClient.connected()) {   // Eğer MQTT'ye bağlı değilse
            reconnectMQTT();             // Yeniden bağlanmayı dene.
        }
        mqttClient.loop(); // MQTT istemcisinin arka plan işlemlerini (mesaj alma, keep-alive) yapmasını sağla.
    }

    server.handleClient(); // Gelen HTTP isteklerini işle.
    handleRotaryEncoder(); // Rotary encoder'dan gelen girdileri işle.

    // IR alıcıdan veri gelip gelmediğini kontrol et.
    if (IrReceiver.decode()) { // Eğer bir IR kodu çözüldüyse
        processIRCode(IrReceiver.decodedIRData.decodedRawData); // Çözülen ham veriyi işle.
                                                                // Farklı IR protokolleri için .command veya .address de kullanılabilir.
        IrReceiver.resume(); // Bir sonraki IR sinyalini almak için alıcıyı tekrar aktif et.
    }

    // DFPlayer olaylarını işle (örn: parça bitti, hata oluştu).
    bool dfStatusChanged = false; // DFPlayer durumunda değişiklik olup olmadığını izle.
    if (dfPlayerAvailable && myDFPlayer.available()) { // DFPlayer kullanılabilir ve seri portta veri varsa
        uint8_t type = myDFPlayer.readType(); // Gelen mesajın türünü oku.
        int value = myDFPlayer.read();        // Gelen mesajın değerini oku.
        switch (type) {
            case DFPlayerPlayFinished: // Bir parça bittiğinde
                Serial.print("Track Finished: "); Serial.println(value);
                if (currentTrackNumber != 0) dfStatusChanged = true; // Eğer bir parça çalıyorduysa durumu güncelle.
                currentTrackNumber = 0; // Mevcut parça numarasını sıfırla (durdu).
                break;
            case DFPlayerError: // Bir hata oluştuğunda
                Serial.print("DFPlayer Error, Code: "); Serial.println(value); // Hata kodunu yazdır.
                dfStatusChanged = true;
                break;
            case DFPlayerCardOnline: // SD kart takıldığında
                 if (!sdCardOnline) dfStatusChanged = true; // Eğer daha önce offline idiyse durumu güncelle.
                sdCardOnline = true;
                break;
            case DFPlayerCardRemoved: // SD kart çıkarıldığında
                if (sdCardOnline) dfStatusChanged = true; // Eğer daha önce online idiyse durumu güncelle.
                sdCardOnline = false;
                break;
        }
    }

    // MQTT durum yayınlama
    if (millis() - lastMqttPublishTime > mqttPublishInterval) { // Belirli aralıklarla
        publishStatusMQTT(true); // Durumu MQTT'ye zorla yayınla.
    } else if (dfStatusChanged) { // Veya DFPlayer durumunda bir değişiklik olduysa
        publishStatusMQTT();      // Durumu MQTT'ye yayınla.
    }
}

// --- BÖLÜM 6: setupHardware() FONKSİYONU ---
// Tüm donanım bileşenlerinin başlangıç ayarlarını yapar.
void setupHardware() {
    // NeoPixel LED Şeridi Başlatma
    strip.begin(); // NeoPixel kütüphanesini başlat.
    strip.setBrightness(globalBrightness); // Başlangıç parlaklığını ayarla.
    // Başlangıç rengini ilk ön tanımlı renge ayarla.
    currentLedR = predefinedColors[currentPredefinedColorIndex] >> 16 & 0xFF;
    currentLedG = predefinedColors[currentPredefinedColorIndex] >> 8 & 0xFF;
    currentLedB = predefinedColors[currentPredefinedColorIndex] & 0xFF;
    updateLeds(); // LED'leri güncelle (yak).
    Serial.println("NeoPixel Initialized. Pin: " + String(LED_PIN));

    // Rotary Encoder Başlatma
    rotaryEncoder.begin(); // Encoder kütüphanesini başlat.
    // Encoder pinlerindeki değişiklikleri dinlemek için kesme (interrupt) ayarını yap. Lambda fonksiyonu kullanılır.
    rotaryEncoder.setup([] { rotaryEncoder.readEncoder_ISR(); });
    rotaryEncoder.setBoundaries(0, 255, false); // Başlangıç modu (parlaklık) için sınırları ayarla (0-255, döngü yok).
    rotaryEncoder.setEncoderValue(globalBrightness); // Encoder'ın başlangıç değerini parlaklığa ayarla.
    Serial.println("Rotary Encoder Initialized. CLK:" + String(ROTARY_ENCODER_A_PIN) + " DT:" + String(ROTARY_ENCODER_B_PIN) + " SW:" + String(ROTARY_ENCODER_BUTTON_PIN) + ". Mode: " + encoderModes[currentEncoderMode]);

    // Harici Buton Başlatma
    pinMode(EXTERNAL_BUTTON_D12_PIN, INPUT_PULLUP); // Pini giriş ve dahili pull-up direnci aktif olarak ayarla.
    Serial.println("External D12 Button Initialized. Pin: " + String(EXTERNAL_BUTTON_D12_PIN));

    // DFPlayer Mini Başlatma
    myDFPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN); // DFPlayer için seri portu başlat.
    Serial.println("DFPlayer Serial Port Initialized.");
    // DFPlayer modülünü başlatmayı dene.
    if (!myDFPlayer.begin(myDFPlayerSerial, false, false)) { // `false, false` feedback ve ACK'yi devre dışı bırakır (isteğe bağlı).
        Serial.println("!!! DFPlayer FAILED to initialize !!!");
        dfPlayerAvailable = false;
        sdCardOnline = false;
    } else {
        Serial.println("DFPlayer Mini online.");
        dfPlayerAvailable = true;
        sdCardOnline = true; // Başlangıçta SD kartın online olduğunu varsayalım, olaylarla güncellenecek.
        myDFPlayer.volume(currentDFPlayerVolume); // Başlangıç ses seviyesini ayarla.
        previousDFPlayerVolume = currentDFPlayerVolume; // Sessize alma için önceki sesi sakla.
    }

    // IR Alıcı Başlatma
    IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // IR Alıcıyı başlat (varsa LED geri bildirimiyle).
                                                         // ENABLE_LED_FEEDBACK, IR sinyali alındığında ESP32'nin dahili LED'ini yakıp söndürebilir (kütüphane versiyonuna bağlı).
    Serial.println("IR Receiver Initialized. Pin: " + String(IR_RECEIVE_PIN));
}

// --- BÖLÜM 7: setupWiFi() FONKSİYONU ---
// ESP32'yi belirtilen Wi-Fi ağına bağlar.
void setupWiFi() {
    Serial.print("Connecting to WiFi: "); Serial.println(ssid);
    WiFi.mode(WIFI_STA); // ESP32'yi Wi-Fi istasyon moduna ayarla.
    WiFi.begin(ssid, password); // Bağlantıyı başlat.

    int wifi_attempts = 0;
    // Bağlantı kurulana kadar veya belirli bir deneme sayısına ulaşana kadar bekle.
    while (WiFi.status() != WL_CONNECTED && wifi_attempts < 20) {
        delay(500); Serial.print(".");
        wifi_attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
    } else {
        Serial.println("\nWiFi Connection Failed.");
    }
}

// --- BÖLÜM 8: setupMQTT() FONKSİYONU ---
// MQTT istemcisini ayarlar ve broker'a bağlanmayı dener.
void setupMQTT() {
    if (WiFi.status() != WL_CONNECTED) { // Eğer Wi-Fi bağlı değilse MQTT'yi başlatma.
        Serial.println("MQTT not started due to WiFi connection failure.");
        return;
    }
    mqttClient.setServer(mqtt_server, mqtt_port); // MQTT broker adresini ve portunu ayarla.
    mqttClient.setCallback(mqttCallback);         // Gelen MQTT mesajlarını işleyecek fonksiyonu ayarla.
    Serial.println("MQTT Setup Done. Attempting to connect...");
    reconnectMQTT(); // İlk bağlantıyı kurmayı dene.
}

// --- BÖLÜM 9: getHexForIrAction() FONKSİYONU ---
// Verilen IR aksiyon ismine (string) karşılık gelen HEX IR kodunu döndürür.
// Bu kodlar örnek NEC protokolü kodlarıdır, kendi kumandanıza göre değişebilir.
uint32_t getHexForIrAction(String actionName) {
    if (actionName == IR_ACTION_MUTE) return 0xED127F80;
    if (actionName == IR_ACTION_POWER) return 0xE11E7F80;
    if (actionName == IR_ACTION_MUSIC) return 0xFD027F80;
    if (actionName == IR_ACTION_PLAYSTOP) return 0xFB047F80;
    if (actionName == IR_ACTION_UP) return 0xFA057F80;
    if (actionName == IR_ACTION_RIGHT) return 0xF6097F80;
    if (actionName == IR_ACTION_LEFT) return 0xF8077F80;
    if (actionName == IR_ACTION_DOWN) return 0xE41B7F80;
    if (actionName == IR_ACTION_ENTER) return 0xF7087F80;
    if (actionName == IR_ACTION_VOL_UP) return 0xF30C7F80;
    if (actionName == IR_ACTION_VOL_DOWN) return 0xFF007F80;
    if (actionName == IR_ACTION_FFWD) return 0xF00F7F80;        // Genellikle "ileri sar" ama burada "sonraki parça" için kullanılabilir
    if (actionName == IR_ACTION_REW) return 0xF20D7F80;         // Genellikle "geri sar" ama burada "önceki parça" için kullanılabilir
    if (actionName == IR_ACTION_NEXT_TRACK) return 0xF10E7F80; // Özel "sonraki parça" tuşu
    if (actionName == IR_ACTION_BACK_TRACK) return 0xE6197F80; // Özel "önceki parça" tuşu
    return 0; // Eşleşme bulunamazsa 0 döndür.
}

// --- BÖLÜM 10: mqttCallback() FONKSİYONU ---
// Abone olunan MQTT konularına mesaj geldiğinde bu fonksiyon çağrılır.
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.print("MQTT Message arrived ["); Serial.print(topic); Serial.print("] ");
    char msg[length + 1]; // Gelen mesajı saklamak için bir karakter dizisi.
    for (unsigned int i = 0; i < length; i++) { msg[i] = (char)payload[i]; }
    msg[length] = '\0'; // String'i null-terminate et.
    Serial.println(msg);

    String topicStr = String(topic); // Konuyu String nesnesine çevir.
    bool publishUpdate = true;       // Durum güncellemesi yayınlanmalı mı?

    // Gelen konuya göre ilgili işlemi yap:
    if (topicStr == "esp32/command/led/state") {
        if (strcmp(msg, "ON") == 0) ledsOn = true; else if (strcmp(msg, "OFF") == 0) ledsOn = false;
        updateLeds();
    } else if (topicStr == "esp32/command/led/brightness") {
        globalBrightness = atoi(msg); // Gelen string mesajı integer'a çevir.
        updateLeds();
    } else if (topicStr == "esp32/command/led/colorRGB") {
        int r, g, b;
        // Mesajı "R,G,B" formatında ayrıştır.
        if (sscanf(msg, "%d,%d,%d", &r, &g, &b) == 3) { // Eğer 3 değer başarıyla okunursa
            currentLedR = r; currentLedG = g; currentLedB = b;
            // Eğer renk ayarlandıysa ve LED'ler kapalıysa, otomatik aç.
            if (!ledsOn && (currentLedR > 0 || currentLedG > 0 || currentLedB > 0) ) ledsOn = true;
            updateLeds();
        } else { Serial.println("MQTT: Failed to parse RGB color string."); publishUpdate = false;}
    } else if (topicStr == "esp32/command/dfplayer/volume") {
        if (dfPlayerAvailable) { currentDFPlayerVolume = atoi(msg); myDFPlayer.volume(currentDFPlayerVolume); dfPlayerMuted = false; } 
        else { publishUpdate = false; }
    } else if (topicStr == "esp32/command/dfplayer/playTrack") {
        if (dfPlayerAvailable) {
            int track = atoi(msg);
            if (track >= 1 && track <= totalTracks) { myDFPlayer.play(track); currentTrackNumber = track; dfPlayerMuted = false; } 
            else { publishUpdate = false; }
        } else { publishUpdate = false; }
    } else if (topicStr == "esp32/command/dfplayer/action") {
        if (dfPlayerAvailable) {
            if (strcmp(msg, "PLAY_CURRENT") == 0) { // Mevcut (veya ilk) parçayı çal/devam et.
                if (currentTrackNumber <= 0 || currentTrackNumber > totalTracks) {myDFPlayer.play(1); currentTrackNumber = 1;} else myDFPlayer.start();
                dfPlayerMuted = false;
            } else if (strcmp(msg, "PAUSE") == 0) { myDFPlayer.pause(); Serial.println("MQTT: DFPlayer Paused");}
            else if (strcmp(msg, "NEXT") == 0) { myDFPlayer.next(); currentTrackNumber = 0; dfPlayerMuted = false; Serial.println("MQTT: DFPlayer Next");} // `currentTrackNumber = 0` bir sonraki IR/manuel işlemde durumu doğru alması için.
            else if (strcmp(msg, "PREV") == 0) { myDFPlayer.previous(); currentTrackNumber = 0; dfPlayerMuted = false; Serial.println("MQTT: DFPlayer Previous");}
            else if (strcmp(msg, "MUTE_TOGGLE") == 0) { // MQTT için sessize alma/açma.
                if (dfPlayerMuted) { myDFPlayer.volume(previousDFPlayerVolume); currentDFPlayerVolume = previousDFPlayerVolume; dfPlayerMuted = false; } 
                else { previousDFPlayerVolume = myDFPlayer.readVolume(); if(previousDFPlayerVolume == -1 || previousDFPlayerVolume == 0) previousDFPlayerVolume = 20; myDFPlayer.volume(0); currentDFPlayerVolume = 0; dfPlayerMuted = true; }
            } else { Serial.print("MQTT: Unknown DFPlayer action: "); Serial.println(msg); publishUpdate = false; }
        } else { publishUpdate = false; }
    } else if (topicStr == "esp32/command/ir_action") { // MQTT üzerinden IR komutu simülasyonu.
        String cmdName = String(msg);
        uint32_t hexCode = getHexForIrAction(cmdName); // Aksiyon ismine karşılık gelen HEX kodu al.
        if (hexCode != 0) { // Eğer geçerli bir aksiyon ismiyse
            Serial.print("MQTT: Simulating IR command: "); Serial.println(cmdName);
            processIRCode(hexCode); // IR kodunu işle.
        } else { Serial.print("MQTT: Unknown IR Sim action: "); Serial.println(cmdName); publishUpdate = false;}
    } else { // Bilinmeyen bir konuysa
        publishUpdate = false;
    }
    if (publishUpdate) publishStatusMQTT(); // Eğer işlem başarılıysa ve durum değiştiyse MQTT'ye güncelleme yayınla.
}

// --- BÖLÜM 11: reconnectMQTT() FONKSİYONU ---
// MQTT broker'ına yeniden bağlanmayı dener ve başarılı olursa konulara abone olur.
void reconnectMQTT() {
    long now = millis();
    static long lastReconnectAttempt = 0; // Son yeniden bağlanma denemesinin zamanını saklar.
                                          // `static` sayesinde fonksiyon çağrıları arasında değeri korunur.
    // Sadece belirli aralıklarla (örn: 5 saniye) yeniden bağlanmayı dene.
    if (now - lastReconnectAttempt > 5000) { 
        lastReconnectAttempt = now;
        // Eğer MQTT'ye bağlı değilse VE Wi-Fi'ye bağlıysa
        if (!mqttClient.connected() && WiFi.status() == WL_CONNECTED) {
            Serial.print("Attempting MQTT connection...");
            // Tanımlanan client ID ile broker'a bağlanmayı dene.
            // Gerekirse kullanıcı adı ve şifre de eklenebilir: mqttClient.connect(mqtt_client_id, mqtt_user, mqtt_pass)
            if (mqttClient.connect(mqtt_client_id)) {
                Serial.println("connected");
                // Komutları alacağımız konulara abone ol.
                mqttClient.subscribe("esp32/command/led/state");
                mqttClient.subscribe("esp32/command/led/brightness");
                mqttClient.subscribe("esp32/command/led/colorRGB");
                mqttClient.subscribe("esp32/command/dfplayer/volume");
                mqttClient.subscribe("esp32/command/dfplayer/playTrack");
                mqttClient.subscribe("esp32/command/dfplayer/action");
                mqttClient.subscribe("esp32/command/ir_action"); // IR simülasyon komutları için yeni konu.
                
                publishStatusMQTT(true); // Bağlantı kurulunca mevcut durumu hemen yayınla.
            } else {
                Serial.print("failed, rc="); Serial.print(mqttClient.state()); Serial.println(" try again in 5 seconds");
                // mqttClient.state() hata kodunu döndürür (örn: -2 ağ hatası).
            }
        }
    }
}

// --- BÖLÜM 12: publishStatusMQTT() FONKSİYONU ---
// Cihazın mevcut durumunu (LED, DFPlayer, sensörler vb.) MQTT konularına yayınlar.
// `forcePublish`: true ise, kısa süre önce yayın yapılmış olsa bile tekrar yayınlar.
void publishStatusMQTT(bool forcePublish = false) {
    static unsigned long lastForcedPublish = 0; // Son zorla yayın zamanı.
                                                // Bu, durum değişikliklerinin çok sık yayınlanmasını engellemek için.
    if (forcePublish) { 
        lastForcedPublish = millis(); 
    } else { 
        // Eğer zorla yayın değilse ve son zorla yayından bu yana çok kısa süre geçtiyse (500ms) yayınlama.
        if (millis() - lastForcedPublish < 500) return; 
    }

    if (!mqttClient.connected() || WiFi.status() != WL_CONNECTED) return; // Bağlantı yoksa yayınlama.

    char buffer[64]; // Sayısal değerleri string'e çevirmek için geçici tampon.

    // Tüm durum bilgilerini ilgili MQTT konularına `true` (retained) olarak yayınla.
    // Retained mesajlar, yeni abone olan istemcilerin son durumu hemen almasını sağlar.
    mqttClient.publish("esp32/status/wifiSSID", WiFi.SSID().c_str(), true);
    mqttClient.publish("esp32/status/encoderMode", encoderModes[currentEncoderMode], true);
    mqttClient.publish("esp32/status/encoderButton", (digitalRead(ROTARY_ENCODER_BUTTON_PIN) == LOW) ? "PRESSED" : "NOT_PRESSED", true);
    mqttClient.publish("esp32/status/buttonD12", (digitalRead(EXTERNAL_BUTTON_D12_PIN) == LOW) ? "PRESSED" : "NOT_PRESSED", true);
    mqttClient.publish("esp32/status/irDpadMode", irDpadModes[irDpadMode], true);
    mqttClient.publish("esp32/status/sdCard", sdCardOnline ? "ONLINE" : "OFFLINE", true);
    mqttClient.publish("esp32/status/ledsOn", ledsOn ? "ON" : "OFF", true);
    sprintf(buffer, "%d", globalBrightness); mqttClient.publish("esp32/status/brightness", buffer, true);
    sprintf(buffer, "%d,%d,%d", currentLedR, currentLedG, currentLedB); mqttClient.publish("esp32/status/led/colorRGB", buffer, true);
    mqttClient.publish("esp32/status/dfplayer/muted", dfPlayerMuted ? "MUTED" : "UNMUTED", true);
    
    if (dfPlayerAvailable) {
        int vol = myDFPlayer.readVolume(); // Mevcut ses seviyesini oku.
        sprintf(buffer, "%d", vol); mqttClient.publish("esp32/status/dfVolume", buffer, true);
        
        int track = myDFPlayer.readCurrentFileNumber(); // Mevcut parça numarasını oku.
                                                       // Not: Bu fonksiyon bazen çalınan parçayı değil, son komutla seçilen dosyayı döndürebilir.
                                                       // Daha güvenilir parça takibi için DFPlayer olayları (örn: PlayFinished) kullanılabilir.
        sprintf(buffer, "%d", track); mqttClient.publish("esp32/status/dfTrackNumber", buffer, true);
        
        if (track > 0 && track <= totalTracks) { mqttClient.publish("esp32/status/dfTrackName", trackNames[track], true); } 
        else { mqttClient.publish("esp32/status/dfTrackName", trackNames[0], true); } // "Unknown/Stopped"
    } else { // DFPlayer kullanılamıyorsa varsayılan/hata değerleri yayınla.
        mqttClient.publish("esp32/status/dfVolume", "-1", true); 
        mqttClient.publish("esp32/status/dfTrackNumber", "-1", true); 
        mqttClient.publish("esp32/status/dfTrackName", trackNames[0], true);
    }
    Serial.println("MQTT Status Published.");
    lastMqttPublishTime = millis(); // Son yayın zamanını güncelle.
}


// --- BÖLÜM 13: setupWebServer() FONKSİYONU ---
// Web sunucusunu başlatır ve HTTP isteklerini işleyecek endpoint'leri (URL yolları) tanımlar.
void setupWebServer() {
    if (WiFi.status() != WL_CONNECTED) { Serial.println("Web Server not started due to WiFi connection failure."); return; }

    server.on("/", HTTP_GET, handleRoot);             // Ana sayfa ("/") isteği geldiğinde handleRoot fonksiyonunu çağır.
    server.on("/status", HTTP_GET, handleStatus);     // "/status" isteği geldiğinde cihazın durumunu JSON olarak döndür.
    
    // LED Kontrolü için endpoint'ler
    server.on("/toggle_led", HTTP_GET, handleToggleLed);
    server.on("/set_brightness", HTTP_GET, handleSetBrightness);
    server.on("/set_led_rgb", HTTP_GET, handleSetLedRGB);
    
    // DFPlayer Kontrolü için endpoint'ler
    server.on("/set_df_volume", HTTP_GET, handleSetDFVolume);
    server.on("/play_track", HTTP_GET, handlePlayTrack);
    // Lambda fonksiyonları ile doğrudan DFPlayer komutlarını çağıran endpoint'ler.
    server.on("/df_play_current", HTTP_GET, [](){ handleDFCommand("play_current"); });
    server.on("/df_pause", HTTP_GET, [](){ handleDFCommand("pause"); });
    server.on("/df_next", HTTP_GET, [](){ handleDFCommand("next"); });
    server.on("/df_prev", HTTP_GET, [](){ handleDFCommand("prev"); });
    server.on("/df_mute_toggle", HTTP_GET, handleDFMuteToggle); // Sessize alma/açma için yeni endpoint.

    // IR Simülasyonu için endpoint
    server.on("/ir_action", HTTP_GET, handleIrActionFromWeb); // Web'den IR komutu göndermek için.
    
    server.onNotFound(handleNotFound); // Tanımlanmayan bir URL isteği gelirse handleNotFound'u çağır.
    
    server.begin(); // Web sunucusunu başlat.
    Serial.println("HTTP Server Started.");
}

// --- BÖLÜM 14: updateLeds() FONKSİYONU ---
// NeoPixel LED şeridini mevcut renge ve parlaklığa göre günceller.
void updateLeds() {
    if (ledsOn) { // Eğer LED'ler açıksa
        strip.setBrightness(globalBrightness); // Parlaklığı ayarla.
        strip.fill(strip.Color(currentLedR, currentLedG, currentLedB)); // Tüm LED'leri belirtilen renkle doldur.
    } else { // Eğer LED'ler kapalıysa
        strip.fill(strip.Color(0,0,0)); // Tüm LED'leri söndür (siyah renk).
    }
    strip.show(); // Değişiklikleri LED şeridine gönder.
}

// --- BÖLÜM 15: handleRotaryEncoder() FONKSİYONU ---
// Rotary encoder'dan gelen döndürme ve buton basma olaylarını işler.
void handleRotaryEncoder() {
    bool stateChanged = false; // Encoder durumunda değişiklik oldu mu?

    // Encoder döndürüldüyse
    if (rotaryEncoder.encoderChanged()) {
        int value = rotaryEncoder.readEncoder(); // Encoder'ın yeni değerini oku.
        stateChanged = true;
        switch (currentEncoderMode) { // Mevcut encoder moduna göre işlem yap.
            case 0: // LED Parlaklık Modu
                globalBrightness = value; // Okunan değeri global parlaklığa ata.
                updateLeds(); // LED'leri güncelle.
                break;
            case 1: // LED Renk Seçim Modu
                currentPredefinedColorIndex = value; // Okunan değeri renk indeksine ata.
                // Seçilen indeksteki ön tanımlı rengi al ve RGB bileşenlerine ayır.
                uint32_t newColor = predefinedColors[currentPredefinedColorIndex];
                currentLedR = (newColor >> 16) & 0xFF; // Kırmızı bileşeni
                currentLedG = (newColor >> 8) & 0xFF;  // Yeşil bileşeni
                currentLedB = newColor & 0xFF;         // Mavi bileşeni
                updateLeds(); // LED'leri güncelle.
                break;
            case 2: // DFPlayer Ses Seviyesi Modu
                currentDFPlayerVolume = value; // Okunan değeri ses seviyesine ata.
                if (dfPlayerAvailable && !dfPlayerMuted) { // DFPlayer varsa ve sessizde değilse
                    myDFPlayer.volume(currentDFPlayerVolume); // Ses seviyesini ayarla.
                }
                break;
        }
    }

    // Encoder butonuna basıldıysa
    if (rotaryEncoder.isEncoderButtonClicked()) {
        currentEncoderMode = (currentEncoderMode + 1) % 3; // Modu bir sonrakine geçir (3 mod arasında döngü).
        stateChanged = true;
        Serial.print("Encoder Mode Changed to: "); Serial.println(encoderModes[currentEncoderMode]);

        // Yeni moda göre encoder'ın sınırlarını ve başlangıç değerini ayarla.
        switch (currentEncoderMode) {
            case 0: // LED Parlaklık
                rotaryEncoder.setBoundaries(0, 255, false); // Sınırlar 0-255, döngü yok.
                rotaryEncoder.setEncoderValue(globalBrightness); // Mevcut parlaklığı ayarla.
                break;
            case 1: // LED Renk Seçimi
                rotaryEncoder.setBoundaries(0, num_predefined_colors - 1, true); // Sınırlar renk sayısı kadar, döngü var.
                rotaryEncoder.setEncoderValue(currentPredefinedColorIndex); // Mevcut renk indeksini ayarla.
                break;
            case 2: // DFPlayer Ses Seviyesi
                rotaryEncoder.setBoundaries(0, 30, false); // Sınırlar 0-30 (DFPlayer ses aralığı), döngü yok.
                int volToSet = currentDFPlayerVolume;
                if(dfPlayerAvailable && !dfPlayerMuted) { // Eğer DFPlayer aktif ve sessizde değilse
                    int volRead = myDFPlayer.readVolume(); // Mevcut sesi oku (emin olmak için).
                    volToSet = (volRead == -1 ? currentDFPlayerVolume : volRead); // Okuma başarısızsa eski değeri kullan.
                } else if (dfPlayerMuted) { // Sessizdeyse encoder'ı 0'a ayarla.
                    volToSet = 0;
                } else { // DFPlayer yoksa varsayılan bir değere ayarla.
                    volToSet = 20;
                }
                rotaryEncoder.setEncoderValue(volToSet);
                break;
        }
        delay(100); // Buton "bounce" etkisini (titreşimle çoklu basım algılama) önlemek için kısa bekleme.
    }

    // Eğer encoder durumu değiştiyse ve MQTT bağlıysa, durumu yayınla.
    if (stateChanged && mqttClient.connected()) {
        publishStatusMQTT();
    }
}

// --- BÖLÜM 16: processIRCode() FONKSİYONU ---
// Alınan ham IR kodunu işler ve ilgili eylemi gerçekleştirir.
// Bu IR kodları (örn: 0xED127F80) spesifik bir IR kumandasına (genellikle NEC protokolü) aittir.
// Kendi kumandanızın kodlarını buraya girmeniz gerekir.
void processIRCode(uint32_t irCode) {
    if (irCode == 0 || irCode == 0xFFFFFFFF) return; // Geçersiz veya tekrar eden kodları yoksay.
    Serial.printf("IR Code Received: 0x%X\n", irCode); // Alınan kodu Seri Monitör'e yazdır.
    bool stateChanged = true; // Durumda değişiklik oldu mu? (MQTT yayınlamak için)

    switch (irCode) {
        case 0xED127F80: // Örnek: MUTE tuşu
            Serial.println("IR: Mute Toggle"); 
            if (dfPlayerAvailable) { 
                if (dfPlayerMuted) { // Eğer sessizdeyse, sesi aç.
                    myDFPlayer.volume(previousDFPlayerVolume); 
                    currentDFPlayerVolume = previousDFPlayerVolume; 
                    dfPlayerMuted = false; 
                } else { // Eğer ses açıksa, sessize al.
                    previousDFPlayerVolume = myDFPlayer.readVolume(); 
                    // Eğer okunan ses -1 (hata) veya 0 ise, ve mevcut ses 0 değilse onu kullan, yoksa varsayılan bir değere ayarla.
                    if(previousDFPlayerVolume == -1 || previousDFPlayerVolume == 0) previousDFPlayerVolume = currentDFPlayerVolume > 0 ? currentDFPlayerVolume : 15; 
                    myDFPlayer.volume(0); 
                    currentDFPlayerVolume = 0; 
                    dfPlayerMuted = true; 
                } 
            } 
            break;
        case 0xE11E7F80: // Örnek: POWER tuşu (LED Toggle olarak kullanılıyor)
            Serial.println("IR: LED Toggle"); 
            ledsOn = !ledsOn; // LED durumunu tersine çevir.
            updateLeds(); 
            break;
        case 0xFD027F80: // Örnek: MUSIC tuşu (Track 1 çal olarak kullanılıyor)
            Serial.println("IR: Play Track 1"); 
            if (dfPlayerAvailable) { 
                myDFPlayer.play(1); 
                currentTrackNumber = 1; 
                dfPlayerMuted = false; // Müzik çalmaya başlayınca sessizden çık.
            } 
            break;
        case 0xFB047F80: // Örnek: PLAY/PAUSE tuşu
            Serial.println("IR: Play/Pause Toggle"); 
            if (dfPlayerAvailable) { 
                if (myDFPlayer.readState() == 1) { // DFPlayer'ın durumunu oku (1: Çalıyor)
                    myDFPlayer.pause(); // Çalıyorsa duraklat.
                } else { // Durmuş veya duraklamışsa
                    // Eğer geçerli bir parça seçili değilse, 1. parçayı çal.
                    if (currentTrackNumber <= 0 || currentTrackNumber > totalTracks) {
                        myDFPlayer.play(1); 
                        currentTrackNumber = 1;
                    } else { // Seçili parçayı devam ettir.
                        myDFPlayer.start();
                    } 
                    dfPlayerMuted = false; // Müzik çalmaya başlayınca sessizden çık.
                } 
            } 
            break; 
        case 0xF30C7F80: // Örnek: VOLUME UP tuşu
            Serial.println("IR: Volume Up"); 
            if (dfPlayerAvailable) { 
                myDFPlayer.volumeUp(); // Sesi bir kademe artır.
                currentDFPlayerVolume = myDFPlayer.readVolume(); // Yeni ses seviyesini oku.
                dfPlayerMuted = false; // Ses ayarı yapıldıysa sessizden çık.
            } 
            break;
        case 0xFF007F80: // Örnek: VOLUME DOWN tuşu
            Serial.println("IR: Volume Down"); 
            if (dfPlayerAvailable) { 
                myDFPlayer.volumeDown(); // Sesi bir kademe azalt.
                currentDFPlayerVolume = myDFPlayer.readVolume(); 
                dfPlayerMuted = (currentDFPlayerVolume == 0); // Ses 0 ise sessize alınmış say.
            } 
            break;
        // NEXT ve FFWD (ileri sarma) tuşları aynı işlevi görebilir (Sonraki Parça)
        case 0xF00F7F80: // Örnek: FFWD tuşu
        case 0xF10E7F80: // Örnek: NEXT TRACK tuşu
            Serial.println("IR: Next Track"); 
            if (dfPlayerAvailable) { 
                myDFPlayer.next(); 
                currentTrackNumber = 0; // Bir sonraki parçanın numarasını DFPlayer'dan okumak gerekebilir. Şimdilik sıfırla.
                dfPlayerMuted = false; 
            } 
            break;
        // PREVIOUS ve REW (geri sarma) tuşları aynı işlevi görebilir (Önceki Parça)
        case 0xF20D7F80: // Örnek: REW tuşu
        case 0xE6197F80: // Örnek: BACK TRACK tuşu
            Serial.println("IR: Previous Track"); 
            if (dfPlayerAvailable) { 
                myDFPlayer.previous(); 
                currentTrackNumber = 0; 
                dfPlayerMuted = false; 
            } 
            break;
        case 0xF7087F80: // Örnek: ENTER veya OK tuşu (IR D-Pad Modunu değiştir)
            irDpadMode = (irDpadMode + 1) % 2; // Modu değiştir (0 -> 1, 1 -> 0).
            Serial.print("IR: D-Pad Mode Changed to: "); Serial.println(irDpadModes[irDpadMode]); 
            break;
        // D-Pad YUKARI tuşu
        case 0xFA057F80: 
            Serial.println("IR: Up Arrow"); 
            if (irDpadMode == 0) { // LED Kontrol Modu
                globalBrightness = min(255, globalBrightness + 15); // Parlaklığı artır (max 255).
                updateLeds(); 
            } else { // DFPlayer Kontrol Modu
                if (dfPlayerAvailable) { 
                    myDFPlayer.volumeUp(); 
                    currentDFPlayerVolume = myDFPlayer.readVolume(); 
                    dfPlayerMuted = false;
                } 
            } 
            break;
        // D-Pad AŞAĞI tuşu
        case 0xE41B7F80: 
            Serial.println("IR: Down Arrow"); 
            if (irDpadMode == 0) { // LED Kontrol Modu
                globalBrightness = max(0, globalBrightness - 15); // Parlaklığı azalt (min 0).
                updateLeds(); 
            } else { // DFPlayer Kontrol Modu
                if (dfPlayerAvailable) { 
                    myDFPlayer.volumeDown(); 
                    currentDFPlayerVolume = myDFPlayer.readVolume(); 
                    dfPlayerMuted = (currentDFPlayerVolume == 0); 
                } 
            } 
            break;
        // D-Pad SOL tuşu
        case 0xF8077F80: 
            Serial.println("IR: Left Arrow"); 
            if (irDpadMode == 0) { // LED Kontrol Modu: Önceki ön tanımlı renk
                currentPredefinedColorIndex = (currentPredefinedColorIndex - 1 + num_predefined_colors) % num_predefined_colors; 
                uint32_t nCL = predefinedColors[currentPredefinedColorIndex]; 
                currentLedR=(nCL>>16)&0xFF; currentLedG=(nCL>>8)&0xFF; currentLedB=nCL&0xFF; 
                updateLeds(); 
            } else { // DFPlayer Kontrol Modu: Önceki parça
                if (dfPlayerAvailable) { 
                    myDFPlayer.previous(); 
                    currentTrackNumber = 0; 
                    dfPlayerMuted = false;
                } 
            } 
            break;
        // D-Pad SAĞ tuşu
        case 0xF6097F80: 
            Serial.println("IR: Right Arrow"); 
            if (irDpadMode == 0) { // LED Kontrol Modu: Sonraki ön tanımlı renk
                currentPredefinedColorIndex = (currentPredefinedColorIndex + 1) % num_predefined_colors; 
                uint32_t nCR = predefinedColors[currentPredefinedColorIndex]; 
                currentLedR=(nCR>>16)&0xFF; currentLedG=(nCR>>8)&0xFF; currentLedB=nCR&0xFF; 
                updateLeds(); 
            } else { // DFPlayer Kontrol Modu: Sonraki parça
                if (dfPlayerAvailable) { 
                    myDFPlayer.next(); 
                    currentTrackNumber = 0; 
                    dfPlayerMuted = false;
                } 
            } 
            break;
        default: // Bilinmeyen veya atanmamış bir IR kodu geldiyse
            Serial.println("IR: Unknown or Unassigned Code"); 
            stateChanged = false; // Durum değişmedi.
            break;
    }

    // Eğer IR komutu bir durum değişikliğine yol açtıysa ve MQTT bağlıysa, durumu yayınla.
    if (stateChanged && mqttClient.connected()) { 
        publishStatusMQTT(); 
    }
}


// --- BÖLÜM 17: WEB HANDLER FONKSİYONLARI ---
// Bu fonksiyonlar, web sunucusuna gelen HTTP isteklerini işler.

// Ana sayfayı ("/") sunar. INDEX_HTML içeriğini gönderir.
void handleRoot() { 
    server.send_P(200, "text/html", INDEX_HTML); // _P versiyonu PROGMEM'den veri gönderir.
}

// Cihazın mevcut durumunu JSON formatında döndürür.
void handleStatus() {
    jsonDocHttp.clear(); // Önceki JSON verilerini temizle.
    // Durum bilgilerini JSON nesnesine ekle.
    jsonDocHttp["wifiSSID"] = (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : "Not Connected";
    jsonDocHttp["mqttConnected"] = mqttClient.connected();
    jsonDocHttp["encoderMode"] = encoderModes[currentEncoderMode];
    jsonDocHttp["encoderButtonPressed"] = (digitalRead(ROTARY_ENCODER_BUTTON_PIN) == LOW); // Buton basılıysa true.
    jsonDocHttp["externalButtonD12Pressed"] = (digitalRead(EXTERNAL_BUTTON_D12_PIN) == LOW);
    jsonDocHttp["irDpadMode"] = irDpadModes[irDpadMode];
    jsonDocHttp["sdCardOnline"] = sdCardOnline;
    jsonDocHttp["ledsOn"] = ledsOn;
    jsonDocHttp["brightness"] = globalBrightness;
    jsonDocHttp["ledR"] = currentLedR;
    jsonDocHttp["ledG"] = currentLedG;
    jsonDocHttp["ledB"] = currentLedB;
    jsonDocHttp["dfPlayerMuted"] = dfPlayerMuted;

    if(dfPlayerAvailable) { // DFPlayer varsa bilgilerini ekle.
        jsonDocHttp["dfVolume"] = myDFPlayer.readVolume(); 
        int track = myDFPlayer.readCurrentFileNumber(); // Güvenilirlik için `currentTrackNumber` da kullanılabilir.
        jsonDocHttp["dfTrack"] = track > 0 ? track : currentTrackNumber; // Eğer okunan 0 ise ve currentTrackNumber >0 ise onu kullan.
        int trackNumForName = track > 0 ? track : currentTrackNumber;
        if (trackNumForName > 0 && trackNumForName <= totalTracks) { jsonDocHttp["dfTrackName"] = trackNames[trackNumForName]; } 
        else { jsonDocHttp["dfTrackName"] = trackNames[0]; } // "Unknown/Stopped"
    } else { // DFPlayer yoksa varsayılan/hata değerleri ekle.
        jsonDocHttp["dfVolume"] = -1; 
        jsonDocHttp["dfTrack"] = -1; 
        jsonDocHttp["dfTrackName"] = trackNames[0];
    }
    String response; 
    serializeJson(jsonDocHttp, response); // JSON nesnesini string'e çevir.
    server.send(200, "application/json", response); // JSON yanıtını gönder.
}

// LED'leri açıp kapatır.
void handleToggleLed() { 
    ledsOn = !ledsOn; 
    updateLeds(); 
    server.send(200, "text/plain", "OK"); 
    publishStatusMQTT(); // Durumu MQTT'ye yayınla.
}

// LED parlaklığını ayarlar.
void handleSetBrightness() { 
    if (server.hasArg("value")) { // Eğer "value" parametresi geldiyse
        globalBrightness = server.arg("value").toInt(); // Değeri integer'a çevir ve ata.
        updateLeds(); 
    } 
    server.send(200, "text/plain", "OK"); 
    publishStatusMQTT();
}

// LED rengini RGB olarak ayarlar.
void handleSetLedRGB() { 
    if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) { // r, g, b parametreleri geldiyse
        currentLedR = server.arg("r").toInt(); 
        currentLedG = server.arg("g").toInt(); 
        currentLedB = server.arg("b").toInt(); 
        // Renk ayarlandıysa ve LED'ler kapalıysa otomatik aç.
        if (!ledsOn && (currentLedR > 0 || currentLedG > 0 || currentLedB > 0) ) { 
            ledsOn = true; 
        } 
        updateLeds(); 
    } 
    server.send(200, "text/plain", "OK"); 
    publishStatusMQTT();
}

// DFPlayer ses seviyesini ayarlar.
void handleSetDFVolume() { 
    if (dfPlayerAvailable && server.hasArg("value")) { 
        currentDFPlayerVolume = server.arg("value").toInt(); 
        myDFPlayer.volume(currentDFPlayerVolume); 
        dfPlayerMuted = (currentDFPlayerVolume == 0); // Ses 0 ise sessize alınmış say.
        // Sessize almadan önceki sesi güncelle (eğer yeni ses 0 değilse).
        if (currentDFPlayerVolume > 0) previousDFPlayerVolume = currentDFPlayerVolume;
    } 
    server.send(200, "text/plain", "OK"); 
    publishStatusMQTT();
}

// Belirli bir parçayı çalar.
void handlePlayTrack() { 
    if (dfPlayerAvailable && server.hasArg("track")) { 
        int trackNum = server.arg("track").toInt(); 
        if (trackNum >= 1 && trackNum <= totalTracks) { // Geçerli parça numarası mı?
            myDFPlayer.play(trackNum); 
            currentTrackNumber = trackNum; 
            dfPlayerMuted = false; // Müzik çalmaya başlayınca sessizden çık.
        } 
    } 
    server.send(200, "text/plain", "OK"); 
    publishStatusMQTT();
}

// Genel DFPlayer komutlarını işler (play, pause, next, prev).
void handleDFCommand(const char* cmd) { 
    if (!dfPlayerAvailable) { server.send(503, "text/plain", "DFPlayer Not Available"); return; } 
    bool statusShouldChange = true; 
    if (strcmp(cmd, "play_current") == 0) { 
        if (currentTrackNumber <= 0 || currentTrackNumber > totalTracks) { myDFPlayer.play(1); currentTrackNumber = 1; } 
        else { myDFPlayer.start(); } 
        dfPlayerMuted = false; 
    } else if (strcmp(cmd, "pause") == 0) myDFPlayer.pause(); 
    else if (strcmp(cmd, "next") == 0) { myDFPlayer.next(); currentTrackNumber = 0; dfPlayerMuted = false; } 
    else if (strcmp(cmd, "prev") == 0) { myDFPlayer.previous(); currentTrackNumber = 0; dfPlayerMuted = false; } 
    else { statusShouldChange = false; } // Bilinmeyen komutsa durumu yayınlama.
    server.send(200, "text/plain", "OK"); 
    if(statusShouldChange) publishStatusMQTT(); 
}

// DFPlayer için web'den sessize alma/açma işlemini yönetir.
void handleDFMuteToggle() {
    if (dfPlayerAvailable) {
        if (dfPlayerMuted) { // Eğer sessizdeyse, sesi aç.
            // `previousDFPlayerVolume` 0 veya -1 (hata) değilse onu kullan, yoksa 15 gibi bir varsayılan kullan.
            int volToSet = previousDFPlayerVolume > 0 ? previousDFPlayerVolume : 15;
            myDFPlayer.volume(volToSet); 
            currentDFPlayerVolume = volToSet;
            dfPlayerMuted = false;
            Serial.println("WEB: DFPlayer Unmuted. Volume set to: " + String(currentDFPlayerVolume));
        } else { // Eğer ses açıksa, sessize al.
            previousDFPlayerVolume = myDFPlayer.readVolume(); // Mevcut sesi kaydet.
            // Eğer okunan ses -1 (hata) veya 0 ise, ve mevcut `currentDFPlayerVolume` 0 değilse onu `previous` olarak sakla,
            // yoksa `previous` için 15 gibi bir varsayılan kullan.
            if (previousDFPlayerVolume == -1 || previousDFPlayerVolume == 0) previousDFPlayerVolume = currentDFPlayerVolume > 0 ? currentDFPlayerVolume : 15;
            myDFPlayer.volume(0); // Sesi sıfırla.
            currentDFPlayerVolume = 0; 
            dfPlayerMuted = true;
            Serial.println("WEB: DFPlayer Muted. Previous volume was: " + String(previousDFPlayerVolume));
        }
    }
    server.send(200, "text/plain", "OK");
    publishStatusMQTT(); // Durumu MQTT'ye yayınla.
}

// --- BÖLÜM 18: handleIrActionFromWeb() FONKSİYONU ---
// Web arayüzünden gönderilen IR komutlarını simüle eder.
void handleIrActionFromWeb() {
    if (server.hasArg("cmd")) { // Eğer "cmd" parametresi geldiyse
        String cmdName = server.arg("cmd"); // Komut ismini al.
        uint32_t hexCode = getHexForIrAction(cmdName); // Komut ismine karşılık gelen HEX IR kodunu al.
        if (hexCode != 0) { // Eğer geçerli bir komutsa
            Serial.print("WEB: Simulating IR command: "); Serial.println(cmdName);
            processIRCode(hexCode); // IR kodunu işle (sanki IR alıcıdan gelmiş gibi).
        } else { // Bilinmeyen komutsa
            Serial.print("WEB: Unknown IR action: "); Serial.println(cmdName);
        }
    }
    server.send(200, "text/plain", "OK"); // Her durumda "OK" yanıtı gönder.
    // publishStatusMQTT(); // processIRCode zaten durum değişikliğinde yayın yapıyor, burada tekrar gerekebilir veya gerekmeyebilir.
                         // İsteğe bağlı olarak, her web IR simülasyonundan sonra durumu zorla yayınlamak için açılabilir.
}

// --- BÖLÜM 19: handleNotFound() FONKSİYONU ---
// Tanımlanmayan bir URL isteği geldiğinde 404 Not Found hatası gönderir.
void handleNotFound() {
    server.send(404, "text/plain", "Not Found");
}