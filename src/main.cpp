#include <Adafruit_GFX.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_INA260.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Arduino_JSON.h>
#include <DHT.h>
#include <DallasTemperature.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <UniversalTelegramBot.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <time.h>
#include <userConfig.h>

#define EARLY 0 // Definitions for DeepSleep States
#define LATE 1

enum TelegramNotificationLevel TELEGRAM_NOTIFICATION_LEVEL;
int airValue;
int waterValue;
int moistureDelay;
int pumpTime;
unsigned long lastPumped;
unsigned long lastMoistureRead;
int soilMoisturePercent[moistureSensorCount]; // SoilMoisture percentage value
int soilMoistureAnalog[moistureSensorCount];  // SoilMoisture analog value
int moistureThresh;
unsigned long lastMQTT;
String jsonBuffer;
bool weatherRequest = false;
unsigned long lastTimeBotRan;
unsigned long lastTimeRead;
int buttonState;
int lastButtonState = LOW;
unsigned long debounceDelay = 50;
unsigned long lastDebounce;
float temperatureC;          // Temperature in Celsius
const int MS_TO_S = 1000;    // ms in s conversion factor
const int US_TO_S = 1000000; // us in s conversion factor
bool pumpRunning = false;    // Pump state
bool moistureUpdate = false; // Trigger for Moisture Sensor read state
int pumpCount;
float t; // DHT temp value
float h; // DHT humidity value
unsigned long lastOLEDWrite;
unsigned long lastSensorRead;
unsigned long lastWakeup;
int sleepH;
int sleepMin;
RTC_DATA_ATTR int nextWakeupTime = EARLY;
RTC_DATA_ATTR bool firstStart = true;

WiFiClientSecure telegramClient;
UniversalTelegramBot bot(BOTtoken, telegramClient);
WiFiClient mqttClient;
PubSubClient client(mqttClient);
Preferences preferences; // initiate Preferences library
DHT dht(DHTPIN, DHTTYPE);
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

// Reads Soil Moisture Sensors
void ReadSoilMoisture(int SensorCount) {

  moistureUpdate = true; // Sets Sensor values updated, allows water pumping
  int SoilMoisturePin[] = {MoisturePin1, MoisturePin2, MoisturePin3,
                           MoisturePin4, MoisturePin5, MoisturePin6};
  String MoistureMessage = "Reading Moisture Sensor.\n";

  for (int i = 0; i < SensorCount; i++) {

    soilMoistureAnalog[i] = analogRead(SoilMoisturePin[i]);

    if (soilMoistureAnalog[i] >= airValue) {
      soilMoisturePercent[i] = 0;
    } else if (soilMoistureAnalog[i] <= waterValue) {
      soilMoisturePercent[i] = 100;
    } else {
      soilMoisturePercent[i] =
          map(soilMoistureAnalog[i], airValue, waterValue, 0, 100);
    }
    MoistureMessage += "Moisture " + String(i + 1) +
                       " is: " + String(soilMoisturePercent[i]) + " %\n";
    Serial.print(MoistureMessage);

    // Publish values to MQTT if enabled
    if (enableMQTT == true) {
      String mqttTopicStr =
          "balcony/plants/moisture" + String(i + 1); // create topic as String
      const char *mqttTopic = mqttTopicStr.c_str();  // convert String to char*
      char mqttVal[3];
      sprintf(mqttVal, "%d",
              soilMoisturePercent[i]); // write int SoilMoisturePercent to char
                                       // mqttVal
      client.publish(mqttTopic, mqttVal);
      Serial.print("Publishing to MQTT ");
      Serial.print(mqttTopic);
      Serial.print(": ");
      Serial.println(mqttVal);
      mqttTopicStr = "balcony/plants/moistureAnalog" +
                     String(i + 1);     // create topic as String
      mqttTopic = mqttTopicStr.c_str(); // convert String to char*
      sprintf(mqttVal, "%d",
              soilMoistureAnalog[i]); // write int SoilMoistureAnalog to char
                                      // mqttVal
      client.publish(mqttTopic, mqttVal);
      Serial.print("Publishing to MQTT ");
      Serial.print(mqttTopic);
      Serial.print(": ");
      Serial.println(mqttVal);
    }
  }

  if (TELEGRAM_NOTIFICATION_LEVEL == TNL_All) {
    bot.sendMessage(CHAT_ID, MoistureMessage, "");
  }
}

