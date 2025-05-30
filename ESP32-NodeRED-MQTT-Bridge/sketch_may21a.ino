#include <WiFi.h>
#include <PubSubClient.h>

// WiFi Ayarları
const char* ssid = "YOUR_WIFI_NAMEt";      // WiFi adı
const char* password = "YOUR_WIFI_PASSWORD";  // WiFi şifre

// MQTT Broker Ayarları
const char* mqtt_server_ip = "MQTT_IP_ADRESS"; // BİLGİSAYARINIZIN IP ADRESİ
const int mqtt_port = "MQTT_PORT(1883)";

// ESP32'nin veri GÖNDERECEĞİ konu (Node-RED bu konuyu dinleyecek)
const char* topic_esp32_publish = "esp32/veri/giden";
// ESP32'nin veri ALACAĞI (Node-RED'den gelen komutlar için) konu
const char* topic_esp32_subscribe = "nodered/komut/gelen";

WiFiClient espClient;
PubSubClient client(espClient);

// MQTT'den mesaj geldiğinde çağrılacak fonksiyon
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mesaj geldi, konu: ");
  Serial.print(topic);
  Serial.print(". Mesaj: ");
  String messageTemp;
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    messageTemp += (char)payload[i];
  }
  Serial.println();
  Serial.println("-----------------------");

  if (String(topic) == topic_esp32_subscribe) {
    Serial.print("Node-RED'den komut alındı: ");
    Serial.println(messageTemp);
  }
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("WiFi'ye baglaniliyor: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi baglantisi kuruldu.");
  Serial.print("ESP32 IP adresi: ");
  Serial.println(WiFi.localIP());
}

void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("MQTT broker'a baglanmaya calisiliyor: ");
    Serial.print(mqtt_server_ip);
    Serial.print("...");
    String clientId = "ESP32-ManualClient-"; // Client ID'yi değiştirebilirsiniz
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("baglandi.");
      client.subscribe(topic_esp32_subscribe);
      Serial.print("Su konuya abone olundu: ");
      Serial.println(topic_esp32_subscribe);
    } else {
      Serial.print("basarisiz, rc=");
      Serial.print(client.state());
      Serial.println(" 5 saniye sonra tekrar denenecek.");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server_ip, mqtt_port);
  client.setCallback(mqtt_callback);
}

void loop() {
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop(); // MQTT istemcisini dinlemede tutar

  // Seri Port Ekranı'ndan gelen veriyi kontrol et
  if (Serial.available() > 0) {
    // Satır sonuna kadar gelen veriyi oku (Enter'a basılana kadar)
    String messageFromSerial = Serial.readStringUntil('\n');
    messageFromSerial.trim(); // Başındaki ve sonundaki boşlukları/satır sonu karakterlerini temizle

    if (messageFromSerial.length() > 0) { // Eğer boş bir mesaj değilse
      Serial.print("Seri Monitorden mesaj alindi: '");
      Serial.print(messageFromSerial);
      Serial.println("'");

      // Alınan mesajı MQTT'ye yayınla
      Serial.print("MQTT'ye yayinlaniyor, konu: ");
      Serial.print(topic_esp32_publish);
      Serial.print(", mesaj: ");
      Serial.println(messageFromSerial);

      // String'i char array'e çevirerek publish etmemiz gerekebilir, PubSubClient genellikle char* bekler.
      char msg_payload_buffer[256]; // Mesajınızın maksimum uzunluğuna göre ayarlayın
      messageFromSerial.toCharArray(msg_payload_buffer, sizeof(msg_payload_buffer));
      
      client.publish(topic_esp32_publish, msg_payload_buffer);
      Serial.println("Mesaj MQTT'ye basariyla gonderildi.");
    }
  }
}