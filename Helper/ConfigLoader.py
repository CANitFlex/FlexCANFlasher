#!/usr/bin/env python3
"""
Simple Settings Helper
Stores configuration in JSON format
"""

import json
import os
from pathlib import Path


class ConfigLoader:
    def __init__(self, config_file: str = "config.json"):
        """Initialize settings helper with config file path"""
        self.config_file = config_file
        self.settings = self._load()

    def _load(self) -> dict:
        """Load settings from JSON file"""
        if os.path.exists(self.config_file):
            try:
                with open(self.config_file, 'r') as f:
                    return json.load(f)
            except (json.JSONDecodeError, IOError):
                return {}
        return {}

    def get(self, key: str, default=None):
        """Get a setting value, supporting nested keys like 'server.ip'"""
        keys = key.split('.')
        value = self.settings
        for k in keys:
            if isinstance(value, dict) and k in value:
                value = value[k]
            else:
                return default
        return value

    def save(self) -> None:
        """Save settings to JSON file"""
        try:
            with open(self.config_file, 'w') as f:
                json.dump(self.settings, f, indent=4)
        except IOError as e:
            print(f"Error saving config file: {e}")

    def set(self, key: str, value) -> None:
        """Set a setting value, supporting nested keys like 'server.ip', and save to file"""
        keys = key.split('.')
        d = self.settings
        for k in keys[:-1]:
            if k not in d or not isinstance(d[k], dict):
                d[k] = {}
            d = d[k]
        d[keys[-1]] = value
        self.save()

    def all(self) -> dict:
        """Get all settings"""
        return self.settings
