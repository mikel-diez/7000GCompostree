/*
Mikel Diez Garcia (arraiz) - (mikel.diez@somorrostro.com)
Device2 (dev2) for NB-IOT comm sensing 
*/


#include <DHT.h>
#include <ArduinoJson.h>
#define DHTPIN 32
#define DHTTYPE DHT11 


#define TINY_GSM_MODEM_SIM7000
#define SerialMon Serial
#define SerialAT Serial1
#define TINY_GSM_DEBUG SerialMon
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#define GSM_PIN ""

const int device = 1; 
const int topicLen = 100;
const char apn[]      = "nb.es.vodafone.iot";
const char gprsUser[] = "";
const char gprsPass[] = "";

unsigned long reconnectStartTime = 0;

// Your WiFi connection credentials, if applicable
const char wifiSSID[] = "YourSSID";
const char wifiPass[] = "YourWiFiPass";

// MQTT details
const char *broker = "broker.emqx.io";


const char topicInit[] = "initTopic";



char topicStatus[topicLen];
char topicNo[topicLen];
char topicLat[topicLen];
char topicLon[topicLen];
char topicTemp[topicLen];
char topicHum[topicLen];
char topicCo2[topicLen];
char topicPos[topicLen];  


 


float temp;
float hum;

char lonBuffer[20]; 
char latBuffer[20]; 
char tempBuffer[10]; 
char humBuffer[10];
char co2Buffer[10];
char noBuffer[10];


unsigned long lastLatPublish = 0;  // Timestamp of the last latitude publish

float lat,  lon, co2;

#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <SPI.h>
#include <SD.h>
#include <MQUnifiedsensor.h>



#define         Board                   ("ESP-32") // Wemos ESP-32 or other board, whatever have ESP32 core.
#define         Pin                     (36) //check the esp32-wroom-32d.jpg image on ESP32 folder 
#define         Type                    ("MQ-3") //MQ2 or other MQ Sensor, if change this verify your a and b values.
#define         Voltage_Resolution      (3.3) // 3V3 <- IMPORTANT. Source: https://randomnerdtutorials.com/esp32-adc-analog-read-arduino-ide/
#define         ADC_Bit_Resolution      (12) // ESP-32 bit resolution. Source: https://randomnerdtutorials.com/esp32-adc-analog-read-arduino-ide/
#define         RatioMQ2CleanAir        (9.83) //RS / R0 = 9.83 ppm
MQUnifiedsensor MQ2(Board, Voltage_Resolution, ADC_Bit_Resolution, Pin, Type);

#if TINY_GSM_USE_GPRS && not defined TINY_GSM_MODEM_HAS_GPRS
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS false
#define TINY_GSM_USE_WIFI true
#endif
#if TINY_GSM_USE_WIFI && not defined TINY_GSM_MODEM_HAS_WIFI
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#endif

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif
TinyGsmClient client(modem);
PubSubClient  mqtt(client);



Ticker tick;



#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  60          // Time ESP32 will go to sleep (in seconds)

#define UART_BAUD   115200
#define PIN_DTR     25
#define PIN_TX      27
#define PIN_RX      26
#define PWR_PIN     4

#define SD_MISO     2
#define SD_MOSI     15
#define SD_SCLK     14
#define SD_CS       13
#define LED_PIN     12

int ledStatus = LOW;

uint32_t lastReconnectAttempt = 0;
DHT dht(DHTPIN, DHTTYPE);

void mqttCallback(char *topic, byte *payload, unsigned int len)
{

    SerialMon.print("Message arrived [");
    SerialMon.print(topic);
    SerialMon.print("]: ");
    SerialMon.write(payload, len);
    SerialMon.println();

}
void enableGPS(void)
{
    modem.sendAT("+CGPIO=0,48,1,1");
    if (modem.waitResponse(10000L) != 1) {
        DBG("Set GPS Power HIGH Failed");
    }
    modem.enableGPS();
}

void disableGPS(void)
{
  
    modem.sendAT("+CGPIO=0,48,1,0");
    if (modem.waitResponse(10000L) != 1) {
        DBG("Set GPS Power LOW Failed");
    }
    modem.disableGPS();
}
boolean mqttConnect()
{
    SerialMon.print("Connecting to ");
    SerialMon.print(broker);

    // Connect to MQTT Broker
    boolean status = mqtt.connect(broker);

    // Or, if you want to authenticate MQTT:
    // boolean status = mqtt.connect("GsmClientName", "mqtt_user", "mqtt_pass");

    if (status == false) {
        SerialMon.println(" fail");
        return false;
    }
    SerialMon.println(" success");
    mqtt.publish(topicInit, "7000G Started");
    return mqtt.connected();
}


