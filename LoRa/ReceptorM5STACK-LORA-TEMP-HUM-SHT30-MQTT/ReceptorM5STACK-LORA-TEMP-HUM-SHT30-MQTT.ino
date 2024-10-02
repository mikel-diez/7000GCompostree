#include <M5Stack.h>
#include <M5LoRa.h>

#include <PubSubClient.h>
#include <WiFi.h>

WiFiClient espClient;
PubSubClient client(espClient);

// Configuración WiFi
const char* ssid        = "SOMORACERS_2GHz";
const char* password    = "mvpemqcv";

// Configuración MQTT
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* temp_topic = "M5/loRa/temperatura";
const char* humidity_topic = "M5/loRa/humedad";
const char* CO2_topic = "M5/loRa/CO2";
const char* RSSI_topic = "M5/loRa/RSSI";
// /dev/+/+/sensor/*
// /dev/loRa/1/sensor/hum

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

void setupWifi();
void callback(char* topic, byte* payload, unsigned int length);
void reConnect();

void setup() 
{
    M5.begin();
    M5.Power.begin();
    
    setupWifi();
    client.setServer(mqtt_server,mqtt_port);  // Sets the server details.
    client.setCallback(callback);  // Sets the message callback function.

    // override the default CS, reset, and IRQ pins (optional)
    LoRa.setPins();  // default set CS, reset, IRQ pin
    Serial.println("LoRa Receiver");
    M5.Lcd.println("LoRa Receiver");

    // frequency in Hz (433E6, 866E6, 915E6)
    if (!LoRa.begin(866E6)) {
        Serial.println("Starting LoRa failed!");
        M5.Lcd.println("Starting LoRa failed!");
        while (1)
            ;
    }

    // LoRa.setSyncWord(0x69);
    Serial.println("LoRa init succeeded.");
    M5.Lcd.println("LoRa init succeeded.");
}

void loop() 
{
  if (!client.connected()) 
       {
        reConnect();
       }
  client.loop();  // This function is called periodically to allow clients to
                    // process incoming messages and maintain connections to the
                    // server.
    
  // try to parse packet
  int packetSize = LoRa.parsePacket();

  if (packetSize) 
       {
        // received a packet
        Serial.print("Received packet: \"");
        M5.Lcd.print("Received packet: \"");

        String loRaData = "";

        // read packet
        while (LoRa.available()) 
           {
            loRaData += (char)LoRa.read();
            
            // Serial.print(loRaData);
            // M5.Lcd.print(loRaData);  
            
           }

        //Serial.println("Datos LoRa recibidos: " + loRaData);
        Serial.println("Datos LoRa recibidos: ");
        Serial.println(loRaData);

        // Parsear los datos recibidos
        float temperature = NAN;
        float humidity = NAN;
        //float co2 = NAN;
        int co2;

        // Suponiendo que los datos están en el formato "T: H: CO2:"
        int tempIndex = loRaData.indexOf("T:");
        int humIndex = loRaData.indexOf("H:");
        int co2Index = loRaData.indexOf("CO2:");

        if (tempIndex != -1) 
          {
            int endIndex = loRaData.indexOf(" ", tempIndex);
            temperature = loRaData.substring(tempIndex + 2, endIndex).toFloat();
            Serial.println(temperature);
          }
        if (humIndex != -1) 
          {
            int endIndex = loRaData.indexOf(" ", humIndex);
            humidity = loRaData.substring(humIndex + 2, endIndex).toFloat();
            Serial.println(humidity);
          }
        if (co2Index != -1) 
          {
            //co2 = loRaData.substring(co2Index + 4).toFloat();  // Hasta el final de la cadena
            co2 = loRaData.substring(co2Index + 4).toInt();
            Serial.println(co2);
          }

        // Publicar datos en MQTT
        if (!isnan(temperature)) 
          {
            String tempString = String(temperature);
            client.publish(temp_topic, tempString.c_str());
          }  
        if (!isnan(humidity)) 
          {
            String humString = String(humidity);
            client.publish(humidity_topic, humString.c_str());
          }
        if (!isnan(co2)) 
          {
            String co2String = String(co2);
            client.publish(CO2_topic, co2String.c_str());
          }
        else 
          {
            Serial.println("Formato de datos LoRa incorrecto.");
          }

        // Leer RSSI del último paquete recibido
        int rssi = LoRa.packetRssi();
        Serial.print("\" with RSSI ");
        Serial.println(rssi);
        M5.Lcd.print("\" with RSSI ");
        M5.Lcd.println(rssi);

        // Publicar RSSI en MQTT
        String rssiString = String(rssi);
        client.publish(RSSI_topic, rssiString.c_str());

    }
}

void setupWifi() 
   {
    delay(10);
    M5.Lcd.printf("Connecting to %s", ssid);
    WiFi.mode(
        WIFI_STA);  // Set the mode to WiFi station mode.  设置模式为WIFI站模式
    WiFi.begin(ssid, password);  // Start Wifi connection.  开始wifi连接

    while (WiFi.status() != WL_CONNECTED) 
       {
        delay(500);
        M5.Lcd.print(".");
       }
    M5.Lcd.printf("\nSuccess\n");
   }

void callback(char* topic, byte* payload, unsigned int length) 
   {
    M5.Lcd.print("Message arrived [");
    M5.Lcd.print(topic);
    M5.Lcd.print("] ");
    for (int i = 0; i < length; i++) 
       {
        M5.Lcd.print((char)payload[i]);
       }
    M5.Lcd.println();
   }

void reConnect() 
  {
    while (!client.connected()) 
       {
        M5.Lcd.print("Attempting MQTT connection...");
        // Create a random client ID.  创建一个随机的客户端ID
        String clientId = "M5Stack-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect.  尝试重新连接
        if (client.connect(clientId.c_str())) 
           {
            M5.Lcd.printf("\nSuccess\n");
            // Once connected, publish an announcement to the topic.
            // 一旦连接，发送一条消息至指定话题
            client.publish("M5Stack", "hello world");
            // ... and resubscribe.  重新订阅话题
            client.subscribe("M5Stack");
           } else 
             {
               M5.Lcd.print("failed, rc=");
               M5.Lcd.print(client.state());
               M5.Lcd.println("try again in 5 seconds");
               delay(5000);
             }
        }
  }
