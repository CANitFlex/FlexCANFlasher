#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#include "cJSON.h"
#include <string>

class JSON_Config_Object{
public:
    JSON_Config_Object() = default;

    bool initialize(cJSON *json, const char *name) {
        obj = cJSON_GetObjectItem(json, name);
        if (!obj) {
            ESP_LOGE("JSON_Config_Object", "No '%s' object in config.json", name);
            cJSON_Delete(json);
            return false;
        }
        return true;
    }

    const char* getAttr(const char* attr) {
        cJSON *attr_item = cJSON_GetObjectItem(obj, attr);
        if (!cJSON_IsString(attr_item)) {
            ESP_LOGE("JSON_Config_Object", "Invalid '%s' in config.json", attr);
            return nullptr;
        }
        return attr_item->valuestring;
    }

    int getIntAttr(const char* attr) {
        cJSON *attr_item = cJSON_GetObjectItem(obj, attr);
        if (!cJSON_IsNumber(attr_item)) {
            ESP_LOGE("JSON_Config_Object", "Invalid numeric '%s' in config.json", attr);
            return -1;
        }
        return (int)attr_item->valuedouble;
    }

private:
    cJSON *obj;
};

extern const char config_json_start[] asm("_binary_config_json_start");
extern const char config_json_end[] asm("_binary_config_json_end");

class JSONParser {

    const char *TAG = "JSONPARSER OTA_Firmware";
public:
    JSONParser() {
        size_t config_size = config_json_end - config_json_start;
        char *config_content = (char *)malloc(config_size + 1);
        if (!config_content) {
            ESP_LOGE(TAG, "Failed to allocate memory for config.json");
            return;
        }
        memcpy(config_content, config_json_start, config_size);
        config_content[config_size] = '\0';

        this->json = cJSON_Parse(config_content);
        if (!json) {
            ESP_LOGE(TAG, "Failed to parse config.json");
            free(config_content);
            return;
        }
        free (config_content);
        bool projectInitialized = projectObj.initialize(json, "project");
        bool routerInitialized = routerObj.initialize(json, "router");
        bool serverInitialized = serverObj.initialize(json, "server");
        bool mqttInitialized = mqttObj.initialize(json, "mqtt");
        if (!projectInitialized || 
            !routerInitialized || 
            !serverInitialized || 
            !mqttInitialized) {
            ESP_LOGE(TAG, "Failed to initialize JSON_Config_Objects");
            cJSON_Delete(json);
            json = nullptr;
        }
    }

    JSON_Config_Object* getProjectConfig() {
        return &projectObj;
    }

    JSON_Config_Object* getRouterConfig() {
        return &routerObj;
    }
    JSON_Config_Object* getServerConfig() {
        return &serverObj;
    }
    JSON_Config_Object* getMQTTConfig() {
        return &mqttObj;
    }


    JSON_Config_Object* getJSONConfigObject(const char* name) {
        if (name == nullptr || strlen(name) == 0) {
            return nullptr;
        }
        if (strcmp(name, "project") == 0) {
            return &projectObj;
        } 
        if (strcmp(name, "router") == 0) {
            return &routerObj;
        } 
        if (strcmp(name, "server") == 0) {
            return &serverObj;
        } 
        if (strcmp(name, "mqtt") == 0) {
            return &mqttObj;
        }
        ESP_LOGE(TAG, "Unknown JSON config object: %s", name);
        return nullptr;
    }

private:
    char *read_file(const char *path) {
        struct stat st;
        if (stat(path, &st) != 0) {
            ESP_LOGE(TAG, "Failed to stat file: %s", path);
            return NULL;
        }

        FILE *file = fopen(path, "r");
        if (!file) {
            ESP_LOGE(TAG, "Failed to open file: %s", path);
            return NULL;
        }

        char *content = (char *)malloc(st.st_size + 1);
        if (!content) {
            ESP_LOGE(TAG, "Failed to allocate memory for file content");
            fclose(file);
            return NULL;
        }

        fread(content, 1, st.st_size, file);
        content[st.st_size] = '\0';
        fclose(file);
        return content;
    }

    cJSON *json = nullptr;
    std::string configPath;
    JSON_Config_Object projectObj;
    JSON_Config_Object routerObj;
    JSON_Config_Object serverObj;
    JSON_Config_Object mqttObj;
};

#endif