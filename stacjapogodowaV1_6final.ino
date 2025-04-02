#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>  // Library to handle JSON responses
#include <ESP8266HTTPClient.h>  // Correct HTTP library
#include <time.h>
#include <DHT.h>
#include <ArduinoOTA.h>  // Biblioteka OTA
#include <ESP8266WebServer.h>
int val=1;
String previousTime = ""; // Dodaj zmienną globalną do przechowywania poprzedniej godziny

const uint16_t WAIT_TIME = 1000;

// Display settings
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CLK_PIN   D6
#define DATA_PIN  D7
#define CS_PIN    D8

// Wi-Fi
const char* ssid = "WIFISSID";
const char* password = "PASSWIFI";

//serwer WWW
ESP8266WebServer server(80);  // Serwer HTTP na porcie 80

// API parameters
String my_Api_Key = "APIKEY";
String my_city = "Pruszkow";
String my_country_code = "PL";

// Konfiguracja DHT22
#define DHTPIN D6
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// NTP settings
const char* ntpServer = "pool.ntp.org";
long gmtOffset_sec = 3600;
int daylightOffset_sec = 3600; // Dodatkowa godzina na czas letni

// Display object
MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

WiFiClient wifiClient;  // Create a Wi-Fi client

// Function to fetch weather data
String getWeatherData() {
  HTTPClient http;
  String weatherURL = "http://api.openweathermap.org/data/2.5/weather?q=" + my_city + "," + my_country_code + "&appid=" + my_Api_Key;

  http.begin(wifiClient, weatherURL);
  int httpCode = http.GET();

  String payload = "";
  if (httpCode > 0) {
    payload = http.getString();
  }
  http.end();
  return payload;
}

void setup(void) {
  P.begin();
  Serial.begin(115200);
  dht.begin();  // Inicjalizacja DHT
  P.setIntensity(0);  // Ustawienie jasności

  // Łączenie z Wi-Fi
  P.displayClear();
  P.displayScroll("Wi-Fi...", PA_LEFT, PA_SCROLL_LEFT, 100);
  while (!P.displayAnimate()) { yield(); }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Komunikat po połączeniu
  Serial.println("Wi-Fi!");
  P.displayClear();
  P.displayScroll("Wi-Fi!", PA_LEFT, PA_SCROLL_LEFT, 100);
  while (!P.displayAnimate()) { yield(); }

  // Konfiguracja strony głównej serwera
  server.on("/", handleRoot);
  server.begin();

  // Set NTP time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  ArduinoOTA.setHostname("NodeMCU-OTA");  // Nazwa urządzenia w sieci
  ArduinoOTA.setPassword("OTAPASS");  // Ustaw hasło OTA

  ArduinoOTA.begin();  // Rozpocznij obsługę OTA
}

// Funkcja generująca stronę HTML z danymi pogodowymi
void handleRoot() {
  String weatherJson = getWeatherData();
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, weatherJson);

  // Odczyt danych z DHT22
  float dhtTemperature = dht.readTemperature();  // Domyślnie w stopniach Celsjusza
  float dhtHumidity = dht.readHumidity();

  if (!error) {
    float temperature = doc["main"]["temp"].as<float>() - 273.15;
    float tempMin = doc["main"]["temp_min"].as<float>() - 273.15;
    float tempMax = doc["main"]["temp_max"].as<float>() - 273.15;
    int humidity = doc["main"]["humidity"].as<int>();
    float windSpeed = doc["wind"]["speed"].as<float>() * 3.6;
    int pressure = doc["main"]["pressure"].as<int>();
    int cloudiness = doc["clouds"]["all"].as<int>();

    // Budowanie strony HTML
    String html = "<html><head>";
    html += "<meta charset=\"UTF-8\">";
    html += "<title>Weather Station</title></head><body>";
    html += "<h1>Dane pogodowe</h1>";
    html += "<p>Temperatura: " + String(temperature, 1) + " &deg;C</p>";
    html += "<p>Temp. min: " + String(tempMin, 1) + " &deg;C</p>";
    html += "<p>Temp. max: " + String(tempMax, 1) + " &deg;C</p>";
    html += "<p>Wilgotność: " + String(humidity) + " %</p>";
    html += "<p>Prędkość wiatru: " + String(windSpeed, 1) + " km/h</p>";
    html += "<p>Ciśnienie: " + String(pressure) + " hPa</p>";
    html += "<p>Zachmurzenie: " + String(cloudiness) + " %</p>";

    // Dodanie danych z DHT22
    html += "<h2>Dane z czujnika DHT22</h2>";
    if (isnan(dhtTemperature) || isnan(dhtHumidity)) {
      html += "<p>Błąd odczytu z DHT22</p>";
    } else {
      html += "<p>Temperatura wewnętrzna: " + String(dhtTemperature, 1) + " &deg;C</p>";
      html += "<p>Wilgotność wewnętrzna: " + String(dhtHumidity, 1) + " %</p>";
    }

    html += "</body></html>";
    server.send(200, "text/html", html);
  } else {
    server.send(500, "text/plain", "Błąd deserializacji danych pogodowych");
  }
}

