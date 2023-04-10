#pragma once

// firmware version (must match with definitions in the main source file)
#define USR_FW_VERSION 0
#define USR_FW_SUBVERSION 1
#define USR_FW_HOTFIX 0
#define USR_FW_BRANCH "main"

/*
Configuration to be done by user below
*/

// Screen
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLEDdelay 1      // Write OLED every X seconds

// Digital Sensors
#define DHTTYPE                                                                \
  DHT22 // DHT11 = DHT 11, DHT21 = DHT 21 (AM2301), DHT22 = DHT 22 (AM2302)
#define sensorDelay 5 // Read sensors every X seconds

/*
Telegram
Bot Token and Chat ID acquisition to be documented in README
*/
#define BOTtoken "myToken" // your Bot Token (Get from Botfather)
#define CHAT_ID "myID"     // Telegram Group ID
#define botRequestDelay 1  // Checks for new messages every X seconds
enum TelegramNotificationLevel { TNL_Off, TNL_Warnings, TNL_All };
const enum TelegramNotificationLevel TELEGRAM_NOTIFICATION_LEVEL_DEFAULT =
    TNL_All; // Sets level of Telegram Notifications

/*
Soil Moisture
Default Values to be calibrated
Calibration procedure to be documented in README
*/
#define moistureSensorCount 2     // Number of Moisture Sensors used
#define airValueDefault 2330      // moisture calibration point air
#define waterValueDefault 380     // moisture calibration point water
#define moistureDelayDefault 3600 // measure moisture every X seconds
#define moistureThreshDefault 30  // SoilMoisture threshold for watering
#define pumpTimeDefault 5         // run pump for X seconds

// WIFI Settings
#define maxReconnect 5 // maximum amount of reconnects when connecting to WIFI
#define ssid "mySSID"  // SSID
#define password "myPassword" // Password

// MQTT Settings
#define enableMQTT 1                 // Enable MQTT | 0 = disabled; 1 = enabled
#define mqttServer "XXX.XXX.XXX.XXX" // MQTT Server IP
#define mqttPort 1883                // MQTT Server Port
#define mqttUser "mymqttuser"        // MQTT Username
#define mqttPassword "mymqttpass"    // MQTT Password
#define mqttUserID "mymqttID"        // MQTT User ID
#define mqttDelay 1                  // Publish to MQTT every X seconds

/*
OpenWeatherMap Settings
API Key acquisition to be documented in README
*/
#define enableWeather                                                          \
  1 // Enable watering depending on weatherforecast | 0 = disabled; 1 = enabled
#define precipationThreshold 3 // Sets threshold in mm to decide for watering
#define openWeatherMapApiKey "myAPIkey"
String CoordLat = "00.000";
String CoordLon = "00.000";

/*
Deepsleep Settings
Currently Deepsleep is only available with WIFI connection
RTC implementation for sleeptime calculation is planned
*/
#define enableDeepSleep 0        // Enable DeepSleep | 0 = disabled; 1 = enabled
#define ntpServer "pool.ntp.org" // time receive URL
#define gmtOffset_sec 3600       // GMT offset in seconds (3600 for Germany)
#define daylightOffset_sec 3600  // Daylight Savings offset in seconds
#define timeReadingDelay 60      // Reads time every X seconds
#define wakeupH 4                // Hour to wake up in the morning
#define wakeupMin 30             // Minute to wake up in the morning
#define wakeupHlate 20           // Hour to wake up in the evening
#define wakeupMinlate 30         // Minute to wake up in the evening
#define timeAwake 1800           // Time awake after Wakeup in seconds

// Pin Layout
#define DHTPIN 14       // Digital pin connected to the DHT sensor
#define MoisturePin1 34 // Analog pin connected to the Analog Sensor 1
#define MoisturePin2 35 // Analog pin connected to the Analog Sensor 2
#define MoisturePin3 0  // Analog pin connected to the Analog Sensor 3
#define MoisturePin4 0  // Analog pin connected to the Analog Sensor 4
#define MoisturePin5 0  // Analog pin connected to the Analog Sensor 5
#define MoisturePin6 0  // Analog pin connected to the Analog Sensor 6
#define REDLED 16       // Digital pin connected to red LED
#define GREENLED 15     // Digital pin connected to green LED
#define ButtonPin 39    // Digital pin connected to pushbutton
#define oneWireBus 4    // GPIO where the DS18B20 is connected to