// Start Pump
void StartPump() {
  pumpRunning = true;
  digitalWrite(GREENLED, LOW);
  digitalWrite(REDLED, HIGH);
  moistureUpdate = false;
  lastPumped = millis();
}

// Handle what happens when you receive new messages
void HandleNewMessages(int numNewMessages) {
  Serial.println("HandleNewMessages");
  Serial.println(String(numNewMessages));

  // handles new messages only if activated
  if (TELEGRAM_NOTIFICATION_LEVEL == TNL_Off) {
    return;
  }

  for (int i = 0; i < numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }

    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);
    String from_name = bot.messages[i].from_name;

    if (text == "/read_moisture") {
      ReadSoilMoisture(moistureSensorCount);
      String ReadHum = "Humidity Readings:\n";
      for (int i = 0; i < moistureSensorCount; i++) {
        ReadHum += "Humidity " + String(i) +
                   " is: " + String(soilMoisturePercent[i]) + " % \n";
      }
      ReadHum += "\n";
      if (TELEGRAM_NOTIFICATION_LEVEL == TNL_All) {
        ReadHum += "Analog Values:\n";
        for (int i = 0; i < moistureSensorCount; i++) {
          ReadHum += "Humidity " + String(i) +
                     " is: " + String(soilMoistureAnalog[i]) + "\n";
        }
        ReadHum += "\n";
        ReadHum += "Humidity reading age " +
                   String((millis() - lastMoistureRead) / MS_TO_S) +
                   " seconds. \n";
        ReadHum +=
            "Next humidity reading in " +
            String(moistureDelay - ((millis() - lastMoistureRead) / MS_TO_S)) +
            " seconds.";
      }
      bot.sendMessage(chat_id, ReadHum, "");
    } else if (text == "/read_temp") {
      String ReadTemp = "Temperature is: " + String(temperatureC) + " ºC";
      bot.sendMessage(chat_id, ReadTemp, "");
    } else if (text == "/read_data") {
      String ReadData = "Temperature is: " + String(temperatureC) + " ºC \n";
      for (int i = 0; i < moistureSensorCount; i++) {
        ReadData += "Humidity " + String(i) +
                    " is: " + String(soilMoisturePercent[i]) + " % \n";
      }
      ReadData += "\n";
      ReadData += "Humidity reading age " +
                  String((millis() - lastMoistureRead) / MS_TO_S) +
                  " seconds. \n";
      ReadData +=
          "Next humidity reading in " +
          String(moistureDelay - ((millis() - lastMoistureRead) / MS_TO_S)) +
          " seconds.";
      bot.sendMessage(chat_id, ReadData, "");
    } else if (text == "/read_room_hum") {
      String ReadRoomHum = "Room humidity is: " + String(h) + " %";
      bot.sendMessage(chat_id, ReadRoomHum, "");
    } else if (text == "/read_room_temp") {
      String ReadRoomTemp = "Room temperature is: " + String(t) + " ºC";
      bot.sendMessage(chat_id, ReadRoomTemp, "");
    } else if (text == "/read_room_climate") {
      String ReadRoomClimate = "Room temperature is: " + String(t) + " ºC \n";
      ReadRoomClimate += "Room humidity is: " + String(h) + " %";
      bot.sendMessage(chat_id, ReadRoomClimate, "");
    } else if (text == "/startpump") {
      StartPump();
      String pumpMsg = "Starting pump for " + String(pumpTime) + " seconds\n";
      bot.sendMessage(chat_id, pumpMsg, "");
      Serial.print("Pump started by Telegram user ");
      Serial.println(from_name);
      Serial.print("pumpCount: ");
      Serial.println(pumpCount);
    } else if (text == "/show_time") {
      struct tm timeinfo;
      getLocalTime(&timeinfo);
      int day = timeinfo.tm_mday;
      int month = timeinfo.tm_mon + 1;
      int year = timeinfo.tm_year + 1900;
      int hour = timeinfo.tm_hour;
      int minute = timeinfo.tm_min;
      String ShowTime = "The current time is: " + String(day) + "." +
                        String(month) + "." + String(year) + " " +
                        String(hour) + ":" + String(minute);
      bot.sendMessage(chat_id, ShowTime, "");
      Serial.print("Time requested by Telegram user ");
      Serial.println(from_name);
      Serial.println(ShowTime);
    } else if (text == "/read_weather") {
      weatherRequest = true;
      String ReadWeather = "Requesting weather data and sending to serial port";
      bot.sendMessage(chat_id, ReadWeather, "");
      Serial.print("Weather requested by Telegram user ");
      Serial.println(from_name);
    } else if (text.indexOf("/setmoisturethreshold") >= 0) {
      String setValMsg = "/setmoisturethreshold";
      if (text.length() <= setValMsg.length()) {
        String errorMsg =
            "Current Moisture Threshold = " + String(moistureThresh) + " %\n";
        errorMsg += "To change threshold, write " + String(setValMsg) + " XX";
        Serial.println(errorMsg);
        bot.sendMessage(chat_id, errorMsg, "");
      } else if (text.charAt(setValMsg.length()) != ' ') {
        String errorMsg = "Error: expected SPACE character after command";
        Serial.println(errorMsg);
        bot.sendMessage(chat_id, errorMsg, "");
      } else {
        String newVal = text.substring(setValMsg.length() + 1);
        bool isValid = true;
        for (int i = 0; i < newVal.length(); i++) {
          if (isDigit(newVal.charAt(i)) != true) {
            isValid = false;
          }
        }
        if (isValid == true) {
          String confirmationMsg =
              "moistureThresh was set from: " + String(moistureThresh) + " %\n";

          // Set moistureThresh and write to preferences
          moistureThresh = newVal.toInt();
          preferences.putInt("moistureThresh", moistureThresh);

          confirmationMsg += "to: " + String(moistureThresh) + " %";
          bot.sendMessage(chat_id, confirmationMsg, "");
          Serial.println(confirmationMsg);
        } else {
          String errorMsg =
              "Error: expected numeric characters only after command";
          Serial.println(errorMsg);
          bot.sendMessage(chat_id, errorMsg, "");
        }
      }
    } else if (text.indexOf("/setwatervalue") >= 0) {
      String setValMsg = "/setwatervalue";
      if (text.length() <= setValMsg.length()) {
        String errorMsg =
            "Current calibration Water Value = " + String(waterValue) + "\n";
        errorMsg += "To change value, write " + String(setValMsg) + " XX";
        Serial.println(errorMsg);
        bot.sendMessage(chat_id, errorMsg, "");
      } else if (text.charAt(setValMsg.length()) != ' ') {
        String errorMsg = "Error: expected SPACE character after command";
        Serial.println(errorMsg);
        bot.sendMessage(chat_id, errorMsg, "");
      } else {
        String newVal = text.substring(setValMsg.length() + 1);
        bool isValid = true;
        for (int i = 0; i < newVal.length(); i++) {
          if (isDigit(newVal.charAt(i)) != true) {
            isValid = false;
          }
        }
        if (isValid == true) {
          String confirmationMsg =
              "calibration WaterValue was set from: " + String(waterValue) +
              "\n";

          // Set waterValue and write to preferences
          waterValue = newVal.toInt();
          preferences.putInt("waterValue", waterValue);

          confirmationMsg += "to: " + String(waterValue);
          bot.sendMessage(chat_id, confirmationMsg, "");
          Serial.println(confirmationMsg);
        } else {
          String errorMsg =
              "Error: expected numeric characters only after command";
          Serial.println(errorMsg);
          bot.sendMessage(chat_id, errorMsg, "");
        }
      }
    } else if (text.indexOf("/setairvalue") >= 0) {
      String setValMsg = "/setairvalue";
      if (text.length() <= setValMsg.length()) {
        String errorMsg =
            "Current calibration Air Value = " + String(airValue) + "\n";
        errorMsg += "To change value, write " + String(setValMsg) + " XX";
        Serial.println(errorMsg);
        bot.sendMessage(chat_id, errorMsg, "");
      } else if (text.charAt(setValMsg.length()) != ' ') {
        String errorMsg = "Error: expected SPACE character after command";
        Serial.println(errorMsg);
        bot.sendMessage(chat_id, errorMsg, "");
      } else {
        String newVal = text.substring(setValMsg.length() + 1);
        bool isValid = true;
        for (int i = 0; i < newVal.length(); i++) {
          if (isDigit(newVal.charAt(i)) != true) {
            isValid = false;
          }
        }
        if (isValid == true) {
          String confirmationMsg =
              "calibration AirValue was set from: " + String(airValue) + "\n";

          // Set airValue and write to preferences
          airValue = newVal.toInt();
          preferences.putInt("airValue", airValue);

          confirmationMsg += "to: " + String(airValue);
          bot.sendMessage(chat_id, confirmationMsg, "");
          Serial.println(confirmationMsg);
        } else {
          String errorMsg =
              "Error: expected numeric characters only after command";
          Serial.println(errorMsg);
          bot.sendMessage(chat_id, errorMsg, "");
        }
      }
    } else if (text.indexOf("/setpumptime") >= 0) {
      String setValMsg = "/setpumptime";
      if (text.length() <= setValMsg.length()) {
        String errorMsg = "Current Pump Time = " + String(pumpTime) + " s\n";
        errorMsg += "To change value, write " + String(setValMsg) + " XX";
        Serial.println(errorMsg);
        bot.sendMessage(chat_id, errorMsg, "");
      } else if (text.charAt(setValMsg.length()) != ' ') {
        String errorMsg = "Error: expected SPACE character after command";
        Serial.println(errorMsg);
        bot.sendMessage(chat_id, errorMsg, "");
      } else {
        String newVal = text.substring(setValMsg.length() + 1);
        bool isValid = true;
        for (int i = 0; i < newVal.length(); i++) {
          if (isDigit(newVal.charAt(i)) != true) {
            isValid = false;
          }
        }
        if (isValid == true) {
          String confirmationMsg =
              "PumpTime was set from: " + String(pumpTime) + " s\n";
          pumpTime = newVal.toInt();
          confirmationMsg += "to: " + String(pumpTime) + " s";
          bot.sendMessage(chat_id, confirmationMsg, "");
          Serial.println(confirmationMsg);
        } else {
          String errorMsg =
              "Error: expected numeric characters only after command";
          Serial.println(errorMsg);
          bot.sendMessage(chat_id, errorMsg, "");
        }
      }
    } else if (text.indexOf("/huso") >= 0) {
      int atPos = text.indexOf('@');
      String huso = text.substring(atPos);
      if (huso == "@Chillkroetchen_Plant_Bot") {
        bot.sendMessage(chat_id, "Nice try faggit LULE", "");
      }
      huso += " ist ein Hurensohn!";
      bot.sendMessage(chat_id, huso, "");
    } else if (text == "/help") {
      String welcome = "Hello, " + from_name + ".\n";
      welcome += "Use the following commands to get current readings.\n\n";
      welcome += "/read_temp: Reads temperature sensor \n";
      welcome += "/read_moisture: Reads soil moisture sensors \n";
      welcome +=
          "/read_data: Reads both, temperature and moisture sensors \n\n";
      welcome += "/read_room_temp: Reads room temperature \n";
      welcome += "/read_room_hum: Reads room humidity \n";
      welcome += "/read_room_climate: Reads both, room temperature and room "
                 "humidity \n\n";
      welcome +=
          "/startpump: Starts pump for " + String(pumpTime) + " seconds \n\n";
      welcome += "/show_time: Shows current time \n";
      welcome += "/read_weather: Reads weather forecast and sends result to "
                 "serial port \n\n";
      welcome += "/setmoisturethreshold XX: Sets Threshold for Soil Moisture "
                 "to XX %\n";
      welcome += "/setairvalue XX: Sets Air Value for calibration to XX\n";
      welcome += "/setwatervalue XX: Sets Water Value for calibration to XX\n";
      welcome += "/setpumptime XX: Sets Pump Time to XX seconds\n\n";
      welcome += "Was I a good bot? :)";
      bot.sendMessage(chat_id, welcome, "");
      String keyboardJson =
          "[[\"/read_temp\", \"/read_moisture\", "
          "\"/read_data\"],[\"/read_room_temp\", \"/read_room_hum\", "
          "\"/read_room_climate\"],[\"/startpump\"],[\"/show_time\", "
          "\"/read_weather\"],[\"/setmoisturethreshold\", \"/setairvalue\", "
          "\"/setwatervalue\", \"/setpumptime\"]]";
      bot.sendMessageWithReplyKeyboard(chat_id, "How can I help you?", "",
                                       keyboardJson, true);
    }
  }
}

