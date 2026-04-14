# SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import http.server
import os
import ssl
import sys
from typing import Optional

from Helper.ServerConfig import ServerConfig

config = ServerConfig()
# From config.json
# Simply set these variables here or even better in the config.json file
PLATTFORM = config.platform
IDF_PATH = config.idf_path
BUILD_DIR = config.build_dir
HTTPS_PORT = config.https_port


def start_https_server(config: ServerConfig) -> None:

    httpd = http.server.HTTPServer((config.server_ip, config.https_port), http.server.SimpleHTTPRequestHandler)

    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    server_file = os.path.join(config.cert_dir, "server_cert.pem")
    key_file = os.path.join(config.cert_dir, "server_key.pem")
    ssl_context.load_cert_chain(certfile=server_file, keyfile=key_file)

    httpd.socket = ssl_context.wrap_socket(httpd.socket, server_side=True)
    save_server_pid(config)
    httpd.serve_forever()

def save_server_pid(config: ServerConfig) -> None:
    """Speichert die PID des Servers in einer Datei."""
    pid = os.getpid()
    config.set("server.pid", pid)

if __name__ == '__main__':
    if not all([PLATTFORM, IDF_PATH, BUILD_DIR, HTTPS_PORT]):
        print("Fehlende Konfiguration. Bitte stellen Sie sicher, dass 'plattform', 'idf_path', 'build_dir' und 'port' in der config.json vorhanden sind.")
        sys.exit(1)

    this_dir = os.path.dirname(os.path.realpath(__file__))
    bin_dir = os.path.join(this_dir, BUILD_DIR)
    cert_dir = os.path.join(this_dir, "certs") 
    print(f'Starting HTTPS server at "https://0.0.0.0:{HTTPS_PORT}"')
    start_https_server(config)
