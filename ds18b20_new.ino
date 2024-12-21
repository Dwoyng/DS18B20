#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>
#include <time.h> // Thư viện lấy thời gian thực

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ 12, /* data=*/ 14, /* reset=*/ U8X8_PIN_NONE); 

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "FPT Telecom 2.4"
#define WIFI_PASSWORD "tuanantuanduong"

// Insert Firebase project API Key
#define API_KEY "AIzaSyCm-HmgnmGyfeBlXdZy0AZuDN7HMHzmOVY"
#define DATABASE_URL "https://esp32-database-7b3e1-default-rtdb.asia-southeast1.firebasedatabase.app/" 

#define ONE_WIRE_BUS D2 // GPIO pin connected to DS18B20 sensor

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

// Hàm thiết lập múi giờ và máy chủ NTP
void setupTime() {
  configTime(25200, 0, "pool.ntp.org", "time.nist.gov"); // Múi giờ UTC+7 (25200 giây)
  Serial.println("Đang đồng bộ thời gian...");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nThời gian đồng bộ thành công!");
}

// Hàm lấy thời gian hiện tại dạng chuỗi (giờ:phút:giây)
String getCurrentTime() {
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);

  char buffer[10]; // Chỉ lấy giờ, phút, giây
  strftime(buffer, sizeof(buffer), "%H:%M:%S", p_tm);
  return String(buffer);
}

void setup() {
  u8g2.begin();
  Serial.begin(115200);
  
  sensors.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nConnected with IP: " + WiFi.localIP().toString());

  setupTime();

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase Sign-Up OK");
    signupOK = true;
  } else {
    Serial.printf("Firebase Sign-Up Error: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 1000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    sensors.requestTemperatures();
    float temperature = sensors.getTempCByIndex(0);

    if (temperature == DEVICE_DISCONNECTED_C) {
      Serial.println("Failed to read from DS18B20 sensor!");
      return;
    }

    String currentTime = getCurrentTime();

    // Hiển thị nhiệt độ và thời gian trên OLED
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(0, 8, "Temperature:");
    
    char tempStr[10];
    dtostrf(temperature, 4, 2, tempStr);
    u8g2.drawStr(0, 20, tempStr);

    u8g2.drawStr(0, 40, "Time:");
    u8g2.drawStr(0, 52, currentTime.c_str());
    u8g2.sendBuffer();

    Serial.printf("Temperature: %.2f °C\n", temperature);
    Serial.println("Time: " + currentTime);

    // Đẩy dữ liệu lên Firebase
    FirebaseJson json;
    json.set("temperature", temperature);
    json.set("time", currentTime);

    if (Firebase.RTDB.setJSON(&fbdo, "sensor/data", &json)) {
      Serial.println("Data uploaded successfully!");
    } else {
      Serial.println("Failed to upload data!");
      Serial.println(fbdo.errorReason());
    }
  }
}
