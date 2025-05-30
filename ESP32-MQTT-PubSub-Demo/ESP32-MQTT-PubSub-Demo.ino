#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "RASPBERRY PI IP ADRESS"; // Raspberry Pi'nin IP adresi
const int mqtt_port = MQTT PORT;
const char* mqtt_client_id = "esp32-basit";
const char* publish_topic = "merhaba/giden";   // ESP32'den gönderilen mesajlar
const char* subscribe_topic = "merhaba/gelen"; // ESP32'ye gönderilen mesajlar

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  Serial.begin(115200);
  delay(10);
  Serial.println("WiFi'ye bağlanılıyor...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi'ye bağlandı");
  Serial.print("IP adresi: ");gem
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Gelen mesaj [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void mqtt_connect() {
  while (!client.connected()) {
    Serial.print("MQTT broker'ına bağlanılıyor...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("bağlandı");
      client.subscribe(subscribe_topic); // Abone ol
    } else {
      Serial.print("Bağlanma başarısız oldu, rc=");
      Serial.print(client.state());
      Serial.println(" 5 saniye sonra tekrar denenecek");
      delay(5000);
    }
  }
}

void setup() {
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    mqtt_connect();
  }
  client.loop();
  client.publish(publish_topic, "Merhaba Arduino'dan");
  delay(5000);
}