#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <PubSubClient.h>
#include <esp_task_wdt.h>
#include <ArduinoJson.h>
#include "esp_adc_cal.h"

// UID ESP
struct SensorConfig {
  const char* uid;
  const char* localization;
  const char* placa;
  float coef_angular;
  float coef_linear;
};

// Escolher configuração com base no índice
const int local = 2;

// Lista de configurações
const SensorConfig sensorConfigs[] = {
  {"Sensor_01", "Lab_Automação", "Yagua_01", 0.003432176343986079, -2.316490866974731},
  {"Sensor_02", "Compressor",    "Yagua_02", 0.003760236101179746, -2.5415210396011174},
  {"Sensor_03", "Lab_Controle",  "Yagua_03", 0.0037482378503744898, -2.6853313743316605},
  {"Sensor_04", "Lab_Pneumática",  "Yagua_04", 0.003877723446137859, -2.305875593512546}
};

SensorConfig config = sensorConfigs[local - 1];


// WiFi credentials
const char* ssid = "xxxxxxxxx";
const char* password = "xxxxxxxxxxx";

static unsigned long lastWiFiCheck = 0;


// MQTT broker credentials
const char* mqtt_server = "xxxxxxxxxxxxxx";

// Define the size of the array and the sampling interval
#define SAMPLE_INTERVAL 20  // 20 ms
#define SAMPLE_COUNT 200    // 200 samples to reach 4000 ms
#define FILTER_LEN 5       // 5 buffer length

// Define the analog input pin
#define ANALOG_PIN 34

// Define the board led pin
// #define PIN_LED 4

// Network error counters
int wifiReconnectCount = 0;
int mqttReconnectCount = 0;

// for WTD
int successCount = 0;

// Array to store analog values
float vtValues[SAMPLE_COUNT];
int sampleIndex = 0;

// Variables for statistics
float average = 0;
float maximum = 0;
float minimum = 0;

// Filter variables
uint32_t AN_Pot1_Buffer[FILTER_LEN] = {0};
int AN_Pot1_i = 0;
int AN_Pot1_Raw = 0;
int AN_Pot1_Filtered = 0;

// Webserver variables
String lastPublishedData = "";
bool lastPublishStatus = false;

// Task handles
TaskHandle_t Task0;
TaskHandle_t Task1;

// Web server
WebServer server(80);

// MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

// Fixes complier error "invalid conversion from 'int' to 'const esp_task_wdt_config_t*'":
esp_task_wdt_config_t twdt_config = 
    {
        .timeout_ms = 15000,
        .idle_core_mask = 0x3,
        .trigger_panic = true,
    };

void setup() {
  // E (95) phy_comm: gpio[0] number: 2 is reserved
  // Significa que o pino 2 está sendo reservado pelo sistema Wi-Fi interno do ESP32 ou está envolvido em alguma função crítica de boot ou comunicação.
  // pinMode(PIN_LED, OUTPUT);
  // digitalWrite(PIN_LED, HIGH);
  // Initialize serial communication
  Serial.begin(115200);

  // --- Wi-Fi Scan apenas na inicialização ---
  // Faz a varredura de redes uma vez ao ligar
  Serial.println("Iniciando varredura de redes WiFi...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("Nenhuma rede encontrada.");
  } else {
    Serial.println("Redes encontradas:");
    for (int i = 0; i < n; ++i) {
      String ssid = WiFi.SSID(i);
      int32_t rssi = WiFi.RSSI(i);
      wifi_auth_mode_t encryption = WiFi.encryptionType(i);

      // Verifica se o SSID não está vazio
      if (ssid.length() == 0) {
        ssid = "<SSID oculto>";
      }

      int quality = 2 * (rssi + 100);
      quality = constrain(quality, 0, 100);

      Serial.printf("%d: %s | RSSI: %d dBm | Qualidade: %d%% | %s\n",
                    i + 1,
                    ssid.c_str(),
                    rssi,
                    quality,
                    (encryption == WIFI_AUTH_OPEN) ? "Aberta" : "Protegida");

      delay(10);
    }
  }
  Serial.println("Finalizada a varredura.\n");

  // -------------------------------------------



  // Initialize the watchdog timer
  esp_task_wdt_deinit(); //wdt is enabled by default, so we need to 'deinit' it first
  esp_task_wdt_init(&twdt_config); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);       // Add the main loop to WDT


  // Initialize WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("Connecting to WiFi...");
    wifiReconnectCount++;
  }
  Serial.println("Connected to WiFi");

  // Initialize ElegantOTA
  server.begin();
  ElegantOTA.begin(&server);

  // Initialize MQTT
  client.setServer(mqtt_server, 1883);

  // Connect to MQTT broker
  connectToMQTT();

  // Webserver page
  server.on("/", handleStatusPage);

  // Create tasks for core 0 and core 1
  xTaskCreatePinnedToCore(
    TaskReadAnalog,    // Function to be called
    "TaskReadAnalog",  // Name of the task
    10000,             // Stack size (bytes)
    NULL,              // Parameter to pass
    2,                 // Task priority
    &Task0,            // Task handle
    0                  // Core to run the task on (core 0)
  );

  xTaskCreatePinnedToCore(
    TaskProcessData,    // Function to be called
    "TaskProcessData",  // Name of the task
    10000,              // Stack size (bytes)
    NULL,               // Parameter to pass
    1,                  // Task priority
    &Task1,             // Task handle
    1                   // Core to run the task on (core 1)
  );

}

