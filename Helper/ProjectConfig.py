from Helper.ConfigLoader import ConfigLoader

class ProjectConfig:
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
    
