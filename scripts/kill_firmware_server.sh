#!/bin/bash

# Path to the config file
CONFIG_FILE="../config.json"

# Check if the config file exists
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Config file not found: $CONFIG_FILE"
    exit 1
fi

# Extract the PID from the config file
PID=$(jq -r '.server.pid' "$CONFIG_FILE")

# Check if the PID is valid
if [ -z "$PID" ] || [ "$PID" == "null" ]; then
    echo "No valid PID found in config file."
    exit 1
fi

# Kill the process with the extracted PID
if kill -0 "$PID" 2>/dev/null; then
    kill "$PID"
    echo "Process with PID $PID has been terminated."
else
    echo "No process found with PID $PID."
fi