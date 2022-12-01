#include <Arduino.h>
#include <Wire.h>
#include <SPIFFS.h>
#include <SD.h>
#include <Update.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// // include firebase dan wifi
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#define WIFI_SSID "KONTRAKAN UYE" // ini adalah nama wifi
#define WIFI_PASSWORD "KUSANG123" // dan ini adalah passwordnya. kosongkan bagian ini kalau tidak pakai password
#define API_KEY "AIzaSyBsVj4YXqGT7PexdZ0QD1wK2UUjRtPk878"
#define DATABASE_URL "bangfaisal-1-default-rtdb.asia-southeast1.firebasedatabase.app" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app
#define USER_EMAIL "ahmadyusufmaulana0@gmail.com"
#define USER_PASSWORD "yusuf1112"

// Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;

// nilai variabel untuk sensor tegangan
float nilaiTeganganGenerator = 0.0;
float nilaiTeganganBaterai = 0.0;

// boolean untuk relay upload ke firebase
bool keadaanRelayBaterai = false;
bool keadaanRelayGenerator = false;

// ini adalah variabel nilai daya generator
float dayaMasukDariGenerator = 0;

// variabel counter dengan menggunakan ir sensor
int keadaanSensor = 0;
int keadaanTekan = 0;
int jumlahTonjolan = 0;  // jumlah polisi tidur tertekan kebawah
int jumlahKendaraan = 0; // adalah jumlah kendaraan dengan membagi jumlah jonjolan dengan dua

// variabel untuk sensor arus generator dan baterai
float nilaiArusGenerator = 0.0;
float nilaiArusBaterai = 0.0;

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup()
{
    Serial.begin(115200);
    // memulai komunikasi dengan wifi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    lcd.setCursor(0, 0);
    lcd.print("connecting     ");
    lcd.setCursor(0, 1);
    lcd.print("               ");
    delay(3000);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        lcd.setCursor(0, 1);
        lcd.print(".");
        delay(3000);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();
    lcd.setCursor(0, 0);
    lcd.print("connected!?    ");
    lcd.setCursor(0, 1);
    lcd.print("IP:");
    lcd.print(WiFi.localIP());
    delay(1000);

    config.api_key = API_KEY;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
    config.database_url = DATABASE_URL;
    config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    Firebase.setDoubleDigits(5);

    // setting inisialisasi sensor IR
    pinMode(18, INPUT);
    // inisialisasi sensor tegangan
    pinMode(34, INPUT); // baterai
    pinMode(35, INPUT); // generator
    // inisialisasi sensor arus
    pinMode(33, INPUT); // generator
    pinMode(32, INPUT); // baterai
    // inisialisasi relay
    pinMode(4, OUTPUT); // generator
    pinMode(5, OUTPUT); // baterai
    // buat nilai kedua relay high secara default
    digitalWrite(4, HIGH); // baterai
    digitalWrite(5, HIGH); // generator

    // Initialize the LCD connected
    lcd.init();
    // Turn on the backlight on LCD.
    lcd.backlight();
}

