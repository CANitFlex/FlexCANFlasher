#!/bin/bash

# Setup and Run Script for FlexFirmwareServer
# This script creates a venv, installs requirements, and starts the server

cd ..


set -e

CONFIG_FILE="config.json"

echo "======================================"
echo "Setup Python Environment for FlexFirmwareServer"
echo "======================================"

# Read platform from config.json
if [ -f "$CONFIG_FILE" ]; then
    PLATFORM=$(python3 -c "import json; data=json.load(open('$CONFIG_FILE')); print(data.get('project', {}).get('platform', 'linux').lower())" 2>/dev/null)
    if [ -z "$PLATFORM" ]; then
        PLATFORM="linux"
    fi
else
    echo "Warning: $CONFIG_FILE not found. Using default platform 'linux'"
    PLATFORM="linux"
fi

echo "Using platform: $PLATFORM"

# Set venv name based on platform
VENV_DIR="venv_${PLATFORM}"

# Create virtual environment if it doesn't exist
if [ ! -d "$VENV_DIR" ]; then
    echo ""
    echo "Creating virtual environment: $VENV_DIR"
    python3 -m venv "$VENV_DIR"
else
    echo ""
    echo "Virtual environment $VENV_DIR already exists"
fi

# Activate virtual environment
echo "Activating virtual environment..."
source "$VENV_DIR/bin/activate"

# Upgrade pip
echo ""
echo "Upgrading pip..."
pip install --upgrade pip

# Install requirements
echo ""
echo "Installing requirements from requirements.txt..."
pip install -r requirements.txt --upgrade-strategy only-if-needed
