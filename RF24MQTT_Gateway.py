#!/usr/bin/env python3
from RF24 import *
from RF24Network import *
from RF24Mesh import *

from struct import pack, unpack

from paho.mqtt.client import Client

from queue import Queue

import json
import click
import os
import sys

RETRIES = 5

topic_table = [];
mqtt = Client()
subscribe_table = {}

tx_queue = Queue()

def mqtt_connect(client, userdata, flags, rc):
    print("Connection returned result: "+str(rc))

def mqtt_message(client, userdata, msg):
    clients = subscribe_table[msg.topic]["clients"]
    payload = msg.topic.encode() + b'\x00'+msg.payload;
    print('Distributing message {payload} of "{topic}" to {clients} '.format(payload=msg.payload, topic=msg.topic, clients=clients))
    for client in clients:
        tx_queue.put((client, payload, RETRIES));

def get_topic_id(topic_name):
    topic_id = -1;
    try:
        topic_id = topic_table.index(topic_name);
    except ValueError:
        topic_table.append(topic_name);
        topic_id = topic_table.index(topic_name);
    mqtt.publish('gateway/topics', json.dumps(topic_table));
    return topic_id

@click.command()
@click.option('-h', '--host', default='localhost')
@click.option('-p', '--port', default=1883)
@click.option('-c', '--channel', default=0)
@click.option('-s', '--statistics', default=True)
@click.option('-r', '--retain/--no-retain', default=False)
@click.option('-v', '--verbose', default=0)
def main(host, port, channel, statistics, retain, verbose):
    print('RF24MQTT Gateway');
    if not os.geteuid() == 0:
        sys.exit('Script must be run as root, use sudo')
    
    radio = RF24(RPI_V2_GPIO_P1_15, RPI_V2_GPIO_P1_24, BCM2835_SPI_SPEED_8MHZ)
    network = RF24Network(radio)
    mesh = RF24Mesh(radio, network)

    mqtt.on_connect = mqtt_connect
    mqtt.on_message = mqtt_message
    mqtt.loop_start()
    mqtt.connect(host, port=port)
    mqtt.publish('gateway/topics', json.dumps(topic_table), retain=retain);

    mesh.setNodeID(0)
    mesh.begin()
    radio.setPALevel(RF24_PA_MAX) # Power Amplifier
    if verbose:
        radio.printDetails()

    while 1:
        mesh.update()
        mesh.DHCP()

        while network.available():
            header, payload = network.read(20)

            _type = chr(header.type);
            if _type == 'P':
                try:
                    topic, data = payload.split(b'\x00', 1)
                    topic = topic.decode();
                    mqtt.publish(topic, data, retain=retain)
                    print("Publishing to {}: {}".format(topic,data))
                except:
                    print("Publishing error")

            elif _type == "S":
                topic = payload.decode()
                nodeID = mesh.getNodeID(header.from_node)
                subscribe_table[topic] = {"clients": [nodeID]}
                mqtt.subscribe(topic)
                mqtt.publish('gateway/subscribes', json.dumps(subscribe_table))
                print('Subscribing "{name}" for {ask}'.format(name=topic, ask=nodeID))
            elif verbose > 0:
                print('''Received message "{}" (type {}) from node {} (0{:o})'''.format(payload, _type, mesh.getNodeID(header.from_node), header.from_node))
        
        cnt = 10;
        while not tx_queue.empty() and cnt:
            client, payload, retries = tx_queue.get()
            status = mesh.write(payload, ord('R'), client)
            print("  Transmission to {}: {}".format(client, "success" if status else "failed"))
            if not status and retries:
                tx_queue.put((client, payload, retries-1));
            cnt = cnt - 1;
            
if __name__ == '__main__':
    main()