// Writes Sensor Data to OLED
void WriteOLED() {
  Serial.println("Writing data to OLED");
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("Klimadaten");
  display.setCursor(0, 20);
  display.setTextSize(1);
  display.println("Daten Sonden:");
  display.setCursor(0, 30);
  display.print(temperatureC);
  display.cp437(true);
  display.write(167);
  display.print("C ");
  display.print(soilMoisturePercent[0]);
  display.print("% ");
  display.print(soilMoisturePercent[1]);
  display.println("%");
  display.setCursor(0, 44), display.println("Daten DHT:");
  display.setCursor(0, 54);
  display.print(t);
  display.cp437(true);
  display.write(167);
  display.print("C ");
  display.print(h);
  display.println("%");
  display.display();
}

// Reads Digital Sensors
void ReadSerialSensors() {

  Serial.println("Reading Sensors");
  // read temperature and humidity from DHT
  t = dht.readTemperature();
  h = dht.readHumidity();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
  }

  // read temperature from DS18B20
  sensors.requestTemperatures();
  temperatureC = sensors.getTempCByIndex(0);
}

// MQTT Callback function, mostly used to subscribe defined topics
void Callback(char *topic, byte *message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // handle messages coming in from MQTT
  if (String(topic) == "balcony/plants/pumpTime") {
    int messageVal = messageTemp.toInt();
    if (messageVal != pumpTime) {

      // Set pumpTime and write to preferences
      pumpTime = messageVal;
      preferences.putInt("pumpTime", pumpTime);

      Serial.print("Changed pumpTime to: ");
      Serial.println(pumpTime);
    } else {
      Serial.print("pumpTime wasn't changed. Current value: ");
      Serial.println(pumpTime);
    }
  } else if (String(topic) == "balcony/plants/startPump" &&
             pumpRunning == false) {
    int messageVal = messageTemp.toInt();
    if (messageVal == 1 && pumpRunning == false) {
      StartPump();
      Serial.println("Pump started by MQTT request");
      Serial.print("pumpCount: ");
      Serial.println(pumpCount);
    }
  } else if (String(topic) == "balcony/plants/moistureThreshold") {
    int messageVal = messageTemp.toInt();
    if (messageVal != moistureThresh) {

      // Set moistureThresh and write to preferences
      moistureThresh = messageVal;
      preferences.putInt("moistureThresh", moistureThresh);

      Serial.print("Changed moistureThresh to: ");
      Serial.println(moistureThresh);
    } else {
      Serial.print("moistureThresh wasn't changed. Current value: ");
      Serial.println(moistureThresh);
    }
  } else if (String(topic) == "balcony/plants/waterValue") {
    int messageVal = messageTemp.toInt();
    if (messageVal != waterValue) {

      // Set waterValue and write to preferences
      waterValue = messageVal;
      preferences.putInt("waterValue", waterValue);

      Serial.print("Changed waterValue to: ");
      Serial.println(waterValue);
    } else {
      Serial.print("waterValue wasn't changed. Current value: ");
      Serial.println(waterValue);
    }
  } else if (String(topic) == "balcony/plants/airValue") {
    int messageVal = messageTemp.toInt();
    if (messageVal != airValue) {

      // Set airValue and write to preferences
      airValue = messageVal;
      preferences.putInt("airValue", airValue);

      Serial.print("Changed airValue to: ");
      Serial.println(airValue);
    } else {
      Serial.print("airValue wasn't changed. Current value: ");
      Serial.println(airValue);
    }
  } else if (String(topic) == "balcony/plants/enableTelegram") {
    int messageVal = messageTemp.toInt();
    if (messageVal == 0) {
      TELEGRAM_NOTIFICATION_LEVEL = TNL_Off;
      Serial.println("Telegram disabled");
      bot.sendMessage(CHAT_ID, "Telegram disabled by MQTT request", "");
    } else if (messageVal == 1) {
      TELEGRAM_NOTIFICATION_LEVEL = TNL_Warnings;
      Serial.println("Telegram notification level: warnings only");
      bot.sendMessage(CHAT_ID,
                      "Telegram notification level changed to WARNINGS ONLY by "
                      "MQTT request",
                      "");
    } else if (messageVal == 2) {
      TELEGRAM_NOTIFICATION_LEVEL = TNL_All;
      Serial.println("Telegram notification level: all");
      bot.sendMessage(
          CHAT_ID, "Telegram notification level changed to ALL by MQTT request",
          "");
    } else {
      Serial.println("Telegram notification level not recognized");
    }
  }
}