void loop() {
  // Handle OTA updates
  server.handleClient();
  
  if (millis() - lastWiFiCheck > 10000) {  // Verifica a cada 10 segundos
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi desconectado, tentando reconectar...");
      WiFi.begin(ssid, password);
      wifiReconnectCount++;
    }
  }


  esp_task_wdt_reset();

}

void TaskReadAnalog(void* pvParameters) {
  (void)pvParameters;
  esp_task_wdt_add(NULL);       // Add the current task to WDT

  while (true) {

    // Read analog value and apply the filter
    AN_Pot1_Raw = analogRead(ANALOG_PIN);
    AN_Pot1_Filtered = readADC_Avg(AN_Pot1_Raw);
    
    // Store the filtered value in the array
    vtValues[sampleIndex] = convertToBar(AN_Pot1_Filtered);
    // vtValues[sampleIndex] = AN_Pot1_Filtered;
    sampleIndex++;

    // If array is full, reset the index and notify the other task
    if (sampleIndex >= SAMPLE_COUNT) {
      sampleIndex = 0;
      xTaskNotifyGive(Task1);
    }

    vTaskDelay(SAMPLE_INTERVAL / portTICK_PERIOD_MS);

    esp_task_wdt_reset();
  }
}

void TaskProcessData(void* pvParameters) {
  (void)pvParameters;
  esp_task_wdt_add(NULL);       // Add the current task to WDT

  while (true) {

    // Wait for notification from TaskReadAnalog
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Calculate statistics
    float sum = 0;
    maximum = vtValues[0];
    minimum = vtValues[0];

    for (int i = 0; i < SAMPLE_COUNT; i++) {
      sum += vtValues[i];
      if (vtValues[i] > maximum) {
        maximum = vtValues[i];
      }
      if (vtValues[i] < minimum) {
        minimum = vtValues[i];
      }
    }

    average = sum / SAMPLE_COUNT;

    // Prepare JSON data
    DynamicJsonDocument doc(512);
    // StaticJsonDocument<300> doc;
    doc["ID"] = config.uid;
    doc["Valor"] = average;
    doc["Aplicacao"] = "Ar_Comprimido";
    doc["Local"] = config.localization;
    doc["Tipo"] = "Sensor";
    doc["Variavel"] = "Pressão";
    doc["Unidade"] = "Bar";
    doc["Professor"] = "Paciencia";
    // doc["Min"] = minimum;
    // doc["Max"] = maximum;
    doc["WiFi"] = wifiReconnectCount;
    doc["MQTT"] = mqttReconnectCount;
    doc["IP"] = WiFi.localIP().toString();

    char jsonBuffer[224];
    serializeJson(doc, jsonBuffer);
    
     // Print the results
    Serial.println(WiFi.localIP());
    Serial.println(jsonBuffer);

    // Publish data to MQTT
    if (!client.connected()) {
      connectToMQTT();
    }

    lastPublishedData = String(jsonBuffer);
    lastPublishStatus = client.publish("smartcampus/arcomprimido", jsonBuffer);

    if (!lastPublishStatus) {
      Serial.println("Falha na publicação MQTT.");
      mqttReconnectCount++;
    } else {
      Serial.println("Publicação MQTT realizada com sucesso.");
      successCount++;
    }
   
    client.loop();

    esp_task_wdt_reset();
  }
}

void connectToMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect(config.placa)) {
      Serial.println("connected");
    } else {
      mqttReconnectCount++;  // Increment MQTT error counter
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 1 seconds");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
}

// Function to read ADC value with averaging filter
int readADC_Avg(int newValue) {
  AN_Pot1_Buffer[AN_Pot1_i] = newValue;
  AN_Pot1_i = (AN_Pot1_i + 1) % FILTER_LEN;
  
  uint32_t sum = 0;
  for (int i = 0; i < FILTER_LEN; i++) {
    sum += AN_Pot1_Buffer[i];
  }
  return sum / FILTER_LEN;
}

// Conversão de Digital para Tensão para Bar
float convertToBar(float value) {
  //float t_vcc = value / 4095 * 3.3333;
  //float p_bar = 4.5 * t_vcc - 1.5;
  // float p_bar = 5.01 * t_vcc - 2.79;
  // p_bar = round(p_bar * 100.0) / 100.0;
  float p_bar = config.coef_angular * value + config.coef_linear;
  p_bar = fmax(0.0, p_bar);
  
  // Serial.print("Analog Read: ");
  // Serial.print(value);
  // Serial.print("Bar: ");
  // Serial.println(p_bar);

  return p_bar;

}

void handleStatusPage() {
  String html = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
  html += "<title>Status</title></head><body>";
  html += "<h1>Status de Publicação MQTT - " + String(config.localization) + "</h1>";
  html += "<p><strong>Último Dado Publicado:</strong> " + lastPublishedData + "</p>";
  html += "<p><strong>Status de Publicação:</strong> " + String(lastPublishStatus ? "Sucesso" : "Falha") + "</p>";
  html += "<p><strong>Erros de WiFi:</strong> " + String(wifiReconnectCount) + "</p>";
  html += "<p><strong>Erros de MQTT:</strong> " + String(mqttReconnectCount) + "</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

