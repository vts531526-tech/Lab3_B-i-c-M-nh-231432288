#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ======================================
// LAB 3 - FIRE GUARD
// ESP32 + FREERTOS + MQTT
// ======================================

// WIFI
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// MQTT
const char* MQTT_HOST = "broker.emqx.io";
const int MQTT_PORT = 1883;
const char* PREFIX = "home/fire";

// GPIO - EXACT DIAGRAM
#define PIN_GAS   34
#define PIN_TEMP  4
#define PIN_FAN   26
#define PIN_PUMP  27

// Relay Wokwi: HIGH activates relay
// Change to LOW if using active-low relays
#define RELAY_ON  HIGH
#define RELAY_OFF LOW

// THRESHOLDS FOR SIMULATION
#define GAS_ON    2000
#define GAS_OFF   1700
#define TEMP_ON   50.0
#define TEMP_OFF  45.0

// TIMING
#define SENSOR_MS 2500
#define STATE_MS  3000
#define WIFI_RETRY_MS 10000
#define MQTT_RETRY_MS 5000

// ======================================
// SENSOR
// ======================================

OneWire oneWire(PIN_TEMP);
DallasTemperature tempSensor(&oneWire);

// ======================================
// FREERTOS DATA STRUCTURES
// ======================================

struct SensorData {
  int gas;
  float temperature;
  uint32_t uptimeMs;
};

struct AlertData {
  int gas;
  float temperature;
};

QueueHandle_t dataQueue;
QueueHandle_t dataAlert;

// ======================================
// MQTT
// ======================================

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// ======================================
// SHARED STATES
// ======================================

portMUX_TYPE stateMux =
  portMUX_INITIALIZER_UNLOCKED;

bool autoMode = true;

bool fanState = false;
bool pumpState = false;

bool gasWarning = false;
bool tempWarning = false;

bool sensorValid = false;

unsigned long lastWiFiAttempt = 0;
unsigned long lastMQTTAttempt = 0;
unsigned long lastStatePublish = 0;

// ======================================
// RELAY CONTROL
// ======================================

void applyOutputs() {
  bool fan;
  bool pump;

  portENTER_CRITICAL(&stateMux);
  fan = fanState;
  pump = pumpState;
  portEXIT_CRITICAL(&stateMux);

  digitalWrite(
    PIN_FAN,
    fan ? RELAY_ON : RELAY_OFF
  );

  digitalWrite(
    PIN_PUMP,
    pump ? RELAY_ON : RELAY_OFF
  );
}

// ======================================
// MQTT PUBLISH
// Only TaskSendData uses MQTT client
// ======================================

void publishText(
  const char* subTopic,
  const String& value,
  bool retain = false
) {
  if (!mqtt.connected()) return;

  String topic = String(PREFIX);
  topic += "/";
  topic += subTopic;

  mqtt.publish(
    topic.c_str(),
    value.c_str(),
    retain
  );
}

// ======================================
// PUBLISH STATES
// ======================================

void publishStates() {
  bool mode;
  bool fan;
  bool pump;
  bool gasAlert;
  bool tempAlert;

  portENTER_CRITICAL(&stateMux);

  mode = autoMode;
  fan = fanState;
  pump = pumpState;
  gasAlert = gasWarning;
  tempAlert = tempWarning;

  portEXIT_CRITICAL(&stateMux);

  publishText(
    "mode/state",
    mode ? "AUTO" : "MANUAL",
    true
  );

  publishText(
    "fan/state",
    fan ? "ON" : "OFF",
    true
  );

  publishText(
    "pump/state",
    pump ? "ON" : "OFF",
    true
  );

  String states = "{";
  states += "\"fan\":\"";
  states += fan ? "ON" : "OFF";
  states += "\",\"pump\":\"";
  states += pump ? "ON" : "OFF";
  states += "\",\"mode\":\"";
  states += mode ? "AUTO" : "MANUAL";
  states += "\"}";

  publishText("states", states, true);

  String alarm = "NORMAL";

  if (gasAlert && tempAlert) {
    alarm = "FIRE_WARNING";
  } else if (gasAlert) {
    alarm = "GAS_WARNING";
  } else if (tempAlert) {
    alarm = "TEMP_WARNING";
  }

  publishText("alarm", alarm, true);
}

// ======================================
// MQTT CALLBACK
// ======================================

