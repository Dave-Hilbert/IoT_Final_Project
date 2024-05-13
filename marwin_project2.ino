#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_SHT4x.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <queue>

const char* ssid = "Infinix NOTE 12";
const char* password = "555eieihehe";

const int SCLpin = 40;
const int SDApin = 41;

Adafruit_SHT4x sth;
Adafruit_BMP280 bmp;

const char* mqtt_server = "192.168.176.194";
const int mqtt_port = 1883;
const char* mqtt_clientId = "MzE2NDU4NjU1MTIxMjE2MzcxNDc2NzA5ODgxODg2MTQ2NTG"; // Client ID
const char* mqtt_username = "MWN";   // Token
const char* mqtt_password = "6430321021";   // Secret
const char* mqtt_topic = "@msg/data";         // Topic

TaskHandle_t GetDataHandle;
TaskHandle_t PublishDataHandle;

sensors_event_t humidity, temp;

struct SensorData {
  float temperature;
  float humidity;
  float pressure;
};

// MQTT client
WiFiClient espClient;
PubSubClient mqttClient(espClient);

std::queue<SensorData> sensorDataQueue;
SemaphoreHandle_t dataQueueSemaphore;

void GetData(void * parameter) {
  for (;;) {
    sth.getEvent(&humidity, &temp);

    // Create a SensorData object and populate it with sensor data
    SensorData newData;
    newData.temperature = temp.temperature;
    newData.humidity = humidity.relative_humidity;
    newData.pressure = bmp.readPressure();

    Serial.print(F("Temperature = "));
    Serial.print(newData.temperature);
    Serial.println(" degC");
    Serial.print(F("Humidity = "));
    Serial.print(newData.humidity);
    Serial.println(" rH");
    Serial.print(F("Pressure = "));
    Serial.print(newData.pressure);
    Serial.println(" Pa");


    // Put the sensor data into the queue
    xSemaphoreTake(dataQueueSemaphore, portMAX_DELAY);
    sensorDataQueue.push(newData);
    xSemaphoreGive(dataQueueSemaphore);
    Serial.println("All data get push in queue!");

    // Delay for 1 minute before reading sensor data again
    vTaskDelay(pdMS_TO_TICKS(6000)); // 1 minute delay
  }
}

void PublishData(void * parameter) {
  for (;;) {
    if (!sensorDataQueue.empty()) {
      // Retrieve sensor data from the front of the queue
      xSemaphoreTake(dataQueueSemaphore, portMAX_DELAY);
      SensorData data = sensorDataQueue.front();
      sensorDataQueue.pop();
      xSemaphoreGive(dataQueueSemaphore);

      // Format sensor data into JSON form
      String payload = "{\"S1_Temp\":" + String(data.temperature) + ",\"S5_CO2\":" + String(data.humidity) + ",\"S7_PIR\":" + String(data.pressure) + "}";
      Serial.println(payload);
      // Connect to MQTT
      if (!mqttClient.connected()) {
        mqttClient.setServer(mqtt_server, mqtt_port);
        if (mqttClient.connect(mqtt_clientId, mqtt_username, mqtt_password)) {
          Serial.println("Connected to MQTT broker");
        } else {
          Serial.println("Failed to connect to MQTT broker");
          vTaskDelay(pdMS_TO_TICKS(5000)); // Wait for 5 seconds before retry
          continue;
        }
      }

      // Publish sensor data to MQTT topic
      mqttClient.publish(mqtt_topic, payload.c_str());
      Serial.println("Published sensor data to MQTT topic");
      Serial.println("Waiting for 1 min before get new data...\n");

      // Delay for 1 second before publishing the next data
      vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second delay
    } else {
      // If the queue is empty, wait for 1 second before checking again
      vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second delay
    }
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  
  Serial.println("Connected to WiFi");

  Wire.begin(SDApin, SCLpin);

  if (sth.begin()) {
    Serial.println("STH4x sensor ready!");
  }
  else{
    Serial.println("STH4x NOT FOUND!");
    while(1);}

  if (bmp.begin(0x76)) {
    Serial.println("BMP280 sensor ready!");
  }
  else{Serial.println("BMP280 NOT FOUND!");}

  // Initialize semaphore
  dataQueueSemaphore = xSemaphoreCreateMutex();

  xTaskCreate(GetData, "Get Data Task", 4096, NULL, 1, &GetDataHandle);

  xTaskCreate(PublishData, "Publish Data Task", 4096, NULL, 2, &PublishDataHandle);

}

void loop(){

}