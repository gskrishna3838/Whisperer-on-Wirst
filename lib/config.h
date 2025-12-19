#ifndef CONFIG_H
#define CONFIG_H

const char* WIFI_SSID = "YOUR_WIFI_SSID";     
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";  
const char* OLLAMA_HOST = "192.168.1.7";     
const int OLLAMA_PORT = 11434;

const char* OLLAMA_MODEL = "rnj-1:latest";      
const int MAX_RESPONSE_LENGTH = 500;         

const unsigned long AI_THINKING_TIMEOUT = 240000;
const unsigned long DEBOUNCE_DELAY = 250;
#endif