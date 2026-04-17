import os
import time
import logging
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler
import paho.mqtt.client as mqtt
from Helper.MQTTConfig import MQTTConfig
from Helper.ProjectConfig import ProjectConfig

# Logging einrichten
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

# MQTT-Konfiguration
mqtt_config = MQTTConfig()
project_config = ProjectConfig()
# From config.json
# Simply set these variables here or even better in the config.json file
MQTT_BROKER = mqtt_config.mqtt_broker
MQTT_PORT = mqtt_config.mqtt_port
MQTT_TOPIC = mqtt_config.mqtt_topic
FIRMWARE_BUILD_PATH = project_config.build_dir

# Build-Pfad
project_config = ProjectConfig()
BUILD_PATH = os.path.join(os.path.dirname(os.path.realpath(__file__)), project_config.build_dir)

class BuildEventHandler(FileSystemEventHandler):
    def __init__(self, mqtt_client):
        self.mqtt_client = mqtt_client
        self.last_timestamps = {}

    def check_and_process_bin_file(self, file_path):
        """Überprüft und verarbeitet eine .bin-Datei."""
        file_name = os.path.basename(file_path)
        try:
            if os.path.exists(file_path):
                file_timestamp = os.path.getmtime(file_path)
                logging.debug(f"Datei gefunden: {file_path}, Zeitstempel: {file_timestamp}")

                # Überprüfen, ob die Datei neu ist
                if file_name not in self.last_timestamps or self.last_timestamps[file_name] < file_timestamp:
                    self.last_timestamps[file_name] = file_timestamp
                    logging.info(f"Neue .bin-Datei gefunden: {file_path}")
                    self.mqtt_client.publish(MQTT_TOPIC, f"{FIRMWARE_BUILD_PATH}/{file_name}")
                    logging.info(f"MQTT-Nachricht gesendet: {FIRMWARE_BUILD_PATH}/{file_name}")
            else:
                logging.debug(f"Datei existiert nicht: {file_path}")
        except Exception as e:
            logging.error(f"Fehler beim Verarbeiten der Datei {file_path}: {e}")

    def on_modified(self, event):
        logging.debug(f"on_modified ausgelöst: {event.src_path}")
        if not event.is_directory and event.src_path.endswith(".bin"):
            self.check_and_process_bin_file(event.src_path)

def main():
    # MQTT-Client einrichten
    mqtt_client = mqtt.Client()
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        logging.info(f"Verbunden mit MQTT-Broker {MQTT_BROKER}:{MQTT_PORT}")
    except Exception as e:
        logging.error(f"Fehler beim Verbinden mit dem MQTT-Broker: {e}")
        return

    # Watchdog-Observer einrichten
    event_handler = BuildEventHandler(mqtt_client)
    observer = Observer()
    try:
        observer.schedule(event_handler, BUILD_PATH, recursive=False)
        observer.start()
        logging.info(f"Überwache Build-Pfad: {BUILD_PATH}")
    except Exception as e:
        logging.error(f"Fehler beim Einrichten des Observers: {e}")
        return

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        logging.info("Beende Überwachung...")
        observer.stop()
    observer.join()

if __name__ == "__main__":
    main()