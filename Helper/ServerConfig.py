from Helper.ConfigLoader import ConfigLoader

class ServerConfig:
    def __init__(self, config_file: str = "config.json"):
        self.config = ConfigLoader(config_file)

    @property
    def platform(self) -> str:
        return self.config.get("project.platform")

    @property
    def idf_path(self) -> str:
        return self.config.get("project.idf_path")

    @property
    def build_dir(self) -> str:
        return self.config.get("project.build_dir")

    @property
    def https_port(self) -> int:
        return int(self.config.get("server.port", 8080))

    @property
    def server_ip(self) -> str:
        return self.config.get("server.ip", "0.0.0.0")
    
    @property
    def cert_dir(self) -> str:
        return self.config.get("server.cert_dir", "server_certs")
    
    @property
    def https_server_pid(self) -> str:
        return self.config.get("server.pid", "")
    
    def set(self, key: str, value) -> None:
        self.config.set(key, value)