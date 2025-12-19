// lib/config.h
#ifndef CONFIG_H
#define CONFIG_H

// Настройки сети
const char* WIFI_SSID = "YOUR_WIFI_SSID";      // Замените в secrets.h
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";  // Замените в secrets.h
const char* OLLAMA_HOST = "192.168.1.7";     // IP вашего ПК с Ollama
const int OLLAMA_PORT = 11434;

// Настройки модели Ollama
const char* OLLAMA_MODEL = "rnj-1:latest";         // Имя вашей модели
const int MAX_RESPONSE_LENGTH = 500;           // Макс. длина ответа

// Настройки интерфейса
const unsigned long AI_THINKING_TIMEOUT = 240000; // 30 сек таймаут
const unsigned long DEBOUNCE_DELAY = 250;        // Задержка антидребезга
#endif