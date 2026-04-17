#!/bin/bash

cd OTA_Firmware_tests

source ~/esp/v5.4.1/esp-idf/export.sh

idf.py build && idf.py flash -p /dev/ttyUSB0 

mkdir -p logs

idf.py monitor | tee "logs/test_results_$(date +'%Y-%m-%d_%H-%M-%S').log"