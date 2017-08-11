#ifndef __RF24MQTT_H__
#define __RF24MQTT_H__

#include <stdio.h>
#include <stdlib.h>

#if defined (__linux) && !defined(__ARDUINO_X86__)
  #include <RF24/RF24.h>
  #include <RF24Network/RF24Network.h>
  #include <RF24Mesh/RF24Mesh.h>
  #define RF24_LINUX
#else
  #include <RF24.h>
  #include <RF24Network.h>
  #include <RF24Mesh.h>
#endif

#define MQTT_PING_TIMEOUT   2000
#define MQTT_PING_PERIOD   20000
#define MQTT_MAX_LENGHT      255

#define MQTT_PING_TYPE      'p'
#define MQTT_PONG_TYPE      'r'
#define MQTT_CONNECT_TYPE   'C'
#define MQTT_DISCON_TYPE    'D'
#define MQTT_PUBLISH_TYPE   'P'
#define MQTT_SUBSCRIBE_TYPE 'S'
#define MQTT_UNSUBSC_TYPE   'U'
#define MQTT_RECIEVE_TYPE   'R'

typedef void (*mqtt_callback_t)(const char topic[], byte* payload, unsigned int length);
typedef char *topics_t[];

class RF24;
class RF24Network;
class RF24Mesh;

class RF24MQTT
{
  /**@}*/
  /**
   * @name RF24MQTT
   *
   *  The mesh library and class documentation is currently in active development and usage may change.
   */
  /**@{*/
  public:

    /**
   * Construct the mesh:
   *
   * @code
   * RF24 radio(7,8);
   * RF24Network network(radio);
   * RF24Mesh mesh(radio,network);
   * RF24MQTT mqtt(radio,network,mesh);
   * @endcode
   * @param _radio The underlying radio driver instance
   * @param _network The underlying network instance
   * @param _mesh The underlying mesh instance
   */

  RF24MQTT(RF24& _radio, RF24Network& _network, RF24Mesh& _mesh, const char name[]=NULL);

   /**
   * Very similar to network.update(), it needs to be called regularly to keep the network
   * and the mesh going.
   */   
  uint8_t update();

   /**
   * Tests connectivity of this node to the mesh.
   * @note If this function fails, the radio will be put into standby mode, and will not receive payloads until the address is renewed.
   * @return Return 1 if connected, 0 if mesh not responding after up to 1 second
   */
  
  bool checkConnection();

  bool connect(const char name[]=NULL);
  bool disconnect();

  inline int publish(const char topic[], const char payload[], bool retained=false) {return publish(topic, payload, strlen(payload), retained);}
  int publish(const char topic[], const void* payload, byte length, bool retained=false);


  bool subscribe(const char topic[], unsigned char qos = 0);
  bool unsubscribe(const char topic[]); 

  void setCallback(mqtt_callback_t _callback);
  void setStaticTopics(topics_t topics);

  private:
  void proccessMessage(RF24NetworkHeader& header, const void* payload, byte length);
  void dispatchMessage(RF24NetworkHeader& header);
  bool incomingMessage(RF24NetworkHeader& header);

  uint32_t ping_time;
  uint8_t ping_cnt;

  mqtt_callback_t callback;

  char **static_topics;
  const char *client_name;
  
  RF24& radio;
  RF24Network& network;
  RF24Mesh& mesh;
};
#endif