void loop(void) {
  // Sprawdzenie połączenia Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    P.displayClear();
    P.displayScroll("Brak Wi-Fi", PA_LEFT, PA_SCROLL_LEFT, 100);
    while (!P.displayAnimate()) { yield(); }
    delay(WAIT_TIME);
    return;  // Spróbuj ponownie w następnej iteracji
  }

  ArduinoOTA.handle();  // Obsługa OTA

  server.handleClient();  // Obsługa przychodzących zapytań HTTP

  // Display Time and Date
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeString[6];  // Format "HH:MM"
    strftime(timeString, sizeof(timeString), "%H:%M", &timeinfo);
    
    // Sprawdzenie, czy godzina się zmieniła
    if (previousTime != String(timeString)) {
      previousTime = String(timeString);  // Zaktualizuj poprzednią godzinę
      P.displayClear();
      P.displayText(timeString, PA_CENTER, 100, 0, PA_PRINT, PA_NO_EFFECT);
      while (!P.displayAnimate()) { yield(); }
    }
  }
    delay(WAIT_TIME);
    if (val==1) {

    char dateString[11];
    strftime(dateString, sizeof(dateString), "%d/%m/%Y", &timeinfo);
    P.displayClear();
    P.displayScroll(dateString, PA_LEFT, PA_SCROLL_LEFT, 100);
    while (!P.displayAnimate()) { yield(); }
    delay(WAIT_TIME);
  

  // Fetch and display weather data
  String weatherJson = getWeatherData();
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, weatherJson);
  if (!error) {
    float temperature = doc["main"]["temp"].as<float>() - 273.15;
    float tempMin = doc["main"]["temp_min"].as<float>() - 273.15;  // Dodane
    float tempMax = doc["main"]["temp_max"].as<float>() - 273.15;  // Dodane
    int humidity = doc["main"]["humidity"].as<int>();
    float windSpeed = doc["wind"]["speed"].as<float>() * 3.6;
    int pressure = doc["main"]["pressure"].as<int>();
    int cloudiness = doc["clouds"]["all"].as<int>();
    // Pobieranie opisu pogody
    String weatherDescription = doc["weather"][0]["description"].as<String>();

    String weatherInfo[] = {
      "T out: " + String(temperature, 1) + "C",
      "T min: " + String(tempMin, 1) + "C",  // Nowy komunikat
      "T max: " + String(tempMax, 1) + "C",  // Nowy komunikat
      "H out: " + String(humidity) + "%",
      "W: " + String(windSpeed, 1) + " km/h",
      "P: " + String(pressure) + " hPa",
      "C: " + String(cloudiness) + "%",
      "Opis: " + weatherDescription  // Dodanie opisu pogody
    };

    for (int i = 0; i < 8; i++) {
      P.displayClear();
      P.displayScroll(weatherInfo[i].c_str(), PA_LEFT, PA_SCROLL_LEFT, 100);
      while (!P.displayAnimate()) { yield(); }
      delay(WAIT_TIME);
    }
  }

  // Wyświetlanie danych z DHT22
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (!isnan(temp) && !isnan(humidity)) {
    String tempStr = "T in: " + String(temp, 1) + "C";
    String humidityStr = "H in: " + String(humidity) + "%";

    P.displayClear();
    P.displayScroll(tempStr.c_str(), PA_LEFT, PA_SCROLL_LEFT, 100);
    while (!P.displayAnimate()) { yield(); }
    delay(WAIT_TIME);

    P.displayClear();
    P.displayScroll(humidityStr.c_str(), PA_LEFT, PA_SCROLL_LEFT, 100);
    while (!P.displayAnimate()) { yield(); }
    delay(WAIT_TIME);
  }
  }
  val++;
  Serial.println(val);  // Wyświetlenie wartości val w porcie szeregowym
  if (val >= 300) {
    val =1;
  }
}