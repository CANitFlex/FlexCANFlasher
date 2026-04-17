from Helper.ConfigLoader import ConfigLoader

class MQTTConfig:
    def __init__(self, config_file: str = "config.json"):
        self.config = ConfigLoader(config_file)

    @property
    def mqtt_broker(self) -> str:
        return self.config.get("mqtt.broker", "localhost")
    
    @property
    def mqtt_port(self) -> int:
        return int(self.config.get("mqtt.port", 1883))
    
    @property
    def mqtt_topic(self) -> str:
        return self.config.get("mqtt.topic", "build/notifications")
    
    def mqtt_message_template(self) -> dict:
        return self.config.get("mqtt.message", {
            "command": "update",
            "version": "{version}",
            "device": "{device}",
            "can_id": "{can_id}"
        })