#pragma once

#include "JSON_Parser.h"
#include <string>

class ConfigBuilder {
    const char *TAG = "ConfigBuilder OTA_Firmware";
    
public:
    ConfigBuilder() {}

    static JSONParser parser;

    struct ProjectConfig {
        static const char* getProjectName() {
            return ConfigBuilder::parser.getProjectConfig()->getAttr("name");
        }
        static const char* getBuildDir() {
            return ConfigBuilder::parser.getProjectConfig()->getAttr("build_dir");
        }
    };

    struct MQTT {
        static const char* getBrokerURL() {
            return ConfigBuilder::parser.getMQTTConfig()->getAttr("broker");
        }
        static const char* getTopic() {
            return ConfigBuilder::parser.getMQTTConfig()->getAttr("topic");
        }
        static int getPort() {
            return ConfigBuilder::parser.getMQTTConfig()->getIntAttr("port");
        }
    };
    
    struct RouterConfig {
        static const char* getSSID() {
            return ConfigBuilder::parser.getRouterConfig()->getAttr("ssid");
        }
        static const char* getPassword() {
            return ConfigBuilder::parser.getRouterConfig()->getAttr("password");
        }
    };

    struct ServerConfig {
        static const char* getExternalIP() {
            return ConfigBuilder::parser.getServerConfig()->getAttr("external_ip");
        }
        static const char* getInternalIP() {
            return ConfigBuilder::parser.getServerConfig()->getAttr("ip");
        }
        static int getPort() {
            return ConfigBuilder::parser.getServerConfig()->getIntAttr("port");
        }

    };
};

JSONParser ConfigBuilder::parser;