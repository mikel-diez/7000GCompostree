#include <M5Stack.h>
#include "DHT.h"
#include <M5LoRa.h>

// Definir el pin al que está conectado el DHT11
#define DHTPIN 5

// Definir el tipo de sensor DHT (en este caso, DHT11)
#define DHTTYPE DHT11

// Inicializar el objeto del sensor DHT
DHT dht(DHTPIN, DHTTYPE);

// Función para obtener la temperatura
float obtenerTemperatura() 
{
  float temp = dht.readTemperature();
  if (isnan(temp)) 
  {
    Serial.println("Error al leer la temperatura");
    return NAN;
  }
  return temp;
}

// Función para obtener la humedad
float obtenerHumedad() 
{
  float hum = dht.readHumidity();
  if (isnan(hum)) 
  {
    Serial.println("Error al leer la humedad");
    return NAN;
  }
  return hum;
}

// Función para inicializar LoRa
void iniciarLoRa() 
{
  // Configuración del LoRa con frecuencia de 868 MHz
  if (!LoRa.begin(868E6)) 
  {
    Serial.println("Error al inicializar LoRa");
    while (1);
  }
  Serial.println("LoRa inicializado correctamente");
}

// Función para enviar los datos por LoRa
void enviarDatosLoRa(float temp, float hum) 
{
  // Iniciar la transmisión de paquete
  LoRa.beginPacket();
  
  // Escribir los datos de temperatura y humedad en el paquete
  LoRa.print("Temperatura: ");
  LoRa.print(temp);
  LoRa.print(" °C, Humedad: ");
  LoRa.print(hum);
  LoRa.println(" %");

  // Finalizar el paquete y enviarlo
  LoRa.endPacket();

  // Mostrar mensaje de confirmación
  Serial.println("Datos enviados vía LoRa");
}

void setup() 
{
  // Iniciar la comunicación serial
  Serial.begin(115200);

    // Iniciar el M5Stack
  M5.begin();
  
  // Iniciar el sensor DHT
  dht.begin();

  // Limpiar la pantalla
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);

  // Iniciar el módulo LoRa
  iniciarLoRa();
}

void loop() 
{
  // Esperar un par de segundos entre lecturas
  delay(2000);

  // Llamar a las funciones para obtener la temperatura y la humedad
  float temperature = obtenerTemperatura();
  float humidity = obtenerHumedad();

  // Comprobar si hay algún error al leer el sensor
  if (isnan(temperature) || isnan(humidity)) 
  {
    M5.Lcd.setCursor(0, 40);
    M5.Lcd.print("Error al leer");
    return;
  }

  // Limpiar la pantalla y mostrar la temperatura y la humedad
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 20);
  M5.Lcd.print("Temperatura: ");
  M5.Lcd.print(temperature);
  M5.Lcd.println(" °C");

  M5.Lcd.setCursor(0, 60);
  M5.Lcd.print("Humedad: ");
  M5.Lcd.print(humidity);
  M5.Lcd.println(" %");

  // Imprimir la temperatura y la humedad por el puerto serie
  Serial.print("Temperatura: ");
  Serial.print(temperature);
  Serial.print(" °C, Humedad: ");
  Serial.print(humidity);
  Serial.println(" %");

  // Enviar los datos por LoRa
  enviarDatosLoRa(temperature, humidity);
}
