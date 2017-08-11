#include "RF24MQTT.h"
#if defined (__linux) && !defined(__ARDUINO_X86__)
#include <fstream>
#endif

RF24MQTT::RF24MQTT( RF24& _radio,RF24Network& _network,RF24Mesh& _mesh, const char name[]): radio(_radio),network(_network),mesh(_mesh){
  ping_time=0;
  client_name=name;
};

bool RF24MQTT::connect(const char name[]){
  if(name) client_name = name;
  if(!mesh.write(client_name, MQTT_CONNECT_TYPE, strlen(client_name))) return false;

  if(static_topics){
    for(byte i=0;i<sizeof(static_topics); i++){
      subscribe(static_topics[i]);
    }
  }
  return true;
}

bool RF24MQTT::disconnect(){
  return mesh.write(NULL, MQTT_DISCON_TYPE, 0);
}

uint8_t RF24MQTT::update(){
  bool state= false;
  mesh.update();
  while (network.available()) {
    RF24NetworkHeader header;
    network.peek(header);
    dispatchMessage(header);
  }
  if(millis()-ping_time > MQTT_PING_PERIOD){
	state = checkConnection();
    ping_time = millis();
  }
  return state;
}

void RF24MQTT::dispatchMessage(RF24NetworkHeader& header){
    if(incomingMessage(header)) return;
    network.read(header, NULL, 0); //Zahoď neznámý paket
}

bool RF24MQTT::incomingMessage(RF24NetworkHeader& header){
  if(header.type != MQTT_RECIEVE_TYPE) return false;

  char payload[255];
  char *topic;
  byte *data;

  uint16_t len = network.read(header, &payload, sizeof(payload));
  payload[len] = 0;
  topic = payload;
  uint16_t presize = strlen(topic) + 1;
  data = (byte *)(payload + presize);

  if(callback) callback(topic, data, len-presize);

  return true;
}

bool RF24MQTT::checkConnection(){
  RF24NetworkHeader header;
  ping_cnt++;

  if(!mesh.checkConnection()){
    mesh.renewAddress();
    connect(NULL);
  }
  if(!mesh.write(&ping_cnt, MQTT_PING_TYPE, 1)) return false;

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

void RF24MQTT::proccessMessage(RF24NetworkHeader& header, const void* payload, byte length){
}

int RF24MQTT::publish(const char topic[], const void* payload, byte length, bool retained){
  uint8_t data[MQTT_MAX_LENGHT];
  uint8_t topic_len = strlen(topic);
  strcpy((char *)data, topic);
  memcpy(data+topic_len+1, payload, length);
  return mesh.write(data, MQTT_PUBLISH_TYPE, topic_len+length+1);
}

bool RF24MQTT::subscribe(const char topic[], unsigned char qos){
  return mesh.write(topic, MQTT_SUBSCRIBE_TYPE, strlen(topic));
}
bool RF24MQTT::unsubscribe(const char topic[]){
  return mesh.write(topic, MQTT_UNSUBSC_TYPE, strlen(topic));
}

void RF24MQTT::setCallback(mqtt_callback_t _callback){
  callback = _callback;
}

void RF24MQTT::setStaticTopics(topics_t topics){
  static_topics = topics;
}
