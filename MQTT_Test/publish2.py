import paho.mqtt.client as mqtt
from random import randrange
import time

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to broker")
    else:
        print(f"Connection failed with code {rc}")

def on_publish(client, userdata, mid):
    print(f"Message {mid} published")

mqttBroker = "192.168.43.16"
client = mqtt.Client("Temperature_Outside")
client.on_connect = on_connect
client.on_publish = on_publish

try:
    print(f"Attempting to connect to {mqttBroker}")
    client.connect(mqttBroker, port=1883, keepalive=60)
    client.loop_start()

    while True:
        randNumber = randrange(10)
        result = client.publish("TEMPERATURE", randNumber)
        result.wait_for_publish()
        print(f"Just published {randNumber} to Topic TEMPERATURE")
        time.sleep(1)

except KeyboardInterrupt:
    print("Stopping...")
    client.loop_stop()
    client.disconnect()
except Exception as e:
    print(f"Failed to connect: {e}")