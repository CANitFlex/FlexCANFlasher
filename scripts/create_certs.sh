#!/bin/bash

# Script to create server certificates in the directory specified in config.json

CONFIG_FILE="../config.json"
ROOT_DIR="$(dirname $(dirname $(realpath $0)))"

# Load CERT_DIR and external IP from config.json
CERT_DIR=$(python3 -c "import json; print(json.load(open('$CONFIG_FILE')).get('server', {}).get('cert_dir', 'certs'))")
EXTERNAL_IP=$(python3 -c "import json; print(json.load(open('$CONFIG_FILE')).get('server', {}).get('external_ip', '127.0.0.1'))")
CERT_DIR="$ROOT_DIR/$CERT_DIR"

# Ensure the directory exists
mkdir -p "$CERT_DIR"

# Check if certificates already exist
if [ ! -f "$CERT_DIR/server_cert.pem" ] || [ ! -f "$CERT_DIR/server_key.pem" ]; then
    echo "Creating server certificates in $CERT_DIR with CN=$EXTERNAL_IP..."
    openssl req -x509 -newkey rsa:4096 -keyout "$CERT_DIR/server_key.pem" -out "$CERT_DIR/server_cert.pem" -days 365 -nodes \
        -subj "/CN=$EXTERNAL_IP" \
        -addext "subjectAltName=IP:$EXTERNAL_IP,IP:127.0.0.1,DNS:localhost"
    echo "Server certificates created in $CERT_DIR: server_cert.pem and server_key.pem"
else
    echo "Server certificates already exist in $CERT_DIR: server_cert.pem and server_key.pem"
fi