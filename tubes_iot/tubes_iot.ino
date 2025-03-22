#if defined(ESP32)
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
  #define DEVICE "ESP32"
#elif defined(ESP8266)
  #include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
  #define DEVICE "ESP8266"
#endif
  
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <LiquidCrystal_I2C.h>

// WiFi AP SSID
#define WIFI_SSID "<INSERT YOUR SSID>"
// WiFi password
#define WIFI_PASSWORD "<INSERT YOUR PASSWORD"

#define INFLUXDB_URL "<INSERT INFLUXDB URL>"
#define INFLUXDB_TOKEN "<INSERT INFLUXDB TOKEN>"
#define INFLUXDB_ORG "<INSERT INFLUXDB ORG>"
#define INFLUXDB_BUCKET "<INSERT INFLUXDB BUCKET>"

// Time zone info
#define TZ_INFO "UTC7"

// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Declare Data point
Point sensorReadings("measurements");

// Declare sensor object
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;

// Declare LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address 0x27, 16 column and 2 rows

// Variables to store previous sensor readings
double prevSuhu = 0.0;
double prevKelembaban = 0.0;
double prevTekanan = 0.0;

// Thresholds for detecting sudden changes
double suhuThreshold = 0.2; 
double kelembabanThreshold = 1.5;
double tekananThreshold = 5.0;

// Declare Buzzer
const int Buzzer = 13; // D13

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // SDA, SCL

  // Inisialisasi AHT20
  if (!aht.begin()) {
      Serial.println("❌ Gagal mendeteksi AHT20!");
  } else {
      Serial.println("✅ AHT20 terdeteksi!");
  }

  // Inisialisasi BMP280 dengan alamat 0x76 atau 0x77
  if (!bmp.begin(0x76)) {  
      Serial.println("❌ Gagal mendeteksi BMP280 di 0x76, mencoba 0x77...");
      if (!bmp.begin(0x77)) {
          Serial.println("❌ Gagal mendeteksi BMP280 di 0x77 juga!");
          while (1);
      }
  }
  Serial.println("✅ BMP280 terdeteksi!");

  // Initialize LCD I2C
  lcd.init(); // initialize the lcd
  lcd.backlight();

  // Initialize buzzer
  pinMode(Buzzer, OUTPUT);
  digitalWrite(Buzzer, LOW);

  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Add tags
  sensorReadings.addTag("device", DEVICE);
  sensorReadings.addTag("location", "campus");
  sensorReadings.addTag("sensor", "aht20+bmp280");

  // Accurate time is necessary for certificate validation and writing in batches
  // We use the NTP servers in your area as provided by: https://www.pool.ntp.org/zone/
  // Syncing progress and the time will be printed to Serial.
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void loop() {
  // Baca suhu & kelembaban dari AHT20
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  lcd.clear();

  // Get latest sensor readings
  double suhu = temp.temperature;
  double kelembaban = humidity.relative_humidity;
  double tekanan = bmp.readPressure() / 100.0F;

  // Check for sudden changes in readings
  int anomalySuhu = 0;
  int anomalyKelembaban = 0;
  int anomalyTekanan = 0;

  // Detect sudden temperature change
  if (abs(suhu - prevSuhu) > suhuThreshold) {
    anomalySuhu = 1;
  }

  // Detect sudden humidity change
  if (abs(kelembaban - prevKelembaban) > kelembabanThreshold) {
    anomalyKelembaban = 1;
  }

  // Detect sudden pressure change
  if (abs(tekanan - prevTekanan) > tekananThreshold) {
    anomalyTekanan = 1;
  }

  // Update previous sensor readings
  prevSuhu = suhu;
  prevKelembaban = kelembaban;
  prevTekanan = tekanan;

  if (((anomalySuhu == 0) && (anomalyKelembaban == 0)) && (anomalyTekanan == 0)) {
    // Print to serial
    Serial.print("AHT20 - Temperature: ");
    Serial.print(suhu); Serial.print("°C, Anomaly Temperature: ");
    Serial.println(anomalySuhu); 
    Serial.print("AHT20 - Humidity:");
    Serial.print(kelembaban); Serial.print("%, Anomaly Humidity: ");
    Serial.println(anomalyKelembaban); 

    Serial.print("🌡 BMP280 - Pressure: ");
    Serial.print(tekanan); Serial.print(" hPa, Anomaly Pressure: ");
    Serial.println(anomalyTekanan); 
    Serial.println("----------------------------");

    // Print to LCD
    lcd.setCursor(0, 0);
    lcd.print("S="); lcd.print(suhu);
    lcd.print(" K="); lcd.print(kelembaban);
    lcd.setCursor(0, 1);
    lcd.print("T="); lcd.print(tekanan);
  } else {
    // Print to serial
    Serial.print("AHT20 - Temperature: ");
    Serial.print(suhu); Serial.print("°C, Anomaly Temperature: ");
    Serial.println(anomalySuhu); 
    Serial.print("AHT20 - Humidity:");
    Serial.print(kelembaban); Serial.print("%, Anomaly Humidity: ");
    Serial.println(anomalyKelembaban); 

    Serial.print("BMP280 - Pressure: ");
    Serial.print(tekanan); Serial.print(" hPa, Anomaly Pressure: ");
    Serial.println(anomalyTekanan); 
    Serial.println("----------------------------");

    // Buzzer ON
    digitalWrite(Buzzer, HIGH);
    delay(100);
    digitalWrite(Buzzer, LOW);
    delay(100);
    digitalWrite(Buzzer, HIGH);
    delay(100);
    digitalWrite(Buzzer, LOW);
    delay(100);

    // Print to LCD
    lcd.setCursor(0, 0); lcd.print("ANOMALY");
    lcd.setCursor(0, 1); lcd.print("DETECTED!");
  }

  // Add readings as fields to point
  sensorReadings.addField("temperature", suhu);
  sensorReadings.addField("humidity", kelembaban);
  sensorReadings.addField("pressure", tekanan);
  sensorReadings.addField("anomalyTemperature", anomalySuhu);
  sensorReadings.addField("anomalyHumidity", anomalyKelembaban);
  sensorReadings.addField("anomalyPressure", anomalyTekanan);

  // Write point into buffer
  client.writePoint(sensorReadings);

  // Clear fields for next usage. Tags remain the same.
  sensorReadings.clearFields();

  // If no Wifi signal, try to reconnect it
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }

  delay(2500);
}