// MQTT Reconnect and Subscribe routine
void MQTTreconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqttUserID, mqttUser, mqttPassword)) {
      Serial.println("MQTT connected");
      // Subscribe
      client.subscribe("balcony/plants/pumpTime");
      client.subscribe("balcony/plants/startPump");
      client.subscribe("balcony/plants/moistureThreshold");
      client.subscribe("balcony/plants/waterValue");
      client.subscribe("balcony/plants/airValue");
      client.subscribe("balcony/plants/enableTelegram");
    } else {
      Serial.print("failed with state ");
      Serial.println(client.state());
      delay(1000);
    }
  }
}

// Sends ESP to DeepSleep
void SendToDeepSleep() {
  struct tm currentTime;
  struct tm sleepTime;
  getLocalTime(&currentTime);

  if (!getLocalTime(&sleepTime)) {
    Serial.println("Failed to obtain time");
    bot.sendMessage(CHAT_ID, "Failed to obtain time");

    return;
  }

  String SleepMessage = "Going to sleep until ";
  if (nextWakeupTime == EARLY) {
    SleepMessage += String(wakeupH) + ":" + String(wakeupMin) + " h";
    sleepTime.tm_mday += 1;
    sleepTime.tm_hour = wakeupH;
    sleepTime.tm_min = wakeupMin;
    sleepTime.tm_sec = 0;
    nextWakeupTime = LATE;
  } else if (nextWakeupTime == LATE) {
    SleepMessage += String(wakeupHlate) + ":" + String(wakeupMinlate) + " h";
    sleepTime.tm_hour = wakeupHlate;
    sleepTime.tm_min = wakeupMinlate;
    sleepTime.tm_sec = 0;
    nextWakeupTime = EARLY;
  }

  time_t future = mktime(&sleepTime);
  time_t now = mktime(&currentTime);
  unsigned long long DeepSleepTime =
      (unsigned long long)(future - now) * 1000000ULL;

  bot.sendMessage(CHAT_ID, SleepMessage, "");
  Serial.println(SleepMessage);

  display.clearDisplay();
  display.display();

  esp_sleep_enable_timer_wakeup(DeepSleepTime);
  delay(1000);
  esp_deep_sleep_start();

  // the above function effectively returns from this function except if an
  // error occured

  bot.sendMessage(CHAT_ID, "DeepSleep Failed", "");
  Serial.println("DeepSleep Failed");
}