void mqttCallback(
  char* topic,
  byte* payload,
  unsigned int length
) {
  String message = "";

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  message.trim();
  message.toUpperCase();

  String receivedTopic = String(topic);

  Serial.print("[MQTT RX] ");
  Serial.print(receivedTopic);
  Serial.print(" = ");
  Serial.println(message);

  bool changed = false;

  // AUTO / MANUAL
  if (
    receivedTopic ==
    String(PREFIX) + "/mode/set"
  ) {
    if (
      message == "AUTO" ||
      message == "MANUAL"
    ) {
      portENTER_CRITICAL(&stateMux);

      autoMode = (message == "AUTO");

      portEXIT_CRITICAL(&stateMux);

      changed = true;
    }
  }

  // FAN MANUAL
  else if (
    receivedTopic ==
    String(PREFIX) + "/fan/set"
  ) {
    if (
      message == "ON" ||
      message == "OFF"
    ) {
      portENTER_CRITICAL(&stateMux);

      if (!autoMode) {
        fanState = (message == "ON");
        changed = true;
      }

      portEXIT_CRITICAL(&stateMux);
    }
  }

  // PUMP MANUAL
  else if (
    receivedTopic ==
    String(PREFIX) + "/pump/set"
  ) {
    if (
      message == "ON" ||
      message == "OFF"
    ) {
      portENTER_CRITICAL(&stateMux);

      if (!autoMode) {
        pumpState = (message == "ON");
        changed = true;
      }

      portEXIT_CRITICAL(&stateMux);
    }
  }

  if (changed) {
    applyOutputs();
    publishStates();
  }
}

// ======================================
// WIFI CONNECTION
// ======================================

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();

  if (
    lastWiFiAttempt != 0 &&
    now - lastWiFiAttempt < WIFI_RETRY_MS
  ) {
    return;
  }

  lastWiFiAttempt = now;

  Serial.println("[WIFI] Connecting...");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

// ======================================
// MQTT CONNECTION
// ======================================

void connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (mqtt.connected()) {
    return;
  }

  unsigned long now = millis();

  if (
    lastMQTTAttempt != 0 &&
    now - lastMQTTAttempt < MQTT_RETRY_MS
  ) {
    return;
  }

  lastMQTTAttempt = now;

  String clientId = "ESP32_FireGuard_";
  clientId += String(
    (uint32_t)ESP.getEfuseMac(),
    HEX
  );

  Serial.println("[MQTT] Connecting...");

  if (
    mqtt.connect(
      clientId.c_str(),
      "home/fire/status",
      0,
      true,
      "offline"
    )
  ) {
    Serial.println("[MQTT] Connected!");

    publishText("status", "online", true);

    mqtt.subscribe("home/fire/mode/set");
    mqtt.subscribe("home/fire/fan/set");
    mqtt.subscribe("home/fire/pump/set");

    publishStates();
  } else {
    Serial.print("[MQTT] Failed: ");
    Serial.println(mqtt.state());
  }
}

// ======================================
// TASK 1 - READ DATA
// CORE 1
// ======================================

void taskReadData(void* parameter) {
  SensorData data;
  AlertData alert;

  for (;;) {
    int gas = analogRead(PIN_GAS);

    tempSensor.requestTemperatures();

    float temperature =
      tempSensor.getTempCByIndex(0);

    if (
      temperature != DEVICE_DISCONNECTED_C &&
      temperature >= -55.0 &&
      temperature <= 125.0
    ) {
      data.gas = gas;
      data.temperature = temperature;
      data.uptimeMs = millis();

      alert.gas = gas;
      alert.temperature = temperature;

      // Send newest data to queues
      xQueueOverwrite(dataQueue, &data);
      xQueueOverwrite(dataAlert, &alert);

      portENTER_CRITICAL(&stateMux);
      sensorValid = true;
      portEXIT_CRITICAL(&stateMux);

      Serial.printf(
        "[SENSOR][CORE %d] GAS=%d | TEMP=%.2f C\n",
        xPortGetCoreID(),
        gas,
        temperature
      );
    } else {
      portENTER_CRITICAL(&stateMux);
      sensorValid = false;
      portEXIT_CRITICAL(&stateMux);

      Serial.println(
        "[SENSOR] DS18B20 read failed!"
      );
    }

    vTaskDelay(
      pdMS_TO_TICKS(SENSOR_MS)
    );
  }
}

// ======================================
// TASK 2 - ALERT
// CORE 1
// ======================================