void setup()
{
    // Set console baud rate
    Serial.begin(115200);
    delay(10);

    sprintf(topicStatus, "/dev/%d/sensor/up", device);
    sprintf(topicNo, "/dev/%d/sensor/no", device);
    sprintf(topicLat, "/dev/%d/sensor/lat", device);
    sprintf(topicLon, "/dev/%d/sensor/lon", device);
    sprintf(topicTemp, "/dev/%d/sensor/tmp", device);
    sprintf(topicHum, "/dev/%d/sensor/hum", device);
    sprintf(topicCo2, "/dev/%d/sensor/co2", device);
    sprintf(topicPos, "/json/dev/%d/sensor/pos", device);



    Serial.print("device: ");
    Serial.println(device);

    sprintf(noBuffer, "%d", device);

    MQ2.setRegressionMethod(1); //_PPM =  a*ratio^b
    MQ2.setA(987.99); MQ2.setB(-2.162); // Configure the equation to to calculate H2 concentration
    MQ2.init(); 
    Serial.print("Calibrating please wait.");
    float calcR0 = 0;
    for(int i = 1; i<=10; i ++)
    {
    MQ2.update(); // Update data, the arduino will read the voltage from the analog pin
    calcR0 += MQ2.calibrate(RatioMQ2CleanAir);
    Serial.print(".");
    }
    MQ2.setR0(calcR0/10);
    Serial.println("  done!.");
    if(isinf(calcR0)) {Serial.println("Warning: Conection issue, R0 is infinite (Open circuit detected) please check your wiring and supply"); while(1);}
    if(calcR0 == 0){Serial.println("Warning: Conection issue found, R0 is zero (Analog pin shorts to ground) please check your wiring and supply"); while(1);}
    
    dht.begin();
    // Set LED OFF
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    pinMode(PWR_PIN, OUTPUT);
    digitalWrite(PWR_PIN, HIGH);
    // Starting the machine requires at least 1 second of low level, and with a level conversion, the levels are opposite
    delay(1000);
    digitalWrite(PWR_PIN, LOW);

    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS)) {
        Serial.println("SDCard MOUNT FAIL");
    } else {
        uint32_t cardSize = SD.cardSize() / (1024 * 1024);
        String str = "SDCard Size: " + String(cardSize) + "MB";
        Serial.println(str);
    }

    Serial.println("\nWait...");

    delay(1000);

    SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

    // Restart takes quite some time
    // To skip it, call init() instead of restart()
    Serial.println("Initializing modem...");
    if (!modem.restart()) {
        Serial.println("Failed to restart modem, attempting to continue without restarting");
    }



    String name = modem.getModemName();
    DBG("Modem Name:", name);

    String modemInfo = modem.getModemInfo();
    DBG("Modem Info:", modemInfo);

#if TINY_GSM_USE_GPRS
    // Unlock your SIM card with a PIN if needed
    if (GSM_PIN && modem.getSimStatus() != 3) {
        modem.simUnlock(GSM_PIN);
    }
#endif

#if TINY_GSM_USE_WIFI
    // Wifi connection parameters must be set before waiting for the network
    SerialMon.print(F("Setting SSID/password..."));
    if (!modem.networkConnect(wifiSSID, wifiPass)) {
        SerialMon.println(" fail");
        delay(10000);
        return;
    }
    SerialMon.println(" success");
#endif

#if TINY_GSM_USE_GPRS && defined TINY_GSM_MODEM_XBEE
    // The XBee must run the gprsConnect function BEFORE waiting for network!
    modem.gprsConnect(apn, gprsUser, gprsPass);
#endif

    SerialMon.print("Waiting for network...");
    if (!modem.waitForNetwork()) {
        SerialMon.println(" fail");
        delay(10000);
        return;
    }
    SerialMon.println(" success");

    if (modem.isNetworkConnected()) {
        SerialMon.println("Network connected");
    }

