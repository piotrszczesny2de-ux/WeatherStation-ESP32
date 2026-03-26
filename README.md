# WeatherStation ESP32 + ILI9341

Stacja pogodowa na ESP32 z ekranem TFT 2.4" ILI9341 (parallel 8-bit).

Dane pogodowe z API [Open-Meteo](https://open-meteo.com/) - darmowe, bez klucza API.

## Funkcje

- Aktualna temperatura, odczuwalna, wilgotnosc, wiatr, cisnienie
- Prognoza na 3 dni
- Zegar NTP (czas lokalny)
- Automatyczne przelaczanie 3 ekranow co 10s
- Odswiezanie pogody co 5 minut

## Konfiguracja WiFi i lokalizacji

W pliku `WeatherStation.ino` zmien sekcje konfiguracji:

```cpp
// ============ KONFIGURACJA ============
const char* WIFI_SSID     = "TwojaSiec";      // nazwa sieci WiFi
const char* WIFI_PASSWORD = "TwojeHaslo";      // haslo WiFi

const float LATITUDE  = 54.15;   // szerokosc geograficzna
const float LONGITUDE = 19.40;   // dlugosc geograficzna
const char* CITY_NAME = "Elblag"; // nazwa miasta (wyswietlana na ekranie)
// ======================================
```

Wspolrzedne swojego miasta znajdziesz np. na [Google Maps](https://maps.google.com) - kliknij prawym na mapie.

## Wymagane biblioteki

- **TFT_eSPI** - sterownik ekranu (wymaga konfiguracji `User_Setup.h`)
- **ArduinoJson** (v7)

## Podlaczenie ekranu (parallel 8-bit)

| Ekran | ESP32 |
|-------|-------|
| VCC | 3.3V |
| GND | GND |
| LCD_RST | GPIO 32 |
| LCD_CS | GPIO 33 |
| LCD_RS | GPIO 15 |
| LCD_WR | GPIO 4 |
| LCD_RD | GPIO 2 |
| LCD_D0 | GPIO 22 lub GPIO 12 (*) |
| LCD_D1 | GPIO 13 |
| LCD_D2 | GPIO 26 |
| LCD_D3 | GPIO 25 |
| LCD_D4 | GPIO 17 |
| LCD_D5 | GPIO 16 |
| LCD_D6 | GPIO 27 |
| LCD_D7 | GPIO 14 |

(*) **GPIO 12 vs 22** - zalezy od wersji ESP32:
- ESP32-D0WD-V3 (rev3.1) - uzyj **GPIO 12** (`TFT_D0 12`)
- ESP32-D0WDQ6 (rev1.0) - uzyj **GPIO 22** (`TFT_D0 22`), bo GPIO 12 blokuje boot

## Konfiguracja TFT_eSPI (`User_Setup.h`)

W pliku `User_Setup.h` biblioteki TFT_eSPI upewnij sie ze:

```cpp
#define TFT_PARALLEL_8_BIT   // odkomentowane
#define TFT_D0   22          // lub 12 - zaleznie od wersji ESP32
```

Piny SPI musza byc **zakomentowane**.

## Kompilacja (Arduino CLI)

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 WeatherStation
arduino-cli upload --fqbn esp32:esp32:esp32 --port COM3 WeatherStation
```