void taskAlert(void* parameter) {
  AlertData alert;

  bool localGasWarning = false;
  bool localTempWarning = false;

  for (;;) {
    if (
      xQueueReceive(
        dataAlert,
        &alert,
        portMAX_DELAY
      ) == pdTRUE
    ) {
      // GAS CONTROL WITH HYSTERESIS
      if (alert.gas >= GAS_ON) {
        localGasWarning = true;
      } else if (alert.gas <= GAS_OFF) {
        localGasWarning = false;
      }

      // TEMPERATURE CONTROL
      if (alert.temperature >= TEMP_ON) {
        localTempWarning = true;
      } else if (
        alert.temperature <= TEMP_OFF
      ) {
        localTempWarning = false;
      }

      bool mode;
      bool fan;
      bool pump;

      portENTER_CRITICAL(&stateMux);

      gasWarning = localGasWarning;
      tempWarning = localTempWarning;

      mode = autoMode;

      if (mode) {
        fanState = localGasWarning;
        pumpState = localTempWarning;
      }

      fan = fanState;
      pump = pumpState;

      portEXIT_CRITICAL(&stateMux);

      applyOutputs();

      Serial.printf(
        "[ALERT][CORE %d] MODE=%s | FAN=%s | PUMP=%s\n",
        xPortGetCoreID(),
        mode ? "AUTO" : "MANUAL",
        fan ? "ON" : "OFF",
        pump ? "ON" : "OFF"
      );
    }
  }
}

// ======================================
// TASK 3 - SEND DATA
// CORE 0
// ======================================

void taskSendData(void* parameter) {
  SensorData data;

  bool wifiWasConnected = false;

  for (;;) {
    connectWiFi();

    bool wifiConnected =
      WiFi.status() == WL_CONNECTED;

    if (
      wifiConnected &&
      !wifiWasConnected
    ) {
      Serial.println("[WIFI] Connected!");
      Serial.println(WiFi.localIP());
    }

    wifiWasConnected = wifiConnected;

    if (wifiConnected) {
      connectMQTT();
    }

    if (mqtt.connected()) {
      mqtt.loop();

      if (
        xQueueReceive(
          dataQueue,
          &data,
          0
        ) == pdTRUE
      ) {
        publishText(
          "gas",
          String(data.gas)
        );

        publishText(
          "temperature",
          String(data.temperature, 2)
        );

        Serial.printf(
          "[MQTT][CORE %d] GAS=%d | TEMP=%.2f\n",
          xPortGetCoreID(),
          data.gas,
          data.temperature
        );
      }

      if (
        millis() - lastStatePublish >= STATE_MS
      ) {
        lastStatePublish = millis();
        publishStates();
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ======================================
// SETUP
// ======================================

void setup() {
  Serial.begin(115200);

  pinMode(PIN_GAS, INPUT);

  pinMode(PIN_FAN, OUTPUT);
  pinMode(PIN_PUMP, OUTPUT);

  digitalWrite(PIN_FAN, RELAY_OFF);
  digitalWrite(PIN_PUMP, RELAY_OFF);

  analogReadResolution(12);

  tempSensor.begin();
  tempSensor.setResolution(10);

  // CREATE QUEUES
  dataQueue = xQueueCreate(
    1,
    sizeof(SensorData)
  );

  dataAlert = xQueueCreate(
    1,
    sizeof(AlertData)
  );

  if (
    dataQueue == NULL ||
    dataAlert == NULL
  ) {
    Serial.println(
      "[ERROR] Queue creation failed!"
    );

    while (true) {
      delay(1000);
    }
  }

  WiFi.mode(WIFI_STA);

  mqtt.setServer(
    MQTT_HOST,
    MQTT_PORT
  );

  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(2);

  // TASK SEND - CORE 0
  xTaskCreatePinnedToCore(
    taskSendData,
    "SendData",
    6144,
    NULL,
    2,
    NULL,
    0
  );

  // TASK READ - CORE 1
  xTaskCreatePinnedToCore(
    taskReadData,
    "ReadData",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  // TASK ALERT - CORE 1
  xTaskCreatePinnedToCore(
    taskAlert,
    "Alert",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  Serial.println("==========================");
  Serial.println("LAB 3 - FIRE GUARD");
  Serial.println("ESP32 + FreeRTOS + MQTT");
  Serial.println("MODE: AUTO");
  Serial.println("==========================");
}

// ======================================
// LOOP
// ======================================

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