// WiFi Reconnect
void WiFiReconnect() {
  int i = 1;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    Serial.print("Number: ");
    Serial.println(i);
    i++;
    if (i == maxReconnect) {
      i = 0;
      Serial.println("WiFi connection failed. Continuing offline.");
      break;
    }
  }
}

String httpGETRequest(const char *serverName) {
  WiFiClient client;
  HTTPClient http;

  // Your Domain name with URL path or IP address with path
  http.begin(client, serverName);

  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

void setup() {

  pinMode(GREENLED, OUTPUT);
  pinMode(REDLED, OUTPUT);
  pinMode(ButtonPin, INPUT);
  digitalWrite(GREENLED, LOW);
  digitalWrite(REDLED, LOW);

  // Start the Serial Monitor
  Serial.begin(115200);
  // Start the DS18B20 sensor
  sensors.begin();
  // Start the DHT sensor
  dht.begin();

  // Starts preferences namespace, reads settings and returns default value if
  // not already set
  preferences.begin("preferences", false);
  airValue = preferences.getInt("airValue", airValueDefault);
  waterValue = preferences.getInt("waterValue", waterValueDefault);
  moistureDelay = preferences.getInt("moistureDelay", moistureDelayDefault);
  pumpTime = preferences.getInt("pumpTime", pumpTimeDefault);
  moistureThresh = preferences.getInt("moistureThresh", moistureThreshDefault);
  // TELEGRAM_NOTIFICATION_LEVEL =
  //     preferences.getInt("tlgrmNotLvl", TELEGRAM_NOTIFICATION_LEVEL_DEFAULT);
  pumpCount = preferences.getInt("pumpCount", 0);

  // Start the OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.display();

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
#ifdef ESP32
  telegramClient.setCACert(
      TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
#endif

  WiFiReconnect();

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  lastWakeup = millis();
  weatherRequest = true;

  // Sends device directly to sleep at first boot
  if (firstStart == true && enableDeepSleep == 1) {
    for (int i = 11; i > 0; i--) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.println("First Boot");
      display.display();
      display.setCursor(0, 20);
      display.setTextSize(1);
      String SleepMessage = "Going to sleep until " + String(wakeupH) + ":" +
                            String(wakeupMin) + " h\n";
      SleepMessage += "Bedge in: " + String(i - 1) + " s";
      display.println(SleepMessage);
      display.display();
      delay(1000);
    }
    firstStart = false;
    SendToDeepSleep();
  }

  // Setup MQTT if enabled
  if (enableMQTT == true) {
    client.setServer(mqttServer, mqttPort);
    client.setCallback(Callback);
    MQTTreconnect();
  }

  ReadSoilMoisture(moistureSensorCount);
  ReadSerialSensors();
  WriteOLED();
}

void loop() {

  // Button Reading Routine
  int reading = digitalRead(ButtonPin);
  if (reading != lastButtonState) {
    lastDebounce = millis();
  }
  if ((millis() - lastDebounce) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == HIGH) { // Write Button Event here
        ReadSoilMoisture(moistureSensorCount);
      }
    }
  }
  lastButtonState = reading;
  // End Button Reading Routine

  WiFiReconnect();

  // MQTT reconnect, subscribe and publish current values
  client.loop(); // call this function frequently to keep client connected
  if (millis() >= lastMQTT + mqttDelay * MS_TO_S && enableMQTT == true) {
    if (!client.connected()) {
      MQTTreconnect();
    }
    char mqttVal[3];
    sprintf(mqttVal, "%d", pumpTime); // convert int to char
    client.publish("balcony/plants/pumpTime", mqttVal);
    sprintf(mqttVal, "%d", moistureThresh); // convert int to char
    client.publish("balcony/plants/moistureThreshold", mqttVal);
    sprintf(mqttVal, "%d", waterValue); // convert int to char
    client.publish("balcony/plants/waterValue", mqttVal);
    sprintf(mqttVal, "%d", airValue); // convert int to char
    client.publish("balcony/plants/airValue", mqttVal);
    lastMQTT = millis();
  }

  // reads Weather Forecast only in the morning and writes PrecipationToday
  if (weatherRequest == true && nextWakeupTime == EARLY && enableWeather == 1) {
    // Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      String serverPath =
          "http://api.openweathermap.org/data/2.5/onecall?lat=" + CoordLat +
          "&lon=" + CoordLon +
          "&units=metric&exclude=minutely,hourly,alerts&appid=" +
          openWeatherMapApiKey;

      jsonBuffer = httpGETRequest(serverPath.c_str());
      Serial.println(jsonBuffer);
      JSONVar myObject = JSON.parse(jsonBuffer);

      // JSON.typeof(jsonVar) can be used to get the type of the var
      if (JSON.typeof(myObject) == "undefined") {
        Serial.println("Parsing input failed!");
        return;
      }

      int RainToday = myObject["daily"][0]["rain"];
      int SnowToday = myObject["daily"][0]["snow"];
      float PrecipationToday = RainToday + SnowToday;

      if (PrecipationToday >= precipationThreshold) {
        String HighPrecipationMessage =
            "High precipation expected for today, skipping plant watering.\n";
        HighPrecipationMessage +=
            "Precipation expected for today: " + String(PrecipationToday) +
            " mm";
        if (TELEGRAM_NOTIFICATION_LEVEL > TNL_Off) {
          bot.sendMessage(CHAT_ID, HighPrecipationMessage, "");
        }
        Serial.println(HighPrecipationMessage);
        if (enableDeepSleep == 1) {
          SendToDeepSleep();
        }
      } else if (PrecipationToday == 0) {
        String weatherMsg = "No precipation expected today";
        bot.sendMessage(CHAT_ID, weatherMsg, "");
        Serial.println(weatherMsg);
      } else {
        String weatherMsg =
            "Precipation expected today: " + String(PrecipationToday) + " mm";
        bot.sendMessage(CHAT_ID, weatherMsg, "");
        Serial.println(weatherMsg);
      }

    } else {
      Serial.println("WiFi Disconnected, failed to obtain weather data");
    }
    weatherRequest =
        false; // WeatherRequest toggle set by Telegram Message & WakeUp
  }

  // read Soil Moisture as defined in MoistureDelay
  if (millis() >= lastMoistureRead + moistureDelay * MS_TO_S) {

    ReadSoilMoisture(moistureSensorCount);
    lastMoistureRead = millis();
  }

  // read Sensors as defined in SensorDelay
  if (millis() >= lastSensorRead + sensorDelay * MS_TO_S) {

    ReadSerialSensors();
    lastSensorRead = millis();
  }

  // Handle Humidity
  if (moistureUpdate == true) {

    moistureUpdate = false;
    // Sets enoughWater false, if one sensor is below threshold
    bool enoughWater = true;
    for (int i = 0; i < moistureSensorCount; i++) {
      if (soilMoisturePercent[i] < moistureThresh) {
        enoughWater = false;
        break;
      }
    }

    // Confirms good humidity if moisture okay and pump not running
    if (enoughWater == true && pumpRunning == false) {
      digitalWrite(GREENLED, HIGH);
      digitalWrite(REDLED, LOW);
      Serial.println("Humidity okay");
      if (TELEGRAM_NOTIFICATION_LEVEL == TNL_All) {
        bot.sendMessage(CHAT_ID, "Humidity okay", "");
      }
    }

    // Warns about low humidity, turns on pump if moisture too low and pumping
    // allowed
    else if (enoughWater == false && pumpRunning == false) {
      digitalWrite(GREENLED, LOW);
      digitalWrite(REDLED, HIGH);
      pumpRunning = true; // Sets pump state to running
      Serial.println("Humidity too low");
      Serial.println("Watering...");
      if (TELEGRAM_NOTIFICATION_LEVEL > TNL_Off) {
        bot.sendMessage(CHAT_ID, "Humidity too low, starting pump.", "");
      }
      lastPumped = millis();
    }

    moistureUpdate = false;
  }

  // Turns off pump if pump is running for defined amount of time
  if (millis() >= lastPumped + pumpTime * MS_TO_S && pumpRunning == true) {
    digitalWrite(GREENLED, HIGH);
    digitalWrite(REDLED, LOW);
    pumpRunning = false; // Sets pump state to off

    // increment pumpCount by one and write to preferences
    pumpCount++;
    preferences.putInt("pumpCount", pumpCount);

    String pumpMessage = "Pump stopped.\n";
    pumpMessage += "Pumpcount = " + String(pumpCount);
    if (TELEGRAM_NOTIFICATION_LEVEL == TNL_All) {
      bot.sendMessage(CHAT_ID, pumpMessage, "");
    }
    Serial.println(pumpMessage);
    if (enableMQTT == true) {
      client.publish("balcony/plants/startPump", "0");
    }
    lastPumped = millis();
  }

  // Telegram Bot behaviour
  if (millis() >= lastTimeBotRan + botRequestDelay * MS_TO_S &&
      TELEGRAM_NOTIFICATION_LEVEL > TNL_Off) {
    Serial.println("Doing Telegram things");
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      Serial.println("got response");
      HandleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }

  // Write OLED as defined in OLEDDelay
  if (millis() >= lastOLEDWrite + OLEDdelay * MS_TO_S) {
    WriteOLED();
    lastOLEDWrite = millis();
  }

  // Sends to DeepSleep after TimeAwake has passed
  if (millis() >= lastWakeup + timeAwake * MS_TO_S && enableDeepSleep == 1) {
    SendToDeepSleep();
  }
}