void loop()
{
    // kode untuk menghitung jumlah kendaraan
    keadaanSensor = digitalRead(18);
    if (keadaanSensor == HIGH && keadaanTekan == 0)
    {
        jumlahTonjolan++;
        keadaanTekan = 1;
    }
    else if (keadaanSensor == LOW)
        keadaanTekan = 0;
    // bagi dua nilai jumlah tonjolan sehingga didapatkan nilai jumlah kendaraan :)
    jumlahKendaraan = jumlahTonjolan / 2;

    // rumus mencari nilai arus
    int analogArusGenerator = analogRead(33);
    int analogArusBaterai = analogRead(32);
    nilaiArusGenerator = (1.67 - ((analogArusGenerator + 205.3) * (3.3 / 4095.0))) / 0.1;
    nilaiArusBaterai = (0.65 - ((analogArusBaterai + 211.6) * (3.3 / 4095.0))) / 0.185;
    // jaga nilai arus agar tetap tinggi
    if (nilaiArusGenerator < 0)
        nilaiArusGenerator = nilaiArusGenerator * -1;
    if (nilaiArusBaterai < 0)
        nilaiArusBaterai = nilaiArusBaterai * -1;

    // rumus mencari nilai tegangan
    int analogTeganganGenerator = analogRead(35);
    int analogTeganganBaterai = analogRead(34);
    // nilai 0.2 adalah penyederhanaan dari = (R2 / (R1 + R2))
    //                                      = (7500 / (30000 + 7500))
    //                                      = 0.2
    // dan 1.08 adalah nilai kalibrasi tegangan (karena grafik adc esp32 tidak linear sempurna)
    nilaiTeganganGenerator = (analogTeganganGenerator * (3.3 / 4096.0)) / 0.2 * 1.08;
    nilaiTeganganBaterai = (analogTeganganBaterai * (3.3 / 4096.0)) / 0.2 * 1.08;

    // perintah menemukan daya dari generator
    dayaMasukDariGenerator = nilaiArusGenerator * nilaiTeganganGenerator;

    // perintah untuk mematikan dan menyalakan relay jika mencapai voltase tertentu
    // 12.23 adalah nilai tegangan minimum baterai lead acid (baterai kosong)
    if (nilaiTeganganBaterai < 12.23)
    {
        digitalWrite(4, LOW);
        keadaanRelayBaterai = false;
    }
    else
    {
        digitalWrite(4, HIGH);
        keadaanRelayBaterai = true;
    }
    // 15 adalah nilai tegangan tertinggi yang masih aman untuk stepdown
    if (nilaiTeganganGenerator > 14.5)
    {
        digitalWrite(5, LOW);
        keadaanRelayGenerator = false;
    }
    else
    {
        digitalWrite(5, HIGH);
        keadaanRelayGenerator = true;
    }

    // tampilkan semua nilai yang penting di serial monitor
    Serial.print("Agen: ");
    Serial.print(nilaiArusGenerator);
    Serial.print(" Abat: ");
    Serial.print(nilaiArusBaterai);
    Serial.print(" Vgen: ");
    Serial.print(nilaiTeganganGenerator);
    Serial.print(" Vbat: ");
    Serial.print(nilaiTeganganBaterai);
    Serial.print(" Pgen: ");
    Serial.println(dayaMasukDariGenerator);

    // tampilan untuk lcd
    // baris pertama
    lcd.setCursor(0, 0);
    // lcd.print("G: ");
    lcd.print(nilaiArusGenerator);
    lcd.print("A");
    lcd.print(nilaiTeganganGenerator);
    lcd.print("V");
    lcd.print(dayaMasukDariGenerator);
    lcd.print("W      ");
    // baris kedua
    lcd.setCursor(0, 1);
    // lcd.print("B: ");
    lcd.print(nilaiArusBaterai);
    lcd.print("A");
    lcd.print(nilaiTeganganBaterai);
    lcd.print("V");
    lcd.print(jumlahKendaraan);
    lcd.print("K      ");

    // ini kode untuk upload kode ke firebase
    if (Firebase.ready() && (millis() - sendDataPrevMillis > 10000 || sendDataPrevMillis == 0))
    {
        sendDataPrevMillis = millis();
        Firebase.RTDB.setFloat(&fbdo, "/DayaGenerator", dayaMasukDariGenerator);
        Firebase.RTDB.setFloat(&fbdo, "/VGenerator", nilaiTeganganGenerator);
        Firebase.RTDB.setFloat(&fbdo, "/VBaterai", nilaiTeganganBaterai);
        Firebase.RTDB.setFloat(&fbdo, "/Agenerator", nilaiArusGenerator);
        Firebase.RTDB.setFloat(&fbdo, "/ABaterai", nilaiArusBaterai);
        Firebase.RTDB.setBool(&fbdo, "/RelayGenerator", keadaanRelayGenerator);
        Firebase.RTDB.setBool(&fbdo, "/RelayBaterai", keadaanRelayBaterai);
        Firebase.RTDB.setInt(&fbdo, "/JumlahKendaraan", jumlahKendaraan);
    }
    // delay(350); // untuk memudahkan pembacaan data dan upload ke firebase #unkomen jika dibutuhkan
}
// END OF FILE //