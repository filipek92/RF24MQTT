#!/usr/bin/env python3

from RF24 import *
from RF24Network import *
from RF24Mesh import *

from struct import pack, unpack

from paho.mqtt.client import Client as MQTT_Client

from queue import Queue

import json
import click
import os
import sys
import re
import time

from collections import namedtuple

Subscription = namedtuple('Subscription', ['topic', 'client', 'qos']);

RETRIES = 5

class MQTT_Subscripctions(MQTT_Client):
    def __init__(self):
        MQTT_Client.__init__(self)
        self._subscriptions = set()
        self.clients = {}
        self.verbose = 0

    def on_connect(self, client, userdata, flags, rc):
        print("Connected to MQTT Broker")
        self.publishJSON('gateway/subscriptions', self._subscriptions)
        self.publishJSON('gateway/clients', self.clients)

    def clientConnected(self, name, id):
        self.clients[id] = {"name": name, "last_comm": time.time()}

        for s in [s for s in self._subscriptions if s.client == id]:
            self._subscriptions.remove(s)

        self.publishJSON('gateway/clients', self.clients)
        self.publishJSON('gateway/subscriptions', self._subscriptions)

    def clientDisconnected(self, id):
        del self.clients[id]
        for s in [s for s in self._subscriptions if s.client == id]:
            self._subscriptions.remove(s)

        self.publishJSON('gateway/clients', self.clients)
        self.publishJSON('gateway/subscriptions', self._subscriptions)

    def subscribe(self, topic, client,  qos=0):
        self._subscriptions.add(Subscription(topic=topic, client=client, qos=qos))
        MQTT_Client.subscribe(self, topic, qos=qos)
        self.publishJSON('gateway/subscriptions', self._subscriptions)

    def publishJSON(self, topic, payload=None, qos=0, retain=False):
        if type(payload) is set:
            payload = tuple(payload)
        self.publish(topic, payload=json.dumps(payload), qos=qos, retain=retain)

    def subscribed_clients(self, topic):
        return (s.client for s in self._subscriptions if MQTT_Subscripctions.match(topic, s.topic))

    def ping(self, id):
        self.clients[id]["last_comm"] = time.time()

    @staticmethod
    def match(topic, mask):
        mask = mask.replace("+", "([^/]*)")
        if(mask[-1] == '#'):
            mask = mask[0:-1]+"(.*)"
        return bool(re.match(mask, topic))

mqtt = MQTT_Subscripctions()
tx_queue = Queue()

def mqtt_message(client, userdata, msg):
    payload = msg.topic.encode() + b'\x00'+msg.payload
    if client.verbose > 2:
        print('Distributing message {payload} of "{topic}"'.format(payload=msg.payload, topic=msg.topic))
    for client in mqtt.subscribed_clients(msg.topic):
        tx_queue.put((client, payload, RETRIES))

@click.command()
@click.option('-h', '--host', default='localhost')
@click.option('-p', '--port', default=1883)
@click.option('-c', '--channel', default=0)
@click.option('-s', '--statistics', default=True)
@click.option('-r', '--retain/--no-retain', default=False)
@click.option('-v', '--verbose', default=0, count=True)
def main(host, port, channel, statistics, retain, verbose):
    print('RF24MQTT Gateway')
    if not os.geteuid() == 0:
        sys.exit('Script must be run as root, use sudo')
    
    radio = RF24(RPI_V2_GPIO_P1_15, RPI_V2_GPIO_P1_24, BCM2835_SPI_SPEED_8MHZ)
    network = RF24Network(radio)
    mesh = RF24Mesh(radio, network)

    mqtt.verbose = verbose

    mqtt.on_message = mqtt_message
    mqtt.loop_start()
    mqtt.connect(host, port=port)

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

            _type = chr(header.type)
            if _type == 'P':
                try:
                    topic, data = payload.split(b'\x00', 1)
                    topic = topic.decode()
                    mqtt.publish(topic, data, retain=retain)
                    if verbose > 1:
                        print("Publishing to {}: {}".format(topic,data))
                except:
                    print("Publishing error")

            elif _type == 'p':
                nodeID = mesh.getNodeID(header.from_node)
                mesh.write(payload, ord('r'), nodeID)
                if verbose > 3:
                    print("Reply for ping from", nodeID)
            elif _type == "S":
                topic = payload.decode()
                nodeID = mesh.getNodeID(header.from_node)
                mqtt.subscribe(topic, nodeID)
                if verbose > 0:
                    print('Subscribing "{name}" for {ask}'.format(name=topic, ask=nodeID))
            elif _type == "C":
                name = payload.decode()
                nodeID = mesh.getNodeID(header.from_node)
                mqtt.clientConnected(name, nodeID)
                if verbose > 0:
                    print('Client "{name}" connected with node ID {node}'.format(node=nodeID, name=name))
            else:
                print('''Received unknown message "{}" (type {}) from node {} (0{:o})'''.format(payload, _type, mesh.getNodeID(header.from_node), header.from_node))
        
        cnt = 10
        while not tx_queue.empty() and cnt:
            client, payload, retries = tx_queue.get()
            status = mesh.write(payload, ord('R'), client)
            if verbose > 1:
                print("  Transmission to {}: {}".format(client, "success" if status else "failed"))
            if not status and retries:
                tx_queue.put((client, payload, retries-1))
            cnt = cnt - 1
            
if __name__ == '__main__':
    main()