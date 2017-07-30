#include "RF24MQTT.h"
#if defined (__linux) && !defined(__ARDUINO_X86__)
#include <fstream>
#endif

RF24MQTT::RF24MQTT( RF24& _radio,RF24Network& _network,RF24Mesh& _mesh): radio(_radio),network(_network),mesh(_mesh){
  ping_time=0;
};

bool RF24MQTT::connect(const char name[]){
  return mesh.write(name, MQTT_CONNECT_TYPE, strlen(name));
}

bool RF24MQTT::disconnect(const char name[]){
  return mesh.write(name, MQTT_DISCON_TYPE, strlen(name));
}

uint8_t RF24MQTT::update(){
  mesh.update();
  while (network.available()) {
    RF24NetworkHeader header;
    network.peek(header);
    dispatchMessage(header);
  }
  if(millis()-ping_time > MQTT_PING_PERIOD){
    Serial.print("Ping");
    if(checkConnection()) Serial.println(" OK");
    else Serial.println(" fail");
    ping_time = millis();
  }
}

void RF24MQTT::dispatchMessage(RF24NetworkHeader& header){
    if(incomingMessage(header)) return;
    Serial.print("Received message ");
    Serial.println((char)header.type);
    network.read(header, NULL, 0); //Zahoď neznámý paket
}

bool RF24MQTT::incomingMessage(RF24NetworkHeader& header){
  if(header.type != MQTT_RECIEVE_TYPE) return false;

  char payload[255];
  char *topic;
  char *data;

  uint16_t len = network.read(header, &payload, sizeof(payload));
  payload[len] = 0;
  topic = payload;
  uint16_t presize = strlen(topic) + 1;
  data = payload + presize;

  if(callback) callback(topic, data, len-presize);

  return true;
}

bool RF24MQTT::checkConnection(){
  RF24NetworkHeader header;
  uint8_t data[MQTT_MAX_LENGHT];
  ping_cnt++;
  if(!mesh.write(&ping_cnt, MQTT_PING_TYPE, 1)){
    if(!mesh.checkConnection()){
      mesh.renewAddress();
    }
    if(!mesh.write(NULL, MQTT_PING_TYPE, 0)) return false;
  }
  uint32_t start = millis();
  while(millis()-start < MQTT_PING_TIMEOUT){
    mesh.update();
    if(network.available()){
      network.peek(header);
      if(header.type == MQTT_PONG_TYPE){
        uint8_t cnt;
        network.read(header, &cnt, 1);
        if(cnt == ping_cnt) return true;
      }
      dispatchMessage(header);
    }
  }
  return false;  
}

void RF24MQTT::proccessMessage(RF24NetworkHeader& header, void* payload, byte length){
}

int RF24MQTT::publish(const char topic[], void* payload, byte length, bool retained){
  uint8_t data[MQTT_MAX_LENGHT];
  uint8_t topic_len = strlen(topic);
  strcpy(data, topic);
  memcpy(data+topic_len+1, payload, length);
  return mesh.write(data, MQTT_PUBLISH_TYPE, topic_len+length+1);
}

bool RF24MQTT::subscribe(const char topic[], unsigned char qos = 0){
  return mesh.write(topic, MQTT_SUBSCRIBE_TYPE, strlen(topic));
}
bool RF24MQTT::unsubscribe(const char topic[]){
  return mesh.write(topic, MQTT_UNSUBSC_TYPE, strlen(topic));
}

void RF24MQTT::setCallback(mqtt_callback_t _callback){
  callback = _callback;
}

byte getTopicFromId(uint16_t topic, char topicName[], byte length); 
