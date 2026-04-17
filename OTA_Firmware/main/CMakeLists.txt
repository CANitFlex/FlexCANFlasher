# Add include paths for FreeRTOS and protocol examples
idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES ${project_dir}/../certs/server_cert.pem
                                   ${project_dir}/../certs/server_key.pem
                                   ${project_dir}/../config.json)
