# ESP32 Sensor de Pressão com MQTT, OTA e Webserver

Este projeto implementa um sistema embarcado com **ESP32** que realiza leitura de sensores analógicos (pressão), aplica filtros, calcula estatísticas e publica os dados via **MQTT**. Também possui uma interface web de monitoramento e suporte a **atualização OTA via ElegantOTA**.

## Visão Geral do Projeto

* Leitura analógica de pressão via ADC
* Conversão para unidade de pressão (*bar*)
* Filtro de média deslizante
* Coleta de amostras em buffer
* Cálculo de média, mínimo e máximo
* Publicação via MQTT em JSON
* Monitoramento via WebServer local
* Atualização Over-The-Air com ElegantOTA
* Monitoramento e reconexão de WiFi e MQTT
* Proteção com *Watchdog Timer*

---

## Estrutura do Projeto

### Configuração de Sensores

```cpp
struct SensorConfig {
  const char* uid;
  const char* localization;
  const char* placa;
  float coef_angular;
  float coef_linear;
};
```

* Cada sensor possui uma configuração com UID, local de instalação, identificação e coeficientes de calibração (`y = ax + b`) para converter valores brutos em *bar*.

---

### Conexão WiFi e MQTT

```cpp
const char* ssid = "xxxx";
const char* password = "xxxx";
const char* mqtt_server = "xxxx";
```

* Parâmetros de rede definidos para conectar à internet e ao broker MQTT.
* O sistema tenta reconectar automaticamente em caso de falha.

---

### ⏱Watchdog Timer

```cpp
esp_task_wdt_init(&twdt_config);
esp_task_wdt_add(NULL);
```

* O watchdog reinicia o ESP32 se as tarefas principais travarem.
* Cada tarefa se registra e precisa fazer `esp_task_wdt_reset()` periodicamente.

---

### WebServer + ElegantOTA

```cpp
server.begin();
ElegantOTA.begin(&server);
server.on("/", handleStatusPage);
```

* Cria uma página web local para visualizar o status de publicação.
* Permite atualização do firmware OTA (Over-the-Air) de forma segura.

---

### Multitarefas com xTaskCreatePinnedToCore

```cpp
xTaskCreatePinnedToCore(TaskReadAnalog, ... core 0);
xTaskCreatePinnedToCore(TaskProcessData, ... core 1);
```

* Leitura analógica e processamento de dados são executados em paralelo.
* A leitura (tarefa mais crítica) fica no Core 0, e o processamento no Core 1.

---

### Leitura Analógica e Filtro

```cpp
int readADC_Avg(int newValue) {
  AN_Pot1_Buffer[AN_Pot1_i] = newValue;
  ...
  return sum / FILTER_LEN;
}
```

* Aplica um filtro de média móvel com 5 amostras para suavizar ruído.
* O valor filtrado é convertido para pressão (Bar) com `convertToBar()`.

---

### Processamento e Publicação

```cpp
float average = sum / SAMPLE_COUNT;

doc["Valor"] = average;
client.publish("smartcampus/arcomprimido", jsonBuffer);
```

* A cada 200 amostras (4 segundos), calcula-se a média, mínima e máxima.
* Um JSON é montado e enviado para o tópico MQTT com as informações do sensor.

---

### Página Web Local

```cpp
void handleStatusPage() {
  ...
  html += "<h1>Status de Publicação MQTT - " + String(config.localization) + "</h1>";
}
```

* Página HTML simples mostra o último dado enviado, status da publicação e erros de rede acumulados.

---

## Exemplo de Payload (MQTT JSON)

```json
{
  "ID": "Sensor_03",
  "Valor": 4.72,
  "Aplicacao": "Ar_Comprimido",
  "Local": "Lab_Controle",
  "Tipo": "Sensor",
  "Variavel": "Pressão",
  "Unidade": "Bar",
  "Professor": "Paciencia",
  "WiFi": 0,
  "MQTT": 0,
  "IP": "192.168.0.123"
}
```

---

## Reconexão Automática

* Caso o WiFi ou MQTT desconectem, o sistema tenta reconectar automaticamente, registrando os erros.
* Requisições pendentes do OTA e WebServer continuam funcionando após reconexão.

---

## Dependências

* `WiFi.h`
* `WebServer.h`
* `ElegantOTA.h`
* `PubSubClient.h`
* `esp_task_wdt.h`
* `ArduinoJson.h`
* `esp_adc_cal.h`

---

## Personalização

* Altere a constante `local = X` para usar uma configuração específica do sensor.
* Altere os valores de `ssid`, `password`, e `mqtt_server` conforme sua rede.
* Modifique os coeficientes de conversão `coef_angular` e `coef_linear` conforme seu sensor.

---

## Sugestões Futuras

* Adicionar autenticação no servidor OTA
* Implementar fallback em caso de falha de publicação contínua
* Salvar dados em cartão SD ou memória flash como backup
