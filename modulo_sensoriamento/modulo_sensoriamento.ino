#include <HardwareSerial.h>
#include <LoRa_E220.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <DHT.h>
#include "extras.h"
#include "packet.h" 
#include "reliable_lora.h"

// LoRa
#define UART 2
#define AUX_PIN 18
#define M0_PIN 32
#define M1_PIN 19
#define UART_BPS_RATE UART_BPS_RATE_9600
#define UART_BPS UART_BPS_9600
HardwareSerial hs(UART);
LoRa_E220 lora(&hs, AUX_PIN, M0_PIN, M1_PIN, UART_BPS_RATE);
struct SysConfigs sc;
byte communication_channel = 64;
byte addr[] = {0,1};
byte receptor_addr[] = {0,2};

int send_delay = 5000;
int reliable = 1;

// Sensors
#define CAP_SOIL_PIN 34
#define RAIN_SENSOR_PIN 35
#define DHT_PIN 4 
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);


void createAndSendSensorReadPacket();
void loraConfig();
void startDHT();
void startAccel();
void loadSensorRead(struct Packet<SensorsRead>* pck);
void sendPacket(struct Packet<SensorsRead>* pck);
byte handshake();

void setup() {
    Serial.begin(9600);
    loadLoraConfig();
    startDHT();
    startAccel();
    lora.setMode(MODE_0_NORMAL);
}
 
void loop() {
    SendSensorsRead();
}

byte SendSensorsRead(){
     if(reliable == 1){
        if(handshake() == 0){
            Serial.println("Erro no handshake");
            return 0;
        }
    }
    Serial.println("Handshake feito");
    struct Packet<SensorsRead> pck;
    struct Packet<byte> pck2;
    unsigned long startTime = millis();
    loadSensorRead(&pck);
    pck.OP = 1;
    sendSensorsRead(&pck);
    while(waitACK(lora, &sc, &pck2) == 0){
        if(millis() - startTime < 3000)
            return 0;
        else
            sendSensorsRead(&pck);
    }
    return 1;
    delay(2000);
}

byte handshake(){
    struct Packet<byte> pck;
    unsigned long startTime = millis();
    while(waitSYN(lora, &pck) == 0){
        if(millis() - startTime >= sc.time_out_handshake){return 0;}
    }
    sendSYNACK(lora, receptor_addr, communication_channel, 1);
    while(waitACK(lora, &sc, &pck) == 0){
        if(millis() - startTime >= sc.time_out_handshake)
            return 0;
        else
            sendSYNACK(lora, receptor_addr, communication_channel, 1);
    }
    return 1;
}


void createAndSendSensorReadPacket(){
    struct Packet<SensorsRead> pck;
    loadSensorRead(&pck);
    pck.OP = 1;
    sendPacket(&pck);
    delay(2000);    
}

byte sendSensorsRead(struct Packet<SensorsRead>* pck){
    ResponseStatus lora_response = lora.sendFixedMessage(receptor_addr[0],receptor_addr[1],communication_channel,pck,sizeof(Packet<SensorsRead>));
    return lora_response.code;
}

void loadLoraConfig(){
    lora.begin();
    ResponseStructContainer c;
    c = lora.getConfiguration();
    Configuration configuration = *(Configuration*) c.data;

    configuration.CHAN = communication_channel; // Communication channel
    configuration.TRANSMISSION_MODE.fixedTransmission = FT_FIXED_TRANSMISSION;
    configuration.SPED.uartBaudRate = UART_BPS; // Serial baud rate
    configuration.ADDH = addr[0];
    configuration.ADDL = addr[1];
    // configuration.TRANSMISSION_MODE.enableLBT = LBT_ENABLED;
    configuration.TRANSMISSION_MODE.enableRSSI = RSSI_ENABLED;

    ResponseStatus rs = lora.setConfiguration(configuration, WRITE_CFG_PWR_DWN_LOSE);
    printParameters(configuration);
    c.close();
}

void startDHT(){
    dht.begin();
}

void startAccel(){
    if(!accel.begin())
    {
        Serial.println("ADXL345 Error");
        while(1);
    }   
}

void loadSensorRead(struct Packet<SensorsRead>* pck){
    sensors_event_t event; 
    accel.getEvent(&event);

    pck->data.accelerometer[0] = event.acceleration.x;
    pck->data.accelerometer[1] = event.acceleration.y;
    pck->data.accelerometer[2] = event.acceleration.z;
    pck->data.air_humidity = dht.readHumidity();
    pck->data.air_temperature = dht.readTemperature();
    pck->data.soil_humidity = analogRead(CAP_SOIL_PIN);
    pck->data.rain_sensor_value = analogRead(RAIN_SENSOR_PIN);

}