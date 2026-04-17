idf_component_register(SRCS "main.cpp" 
                       INCLUDE_DIRS "." "../../OTA_Firmware/main"
                       EMBED_TXTFILES "../../certs/server_cert.pem"
                                      "../../certs/server_key.pem"
                                      "../../config.json"
                       )

# Link Unity framework
set(COMPONENT_REQUIRES unity)