#if TINY_GSM_USE_GPRS
    // GPRS connection parameters are usually set after network registration
    SerialMon.print(F("Connecting to "));
    SerialMon.print(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(" fail");
        delay(10000);
        return;
    }
    SerialMon.println(" success");

    if (modem.isGprsConnected()) {
        SerialMon.println("GPRS connected");
          modem.sendAT("+CPSI?");
          String r = SerialAT.readString();
          SerialMon.println(r);
          DBG("Set GPS Power LOW Failed");

    }
#endif

    // MQTT Broker setup
    //mqtt.setServer(broker, 1243);
    mqtt.setServer(broker, 1883);
    mqtt.setCallback(mqttCallback);



    enableGPS();
    Serial.println("Searching for GPS signal, pls wait...");
    while (1) {
        if (modem.getGPS(&lat, &lon)) {
            Serial.println("The location has been locked, the latitude and longitude are:");
            Serial.print("latitude:"); Serial.println(lat);
            Serial.print("longitude:"); Serial.println(lon);
            
            dtostrf(lon, 0, 10, lonBuffer); 
            dtostrf(lat, 0, 10, latBuffer);  // Convert the float to a char array with 6 digits after the decimal point
            break;
        }
    }

    disableGPS();

}

void loop()
{


    // Make sure we're still registered on the network
    if (!modem.isNetworkConnected()) {
        SerialMon.println("Network disconnected");
        if (!modem.waitForNetwork(180000L, true)) {
            SerialMon.println(" fail");
            delay(10000);
            return;
        }
        if (modem.isNetworkConnected()) {
            SerialMon.println("Network re-connected");
        }

#if TINY_GSM_USE_GPRS
        // and make sure GPRS/EPS is still connected
        if (!modem.isGprsConnected()) {
            SerialMon.println("GPRS disconnected!");
            SerialMon.print(F("Connecting to "));
            SerialMon.print(apn);
            if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
                SerialMon.println(" fail");
                delay(10000);
                return;
            }
            if (modem.isGprsConnected()) {
                SerialMon.println("GPRS reconnected");
            }
        }
#endif
    }

    if (!mqtt.connected()) {
        SerialMon.println("=== MQTT NOT CONNECTED ===");
        // Reconnect every 10 seconds
        uint32_t t = millis();
        
        if (reconnectStartTime == 0) {
        reconnectStartTime = t;  // Guarda el momento inicial
        }


        if (t - reconnectStartTime > 60000L) {
            SerialMon.println("Unable to connect after 1 minute, resetting...");
            ESP.restart(); 
        }


        if (t - lastReconnectAttempt > 10000L) {
            lastReconnectAttempt = t;
            SerialMon.println("Reconecting");
            if (mqttConnect()) {
                lastReconnectAttempt = 0;
                 reconnectStartTime = 0;
            }
        }
        delay(100);
        return;
    }

    mqtt.loop();


 StaticJsonDocument<200> jsonDoc;
  jsonDoc["lat"] = lat; 
  jsonDoc["lon"] = lon; 
  jsonDoc["no"] = device; 



  char JSONbuffer[200];
  serializeJson(jsonDoc, JSONbuffer);


if (millis() - lastLatPublish > 10000) {
    lastLatPublish = millis();

    // Asegurar la conexión MQTT está activa
    // if (!mqtt.connected()) {
    //     mqttConnect();  
    // }

   
    temp = dht.readTemperature();
    MQ2.update();
    co2 = MQ2.readSensor();
    dtostrf(co2, 1, 2, co2Buffer);
    if (!isnan(temp)) {
        dtostrf(temp, 1, 2, tempBuffer);
        mqtt.publish(topicTemp, tempBuffer);
        delay(10);
    } else {
        Serial.println("Error al leer la temperatura del DHT11");
    }

  
    hum = dht.readHumidity();
    
    if (!isnan(hum)) {
        dtostrf(hum, 1, 2, humBuffer);
        mqtt.publish(topicHum, humBuffer);
        delay(10);
    } else {
        Serial.println("Error al leer la humedad del DHT11");
    }

    mqtt.publish(topicLat, latBuffer);
    delay(10);
    mqtt.publish(topicLon, lonBuffer);
    delay(10);
    mqtt.publish(topicCo2, co2Buffer);
    delay(10);
    mqtt.publish(topicNo, noBuffer);
    delay(10);
    mqtt.publish(topicPos, JSONbuffer);
    delay(10);
    Serial.println("Data Sent");
}


}