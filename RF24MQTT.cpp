#include "RF24MQTT.h"
#if defined (__linux) && !defined(__ARDUINO_X86__)
#include <fstream>
#endif

RF24MQTT::RF24MQTT( RF24& _radio,RF24Network& _network,RF24Mesh& _mesh): radio(_radio),network(_network),mesh(_mesh){};

bool RF24MQTT::begin(uint8_t channel = MESH_DEFAULT_CHANNEL, rf24_datarate_e data_rate = RF24_1MBPS, uint32_t timeout=MESH_RENEWAL_TIMEOUT ){
  return true;
}

uint8_t RF24MQTT::update(){
  mesh.update();
  while (network.available()) {
    RF24NetworkHeader header;
    char payload[255];
    char *topic;
    char *data;
    uint16_t len = network.read(header, &payload, sizeof(payload));
    payload[len] = 0;
    topic = payload;
    uint16_t presize = strlen(topic) + 1;
    data = payload + presize;
    
    if(callback) callback(topic, data, len-presize);
  }
}
bool RF24MQTT::checkConnection(){
  RF24NetworkHeader header;
  uint8_t data[MQTT_MAX_LENGHT]; 
  if(!mesh.write(NULL, MQTT_PING_TYPE, 0)){
    if(!mesh.checkConnection()){
      mesh.renewAddress();
    }
    if(!mesh.write(NULL, MQTT_PING_TYPE, 0)) return false;
  }
  uint32_t start = millis();
  while(millis()-start < MQTT_PING_TIMEOUT){
    if(network.available()){
      byte len = network.read(header, data, MQTT_MAX_LENGHT);
      if(header.type == MQTT_PING_TYPE) return true;
      else proccessMessage(header, data, len);
    }
  }
  return false;  
}

int RF24MQTT::sendAndReply(unsigned char qtype, unsigned char atype, void* data, byte *length, byte node=0){
  RF24NetworkHeader header;
  uint8_t d[MQTT_MAX_LENGHT]; 
  if(!mesh.write(data, qtype, *length, node)){
    if(!mesh.checkConnection()){
      mesh.renewAddress();
    }
    if(!mesh.write(data, qtype, *length)) return MQTT_ERROR_MESH_NET;
  }
  uint32_t start = millis();
  while(millis()-start < MQTT_PING_TIMEOUT){  
    if(network.available()){
      byte len = network.read(header, d, MQTT_MAX_LENGHT);
      if(header.type == atype) return 0;
      else proccessMessage(header, d, len);
    }
  }
  return MQTT_ERROR_TIMEOUT;  
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
