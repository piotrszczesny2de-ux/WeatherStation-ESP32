/*
 * Stacja Pogodowa - ESP32 + ILI9341 (BEZ dotyku)
 * API: Open-Meteo (darmowe, BEZ klucza API!)
 *
 * 3 ekrany (przelaczanie automatyczne co 10s):
 *   1. Pogoda teraz
 *   2. Prognoza 3 dni
 *   3. Szczegoly (wilgotnosc, cisnienie, wiatr)
 *
 * Podlaczenie ILI9341 -> ESP32:
 *   VCC   -> 3.3V
 *   GND   -> GND
 *   CS    -> GPIO 15
 *   RESET -> GPIO 4
 *   DC    -> GPIO 2
 *   MOSI  -> GPIO 23
 *   SCK   -> GPIO 18
 *   LED   -> 3.3V
 *   SDO   -> GPIO 19
 *
 * Wymagane biblioteki:
 *   - TFT_eSPI (skonfiguruj User_Setup.h!)
 *   - ArduinoJson (v7)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <time.h>

// ============ KONFIGURACJA ============
const char* WIFI_SSID     = "TwojaSiec"       // <-- zmien na swoja siec WiFi;
const char* WIFI_PASSWORD = "TwojeHaslo"      // <-- zmien na swoje haslo WiFi;

const float LATITUDE  = 54.15;  // Elblag
const float LONGITUDE = 19.40;
const char* CITY_NAME = "Elblag";
// ======================================

// --- Open-Meteo URL ---
String WEATHER_URL = "https://api.open-meteo.com/v1/forecast?"
  "latitude=" + String(LATITUDE, 2) +
  "&longitude=" + String(LONGITUDE, 2) +
  "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
  "weather_code,wind_speed_10m,wind_direction_10m,surface_pressure"
  "&daily=temperature_2m_max,temperature_2m_min,weather_code"
  "&forecast_days=3"
  "&timezone=Europe%2FWarsaw";

const unsigned long UPDATE_INTERVAL = 300000; // 5 minut
const unsigned long SCREEN_INTERVAL = 10000;  // 10 sekund na ekran
unsigned long lastUpdate = 0;
unsigned long lastScreenChange = 0;

TFT_eSPI tft = TFT_eSPI();

// --- Ekrany ---
int currentScreen = 0;
const int NUM_SCREENS = 3;

// --- Dane pogodowe ---
struct WeatherData {
  float temp;
  float feels_like;
  int humidity;
  float wind_speed;
  int wind_dir;
  int pressure;
  int weather_code;
  bool valid;
};

struct ForecastDay {
  float temp_max;
  float temp_min;
  int weather_code;
  char date[11];
};

WeatherData weather;
ForecastDay forecast[3];

// --- Kolory ---
#define BG_COLOR      0x0000
#define TEXT_COLOR    0xFFFF
#define TEMP_COLOR    0xFD20
#define TEMP_MIN_COL  0x5D1F
#define HUMID_COLOR   0x07FF
#define WIND_COLOR    0x07E0
#define PRESS_COLOR   0xF81F
#define HEADER_COLOR  0x001F
#define LABEL_COLOR   0xAD55
#define LINE_COLOR    0x4208
#define DOT_ACTIVE    0xFD20
#define DOT_INACTIVE  0x4208

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(BG_COLOR);

  showStartScreen();
  connectWiFi();

  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  int retry = 0;
  while (!getLocalTime(&timeinfo) && retry < 10) { delay(500); retry++; }

  if (fetchWeather()) {
    drawScreen();
  }
  lastScreenChange = millis();
}

// ===================== LOOP =====================
void loop() {
  unsigned long now = millis();

  // --- Automatyczne przelaczanie ekranow co 10s ---
  if (now - lastScreenChange >= SCREEN_INTERVAL) {
    lastScreenChange = now;
    currentScreen = (currentScreen + 1) % NUM_SCREENS;
    drawScreen();
  }

  // --- Odswiezanie pogody co 5 min ---
  if (now - lastUpdate >= UPDATE_INTERVAL || lastUpdate == 0) {
    lastUpdate = now;

    if (WiFi.status() != WL_CONNECTED) {
      connectWiFi();
    }

    if (fetchWeather()) {
      drawScreen();
    } else {
      showError("Blad pobierania danych!");
    }
  }

  delay(100);
}

// ============ RYSUJ AKTUALNY EKRAN ============
void drawScreen() {
  tft.startWrite();
  tft.fillScreen(BG_COLOR);

  switch (currentScreen) {
    case 0: displayWeatherNow(); break;
    case 1: displayForecast();   break;
    case 2: displayDetails();    break;
  }

  drawPageDots();
  tft.endWrite();
}

// ============ KROPKI STRON ============
void drawPageDots() {
  int dotY = 232;
  int startX = 160 - (NUM_SCREENS * 10);

  for (int i = 0; i < NUM_SCREENS; i++) {
    uint16_t color = (i == currentScreen) ? DOT_ACTIVE : DOT_INACTIVE;
    int dotX = startX + i * 20;
    tft.fillCircle(dotX, dotY, 4, color);
  }
}

// ============ NAGLOWEK Z GODZINA ============
void drawHeader(const char* title) {
  tft.fillRect(0, 0, 320, 36, HEADER_COLOR);
  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print(title);

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    tft.setCursor(248, 10);
    tft.print(timeStr);
  }
}

// =========================================================
//  EKRAN 1: POGODA TERAZ
// =========================================================
void displayWeatherNow() {
  drawHeader(CITY_NAME);

  drawWeatherIcon(30, 50, weather.weather_code);

  tft.setTextColor(TEMP_COLOR);
  tft.setTextSize(4);
  tft.setCursor(100, 50);
  char tempStr[10];
  dtostrf(weather.temp, 4, 1, tempStr);
  tft.print(tempStr);
  tft.setTextSize(2);
  tft.print(" C");

  tft.setTextSize(2);
  int tempEndX = 100 + strlen(tempStr) * 24;
  tft.setCursor(tempEndX, 48);
  tft.print("o");

  const char* desc = getWeatherDescription(weather.weather_code);
  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(2);
  tft.setCursor(100, 90);
  tft.print(desc);

  tft.drawLine(10, 120, 310, 120, LINE_COLOR);

  tft.setTextColor(LABEL_COLOR);
  tft.setTextSize(2);
  tft.setCursor(15, 128);
  tft.print("Odczuw:");
  tft.setTextColor(TEMP_COLOR);
  tft.setTextSize(2);
  tft.setCursor(110, 128);
  dtostrf(weather.feels_like, 4, 1, tempStr);
  tft.print(tempStr);
  tft.setTextSize(1);
  tft.print("o");
  tft.setTextSize(2);
  tft.print("C");

  tft.drawLine(10, 148, 310, 148, LINE_COLOR);

  int y = 155;
  drawInfoRow(15, y, "Wilgotnosc", String(weather.humidity) + " %", HUMID_COLOR);
  y += 24;
  drawInfoRow(15, y, "Wiatr", String(weather.wind_speed, 1) + " km/h", WIND_COLOR);
  y += 24;
  drawInfoRow(15, y, "Cisnienie", String(weather.pressure) + " hPa", PRESS_COLOR);
}

// =========================================================
//  EKRAN 2: PROGNOZA 3 DNI
// =========================================================
void displayForecast() {
  drawHeader("PROGNOZA 3 DNI");

  for (int i = 0; i < 3; i++) {
    drawForecastDay(i);
  }
}

void drawForecastDay(int i) {
  int y = 46 + i * 62;

  // Flush i poczekaj przed kazdym dniem
  tft.endWrite();
  delay(20);
  tft.startWrite();

  const char* dayName = getDayName(forecast[i].date);
  tft.setTextColor(TEXT_COLOR, BG_COLOR);
  tft.setTextSize(2);
  tft.setCursor(10, y + 4);
  tft.print(dayName);

  tft.setTextColor(LABEL_COLOR, BG_COLOR);
  tft.setTextSize(1);
  tft.setCursor(10, y + 26);
  char shortDate[6];
  snprintf(shortDate, sizeof(shortDate), "%c%c.%c%c",
    forecast[i].date[8], forecast[i].date[9],
    forecast[i].date[5], forecast[i].date[6]);
  tft.print(shortDate);

  tft.endWrite();
  delay(5);
  tft.startWrite();

  drawWeatherIconSmall(120, y, forecast[i].weather_code);

  char maxStr[8];
  dtostrf(forecast[i].temp_max, 3, 0, maxStr);
  tft.setTextColor(TEMP_COLOR, BG_COLOR);
  tft.setTextSize(2);
  tft.setCursor(185, y + 4);
  tft.print(maxStr);
  tft.setTextSize(1);
  tft.print("o");

  char minStr[8];
  dtostrf(forecast[i].temp_min, 3, 0, minStr);
  tft.setTextColor(TEMP_MIN_COL, BG_COLOR);
  tft.setTextSize(2);
  tft.setCursor(245, y + 4);
  tft.print(minStr);
  tft.setTextSize(1);
  tft.print("o");

  tft.setTextColor(LABEL_COLOR, BG_COLOR);
  tft.setTextSize(1);
  tft.setCursor(185, y + 30);
  tft.print(getWeatherDescription(forecast[i].weather_code));

  if (i < 2) {
    tft.drawLine(5, y + 54, 315, y + 54, LINE_COLOR);
  }

  tft.endWrite();
  delay(5);
  tft.startWrite();
}

// =========================================================
//  EKRAN 3: SZCZEGOLY
// =========================================================
void displayDetails() {
  drawHeader("SZCZEGOLY");

  int y = 48;
  int spacing = 36;

  drawDetailBlock(y, "Temperatura", weather.temp, "C", TEMP_COLOR, true);
  y += spacing;

  drawDetailBlock(y, "Odczuwalna", weather.feels_like, "C", TEMP_COLOR, true);
  y += spacing;

  tft.setTextColor(LABEL_COLOR);
  tft.setTextSize(2);
  tft.setCursor(15, y);
  tft.print("Wilgotn.");
  tft.setTextColor(HUMID_COLOR);
  tft.setCursor(160, y);
  tft.printf("%d %%", weather.humidity);
  tft.drawRect(160, y + 20, 140, 6, HUMID_COLOR);
  tft.fillRect(160, y + 20, map(weather.humidity, 0, 100, 0, 140), 6, HUMID_COLOR);
  y += spacing + 6;

  tft.setTextColor(LABEL_COLOR);
  tft.setTextSize(2);
  tft.setCursor(15, y);
  tft.print("Cisn.");
  tft.setTextColor(PRESS_COLOR);
  tft.setCursor(160, y);
  tft.printf("%d hPa", weather.pressure);
  y += spacing;

  tft.setTextColor(LABEL_COLOR);
  tft.setTextSize(2);
  tft.setCursor(15, y);
  tft.print("Wiatr");
  tft.setTextColor(WIND_COLOR);
  tft.setCursor(160, y);
  tft.printf("%.1f km/h", weather.wind_speed);

  tft.setTextColor(LABEL_COLOR);
  tft.setTextSize(1);
  tft.setCursor(160, y + 20);
  tft.printf("Kierunek: %s (%d)", getWindDirection(weather.wind_dir), weather.wind_dir);
}

// ============ BLOK SZCZEGOLU ============
void drawDetailBlock(int y, const char* label, float value, const char* unit, uint16_t color, bool showDegree) {
  tft.setTextColor(LABEL_COLOR);
  tft.setTextSize(2);
  tft.setCursor(15, y);
  tft.print(label);

  tft.setTextColor(color);
  tft.setTextSize(2);
  tft.setCursor(160, y);
  char valStr[10];
  dtostrf(value, 4, 1, valStr);
  tft.print(valStr);
  if (showDegree) {
    tft.setTextSize(1);
    tft.print("o");
    tft.setTextSize(2);
  }
  tft.print(unit);
}

// ============ KIERUNEK WIATRU ============
const char* getWindDirection(int deg) {
  if (deg >= 338 || deg < 23)  return "N";
  if (deg < 68)                return "NE";
  if (deg < 113)               return "E";
  if (deg < 158)               return "SE";
  if (deg < 203)               return "S";
  if (deg < 248)               return "SW";
  if (deg < 293)               return "W";
  return "NW";
}

// ============ DZIEN TYGODNIA ============
const char* getDayName(const char* dateStr) {
  int y, m, d;
  sscanf(dateStr, "%d-%d-%d", &y, &m, &d);
  if (m < 3) { m += 12; y--; }
  int dow = (d + (13 * (m + 1)) / 5 + y + y / 4 - y / 100 + y / 400) % 7;
  switch (dow) {
    case 0: return "Sob";
    case 1: return "Nie";
    case 2: return "Pon";
    case 3: return "Wto";
    case 4: return "Sro";
    case 5: return "Czw";
    case 6: return "Pia";
    default: return "???";
  }
}

// ============ WIFI ============
void connectWiFi() {
  tft.fillScreen(BG_COLOR);
  tft.setTextColor(TEXT_COLOR, BG_COLOR);
  tft.setTextSize(2);
  tft.setCursor(20, 100);
  tft.print("Laczenie z WiFi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
    dots++;
    if (dots > 20) {
      tft.fillScreen(BG_COLOR);
      tft.setCursor(20, 100);
      tft.print("Laczenie z WiFi");
      dots = 0;
    }
    Serial.print(".");
  }
  Serial.println("\nWiFi polaczone!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ============ POBIERANIE POGODY ============
bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.begin(WEATHER_URL);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.printf("HTTP error: %d\n", httpCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("JSON error: ");
    Serial.println(error.c_str());
    return false;
  }

  JsonObject current = doc["current"];
  weather.temp         = current["temperature_2m"];
  weather.feels_like   = current["apparent_temperature"];
  weather.humidity     = current["relative_humidity_2m"];
  weather.wind_speed   = current["wind_speed_10m"];
  weather.wind_dir     = current["wind_direction_10m"];
  weather.pressure     = (int)((float)current["surface_pressure"]);
  weather.weather_code = current["weather_code"];
  weather.valid        = true;

  JsonObject daily = doc["daily"];
  JsonArray dates    = daily["time"];
  JsonArray maxTemps = daily["temperature_2m_max"];
  JsonArray minTemps = daily["temperature_2m_min"];
  JsonArray codes    = daily["weather_code"];

  for (int i = 0; i < 3 && i < dates.size(); i++) {
    strncpy(forecast[i].date, dates[i].as<const char*>(), 10);
    forecast[i].date[10] = '\0';
    forecast[i].temp_max     = maxTemps[i];
    forecast[i].temp_min     = minTemps[i];
    forecast[i].weather_code = codes[i];
  }

  Serial.printf("Pogoda: %.1f C, kod: %d\n", weather.temp, weather.weather_code);
  return true;
}

// ============ WMO KOD -> OPIS ============
const char* getWeatherDescription(int code) {
  if (code == 0)                    return "Czyste niebo";
  if (code == 1)                    return "Prawie czyste";
  if (code == 2)                    return "Cz. pochmurno";
  if (code == 3)                    return "Pochmurno";
  if (code == 45 || code == 48)     return "Mgla";
  if (code == 51)                   return "Lekka mzawka";
  if (code == 53)                   return "Mzawka";
  if (code == 55)                   return "Gesta mzawka";
  if (code == 56 || code == 57)     return "Mzawka marznaca";
  if (code == 61)                   return "Lekki deszcz";
  if (code == 63)                   return "Deszcz";
  if (code == 65)                   return "Silny deszcz";
  if (code == 66 || code == 67)     return "Deszcz marznacy";
  if (code == 71)                   return "Lekki snieg";
  if (code == 73)                   return "Snieg";
  if (code == 75)                   return "Silny snieg";
  if (code == 77)                   return "Ziarna sniegowe";
  if (code == 80)                   return "Lekkie opady";
  if (code == 81)                   return "Opady deszczu";
  if (code == 82)                   return "Silne opady";
  if (code == 85 || code == 86)     return "Opady sniegu";
  if (code == 95)                   return "Burza";
  if (code == 96 || code == 99)     return "Burza z gradem";
  return "Nieznane";
}

// ============ WIERSZ INFO ============
void drawInfoRow(int x, int y, const char* label, String value, uint16_t color) {
  tft.setTextColor(LABEL_COLOR);
  tft.setTextSize(2);
  tft.setCursor(x, y);
  tft.print(label);

  tft.setTextColor(color);
  tft.setTextSize(2);
  tft.setCursor(x + 145, y);
  tft.print(value);
}

// ============ IKONA POGODY (duza) ============
void drawWeatherIcon(int x, int y, int code) {
  if (code == 0 || code == 1) {
    tft.fillCircle(x + 25, y + 20, 14, TFT_YELLOW);
    for (int i = 0; i < 8; i++) {
      float angle = i * PI / 4;
      int x1 = x + 25 + cos(angle) * 18;
      int y1 = y + 20 + sin(angle) * 18;
      int x2 = x + 25 + cos(angle) * 24;
      int y2 = y + 20 + sin(angle) * 24;
      tft.drawLine(x1, y1, x2, y2, TFT_YELLOW);
    }
  }
  else if (code == 2) {
    tft.fillCircle(x + 15, y + 12, 10, TFT_YELLOW);
    tft.fillCircle(x + 30, y + 22, 10, TFT_LIGHTGREY);
    tft.fillCircle(x + 20, y + 26, 8, TFT_LIGHTGREY);
    tft.fillCircle(x + 38, y + 26, 8, TFT_LIGHTGREY);
    tft.fillRect(x + 18, y + 26, 22, 10, TFT_LIGHTGREY);
  }
  else if (code == 3) {
    tft.fillCircle(x + 25, y + 16, 12, TFT_DARKGREY);
    tft.fillCircle(x + 15, y + 22, 10, TFT_LIGHTGREY);
    tft.fillCircle(x + 35, y + 22, 10, TFT_LIGHTGREY);
    tft.fillRect(x + 13, y + 22, 24, 12, TFT_LIGHTGREY);
  }
  else if (code == 45 || code == 48) {
    for (int i = 0; i < 4; i++) {
      tft.drawLine(x + 5, y + 10 + i * 8, x + 45, y + 10 + i * 8, TFT_LIGHTGREY);
    }
  }
  else if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67) || (code >= 80 && code <= 82)) {
    tft.fillCircle(x + 25, y + 12, 10, TFT_DARKGREY);
    tft.fillCircle(x + 15, y + 16, 8, TFT_DARKGREY);
    tft.fillCircle(x + 35, y + 16, 8, TFT_DARKGREY);
    tft.fillRect(x + 13, y + 16, 24, 8, TFT_DARKGREY);
    int drops = (code >= 65 || code == 82) ? 4 : (code >= 53 ? 3 : 2);
    for (int i = 0; i < drops; i++) {
      tft.drawLine(x + 12 + i * 10, y + 28, x + 10 + i * 10, y + 36, HUMID_COLOR);
    }
  }
  else if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) {
    tft.fillCircle(x + 25, y + 12, 10, TFT_LIGHTGREY);
    tft.fillCircle(x + 15, y + 16, 8, TFT_LIGHTGREY);
    tft.fillCircle(x + 35, y + 16, 8, TFT_LIGHTGREY);
    int flakes = (code >= 75 || code == 86) ? 4 : 3;
    for (int i = 0; i < flakes; i++) {
      tft.fillCircle(x + 12 + i * 10, y + 32, 2, TEXT_COLOR);
    }
  }
  else if (code >= 95 && code <= 99) {
    tft.fillCircle(x + 25, y + 10, 10, TFT_DARKGREY);
    tft.fillCircle(x + 15, y + 14, 8, TFT_DARKGREY);
    tft.fillCircle(x + 35, y + 14, 8, TFT_DARKGREY);
    tft.drawLine(x + 28, y + 22, x + 22, y + 30, TFT_YELLOW);
    tft.drawLine(x + 22, y + 30, x + 30, y + 30, TFT_YELLOW);
    tft.drawLine(x + 30, y + 30, x + 24, y + 40, TFT_YELLOW);
  }
  else {
    tft.setTextColor(TEXT_COLOR);
    tft.setTextSize(4);
    tft.setCursor(x + 10, y + 5);
    tft.print("?");
  }
}

// ============ IKONA POGODY (mala) ============
void drawWeatherIconSmall(int x, int y, int code) {
  int cx = x + 15;
  int cy = y + 15;

  if (code == 0 || code == 1) {
    tft.fillCircle(cx, cy, 8, TFT_YELLOW);
    for (int i = 0; i < 8; i++) {
      float a = i * PI / 4;
      tft.drawLine(cx + cos(a) * 10, cy + sin(a) * 10,
                   cx + cos(a) * 14, cy + sin(a) * 14, TFT_YELLOW);
    }
  }
  else if (code == 2) {
    tft.fillCircle(cx - 4, cy - 4, 6, TFT_YELLOW);
    tft.fillCircle(cx + 4, cy + 2, 6, TFT_LIGHTGREY);
    tft.fillCircle(cx - 2, cy + 4, 5, TFT_LIGHTGREY);
  }
  else if (code == 3) {
    tft.fillCircle(cx, cy - 2, 7, TFT_DARKGREY);
    tft.fillCircle(cx - 6, cy + 2, 5, TFT_LIGHTGREY);
    tft.fillCircle(cx + 6, cy + 2, 5, TFT_LIGHTGREY);
  }
  else if (code == 45 || code == 48) {
    for (int i = 0; i < 3; i++) {
      tft.drawLine(x + 3, y + 8 + i * 7, x + 27, y + 8 + i * 7, TFT_LIGHTGREY);
    }
  }
  else if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
    tft.fillCircle(cx, cy - 4, 6, TFT_DARKGREY);
    tft.fillCircle(cx - 5, cy, 4, TFT_DARKGREY);
    tft.fillCircle(cx + 5, cy, 4, TFT_DARKGREY);
    tft.drawLine(cx - 4, cy + 6, cx - 5, cy + 12, HUMID_COLOR);
    tft.drawLine(cx + 4, cy + 6, cx + 3, cy + 12, HUMID_COLOR);
  }
  else if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) {
    tft.fillCircle(cx, cy - 4, 6, TFT_LIGHTGREY);
    tft.fillCircle(cx - 5, cy, 4, TFT_LIGHTGREY);
    tft.fillCircle(cx + 5, cy, 4, TFT_LIGHTGREY);
    tft.fillCircle(cx - 4, cy + 8, 2, TEXT_COLOR);
    tft.fillCircle(cx + 4, cy + 8, 2, TEXT_COLOR);
  }
  else if (code >= 95) {
    tft.fillCircle(cx, cy - 4, 6, TFT_DARKGREY);
    tft.drawLine(cx + 2, cy + 4, cx - 2, cy + 10, TFT_YELLOW);
    tft.drawLine(cx - 2, cy + 10, cx + 2, cy + 10, TFT_YELLOW);
    tft.drawLine(cx + 2, cy + 10, cx - 2, cy + 16, TFT_YELLOW);
  }
  else {
    tft.setTextColor(TEXT_COLOR);
    tft.setTextSize(2);
    tft.setCursor(x + 6, y + 4);
    tft.print("?");
  }
}

// ============ EKRAN STARTOWY ============
void showStartScreen() {
  tft.setTextColor(TEMP_COLOR);
  tft.setTextSize(3);
  tft.setCursor(30, 50);
  tft.print("POGODA ESP32");

  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(1);
  tft.setCursor(60, 100);
  tft.print("Stacja pogodowa v2.0");

  tft.setTextColor(LABEL_COLOR);
  tft.setCursor(40, 120);
  tft.print("Open-Meteo - bez klucza API!");
  tft.setCursor(50, 140);
  tft.print("Ekrany przelaczaja sie co 10s");

  delay(2500);
}

// ============ BLAD ============
void showError(const char* msg) {
  tft.fillRect(0, 220, 320, 20, TFT_RED);
  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(1);
  tft.setCursor(10, 224);
  tft.print(msg);
}
