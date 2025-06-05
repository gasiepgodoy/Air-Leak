#include <Arduino.h>

// Definições
#define ANALOG_PIN 34    // Pino analógico
#define FILTER_LEN 1     // Comprimento do filtro (sample de 1)

// Variáveis do filtro
uint32_t AN_Pot1_Buffer[FILTER_LEN] = {0};
int AN_Pot1_i = 0;

// Função para ler o ADC com filtro
int readADC_Avg(int newValue) {
  AN_Pot1_Buffer[AN_Pot1_i] = newValue;
  AN_Pot1_i = (AN_Pot1_i + 1) % FILTER_LEN;
  
  uint32_t sum = 0;
  for (int i = 0; i < FILTER_LEN; i++) {
    sum += AN_Pot1_Buffer[i];
  }
  return sum / FILTER_LEN;
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // Aguarda serial estabilizar
  Serial.println("Iniciando leitura ADC...");
}

void loop() {
  int rawValue = analogRead(ANALOG_PIN);        // Lê o valor bruto
  int filteredValue = readADC_Avg(rawValue);     // Aplica o filtro

  Serial.print("ADC Raw: ");
  Serial.print(rawValue);
  Serial.print(" | ADC Filtrado: ");
  Serial.println(filteredValue);

  delay(500);  // Intervalo entre leituras
}
