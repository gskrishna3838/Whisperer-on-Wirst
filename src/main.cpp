#include <M5Unified.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

// --- Конфигурация ---
const char* OLLAMA_MODEL = "rnj-1:latest";
const int OLLAMA_PORT = 11434;
constexpr uint32_t AI_TIMEOUT = 180000UL;

// Размеры экрана M5StickC Plus2 в горизонтальной ориентации
const int SCREEN_WIDTH = 240;   // Ширина
const int SCREEN_HEIGHT = 135;  // Высота

// --- Состояние программы ---
enum AppState {
    STATE_IDLE,
    STATE_CONNECTING,
    STATE_READY,
    STATE_THINKING,
    STATE_RESPONSE,
    STATE_ERROR
};

AppState currentState = STATE_IDLE;
String lastResponse = "";
String lastError = "";
unsigned long stateStartTime = 0;
bool screenNeedsRedraw = true;

// === ПРОТОТИПЫ ФУНКЦИЙ ===
void connectToWiFi();
void sendToOllama(String prompt);
void drawScreen(bool forceRedraw = false);
void drawConnectingScreen(bool forceRedraw);
void drawReadyScreen(bool forceRedraw);
void drawThinkingScreen(bool forceRedraw);
void drawResponseScreen(bool forceRedraw);
void drawErrorScreen(bool forceRedraw);
void drawIdleScreen(bool forceRedraw);
void drawFullScreenText(const String& text, int y, int maxLines = 6);
void drawCenteredText(const String& text, int y, uint16_t color = TFT_WHITE);
void drawStatusBar();
void drawRussianText(const String& text, int startY, int maxLines);
// === КОНЕЦ ПРОТОТИПОВ ===

void setup() {
    auto cfg = M5.config();
    cfg.internal_mic = false;
    M5.begin(cfg);
    
    // Горизонтальная ориентация (0 = landscape, 1 = portrait)
    M5.Display.setRotation(0);  // Горизонтальный режим
    M5.Display.setBrightness(100);
    M5.Display.fillScreen(TFT_BLACK);
    
    // Тест ориентации (можно убрать после проверки)
    M5.Display.setCursor(10, 10);
    M5.Display.println("Screen: " + String(SCREEN_WIDTH) + "x" + String(SCREEN_HEIGHT));
    delay(1000);
    M5.Display.fillScreen(TFT_BLACK);
    
    Serial.begin(115200);
    Serial.println("\n=== Whisperer on the Wrist ===");
    Serial.println("AI Bridge - Horizontal Mode");
    
    currentState = STATE_CONNECTING;
    stateStartTime = millis();
    screenNeedsRedraw = true;
    
    // Начинаем подключение к WiFi
    connectToWiFi();
}

void loop() {
    M5.update();
    
    // Проверка таймаута
    if ((currentState == STATE_CONNECTING || currentState == STATE_THINKING) && 
        millis() - stateStartTime > AI_TIMEOUT) {
        lastError = "Timeout (" + String(AI_TIMEOUT/1000) + "s)";
        currentState = STATE_ERROR;
        stateStartTime = millis();
        screenNeedsRedraw = true;
    }
    
    // Обработка кнопок
    if (M5.BtnA.wasPressed()) {
        switch(currentState) {
            case STATE_READY:
                sendToOllama("Hi! what's up?");
                screenNeedsRedraw = true;
                break;
            case STATE_RESPONSE:
            case STATE_ERROR:
                currentState = STATE_READY;
                screenNeedsRedraw = true;
                break;
        }
    }
    
    if (M5.BtnB.wasPressed()) {
        switch(currentState) {
            case STATE_READY:
                M5.Display.fillScreen(TFT_BLACK);
                screenNeedsRedraw = true;
                break;
            case STATE_THINKING:
                // Можно добавить отмену запроса
                break;
        }
    }
    
    // Отрисовка экрана
    if (screenNeedsRedraw) {
        drawScreen(true);
        screenNeedsRedraw = false;
    }
    
    // Для анимации "думания" перерисовываем чаще
    if (currentState == STATE_THINKING && millis() % 200 < 50) {
        drawScreen(false); // Перерисовка только анимации
    }
    
    delay(50);
}

// --- Функция для отображения русского текста ---
void drawRussianText(const String& text, int startY, int maxLines) {
    M5.Display.setTextSize(1);
    M5.Display.setTextWrap(false);
    
    int lineHeight = 14;
    int lineWidth = SCREEN_WIDTH - 20;
    int currentY = startY;
    int linesDrawn = 0;
    
    String remainingText = text;
    remainingText.replace("\n", " "); // Заменяем переносы на пробелы
    
    // Локальная переменная (не static!)
    String currentLine = "";
    
    // Разбиваем на слова
    int startPos = 0;
    while (startPos < remainingText.length() && linesDrawn < maxLines) {
        // Находим следующее слово
        int spacePos = remainingText.indexOf(' ', startPos);
        if (spacePos == -1) spacePos = remainingText.length();
        
        String word = remainingText.substring(startPos, spacePos);
        
        // Проверяем, помещается ли слово в текущую строку
        if (M5.Display.textWidth(currentLine + " " + word) < lineWidth || currentLine.length() == 0) {
            if (currentLine.length() > 0) currentLine += " ";
            currentLine += word;
        } else {
            // Рисуем текущую строку
            if (currentY < SCREEN_HEIGHT - lineHeight) {
                M5.Display.setCursor(10, currentY);
                M5.Display.println(currentLine);
                currentY += lineHeight;
                linesDrawn++;
            }
            currentLine = word;
        }
        
        startPos = spacePos + 1;
    }
    
    // Рисуем последнюю строку
    if (currentLine.length() > 0 && linesDrawn < maxLines && currentY < SCREEN_HEIGHT - lineHeight) {
        M5.Display.setCursor(10, currentY);
        M5.Display.println(currentLine);
    }
}

// --- Функция для центрированного текста ---
void drawCenteredText(const String& text, int y, uint16_t color) {
    M5.Display.setTextColor(color);
    int textWidth = M5.Display.textWidth(text);
    int x = (SCREEN_WIDTH - textWidth) / 2;
    M5.Display.setCursor(max(10, x), y);
    M5.Display.println(text);
    M5.Display.setTextColor(TFT_WHITE);
}

// --- Статус-бар вверху ---
void drawStatusBar() {
    // Фон статус-бара
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, 20, TFT_DARKGREY);
    
    // Индикатор состояния WiFi
    if (WiFi.status() == WL_CONNECTED) {
        M5.Display.fillCircle(15, 10, 4, TFT_GREEN);
        M5.Display.setCursor(25, 6);
        M5.Display.setTextSize(1);
        M5.Display.print("WiFi");
    } else {
        M5.Display.fillCircle(15, 10, 4, TFT_RED);
        M5.Display.setCursor(25, 6);
        M5.Display.setTextSize(1);
        M5.Display.print("No WiFi");
    }
    
    // Время работы справа
    unsigned long uptime = millis() / 1000;
    M5.Display.setCursor(SCREEN_WIDTH - 60, 6);
    M5.Display.printf("%02lu:%02lu", uptime / 60, uptime % 60);
}

// --- Функции отрисовки экранов ---
void drawScreen(bool forceRedraw) {
    if (forceRedraw) {
        M5.Display.fillScreen(TFT_BLACK);
        drawStatusBar();
    }
    
    switch(currentState) {
        case STATE_CONNECTING:
            drawConnectingScreen(forceRedraw); break;
        case STATE_READY:
            drawReadyScreen(forceRedraw); break;
        case STATE_THINKING:
            drawThinkingScreen(forceRedraw); break;
        case STATE_RESPONSE:
            drawResponseScreen(forceRedraw); break;
        case STATE_ERROR:
            drawErrorScreen(forceRedraw); break;
        default:
            drawIdleScreen(forceRedraw);
    }
}

void drawConnectingScreen(bool forceRedraw) {
    if (forceRedraw) {
        drawCenteredText("CONNECTING", 45, TFT_YELLOW);
        
        M5.Display.setCursor(20, 65);
        M5.Display.println("WiFi Network:");
        M5.Display.setCursor(20, 80);
        M5.Display.setTextSize(2);
        M5.Display.println(WIFI_SSID);
        M5.Display.setTextSize(1);
    }
    
    // Анимация подключения
    int dots = (millis() / 500) % 4;
    M5.Display.fillRect(SCREEN_WIDTH/2 - 20, 100, 40, 20, TFT_BLACK);
    M5.Display.setCursor(SCREEN_WIDTH/2 - 20, 100);
    for (int i = 0; i < dots; i++) {
        M5.Display.print(".");
    }
}

void drawReadyScreen(bool forceRedraw) {
    if (forceRedraw) {
        drawCenteredText("READY", 40, TFT_GREEN);
        
        // Информация о подключении
        M5.Display.setCursor(20, 60);
        if (WiFi.status() == WL_CONNECTED) {
            M5.Display.print("IP: ");
            M5.Display.println(WiFi.localIP());
        }
        
        // Информация о сервере
        M5.Display.setCursor(20, 75);
        M5.Display.print("Ollama: ");
        M5.Display.println(OLLAMA_HOST);
        
        M5.Display.setCursor(20, 90);
        M5.Display.print("Model: ");
        M5.Display.println(OLLAMA_MODEL);
        
        // Инструкции
        M5.Display.setCursor(20, 110);
        M5.Display.setTextColor(TFT_CYAN);
        M5.Display.println("A: Ask AI a question");
        M5.Display.setCursor(20, 125);
        M5.Display.println("B: Clear screen");
        M5.Display.setTextColor(TFT_WHITE);
    }
}

void drawThinkingScreen(bool forceRedraw) {
    if (forceRedraw) {
        drawCenteredText("AI IS THINKING", 40, TFT_CYAN);
    }
    
    // Анимация прогресса
    unsigned long elapsed = millis() - stateStartTime;
    int progress = min(100, (int)((elapsed * 100) / (uint32_t)AI_TIMEOUT));
    
    // Прогресс-бар
    M5.Display.drawRect(20, 60, SCREEN_WIDTH - 40, 10, TFT_WHITE);
    M5.Display.fillRect(22, 62, (SCREEN_WIDTH - 44) * progress / 100, 6, TFT_CYAN);
    
    // Время
    M5.Display.fillRect(20, 75, 100, 15, TFT_BLACK);
    M5.Display.setCursor(20, 75);
    M5.Display.printf("Time: %lu/%lus", elapsed/1000, AI_TIMEOUT/1000);
    
    // Анимация вращения (только если не forceRedraw)
    if (!forceRedraw) {
        int frame = (millis() / 200) % 12;
        int centerX = SCREEN_WIDTH / 2;
        int centerY = 105;
        
        // Очищаем только область анимации
        M5.Display.fillRect(centerX - 25, centerY - 25, 50, 50, TFT_BLACK);
        
        // Рисуем вращающиеся точки
        for (int i = 0; i < 12; i++) {
            float angle = (frame * 30 + i * 30) * 3.14159 / 180.0;
            int x = centerX + cos(angle) * 20;
            int y = centerY + sin(angle) * 20;
            
            uint16_t color;
            if (i % 3 == 0) color = TFT_CYAN;
            else if (i % 3 == 1) color = TFT_MAGENTA;
            else color = TFT_YELLOW;
            
            M5.Display.fillCircle(x, y, 3, color);
        }
        
        // Центральный круг
        M5.Display.fillCircle(centerX, centerY, 8, TFT_WHITE);
    }
}

void drawResponseScreen(bool forceRedraw) {
    if (forceRedraw) {
        drawCenteredText("AI RESPONSE", 30, TFT_GREEN);
        
        // Разделительная линия
        M5.Display.drawFastHLine(10, 45, SCREEN_WIDTH - 20, TFT_DARKGREY);
        
        // Область для текста с рамкой
        M5.Display.drawRect(5, 50, SCREEN_WIDTH - 10, 75, TFT_DARKGREY);
    }
    
    // Отображаем текст ответа
    if (lastResponse.length() > 0) {
        M5.Display.fillRect(6, 51, SCREEN_WIDTH - 12, 73, TFT_BLACK);
        drawRussianText(lastResponse, 52, 5);
    }
    
    // Инструкции внизу
    if (forceRedraw) {
        M5.Display.fillRect(10, 125, SCREEN_WIDTH - 20, 20, TFT_BLACK);
        M5.Display.setCursor(20, 125);
        M5.Display.setTextColor(TFT_YELLOW);
        M5.Display.print("[A] Back");
        M5.Display.setCursor(SCREEN_WIDTH - 80, 125);
        M5.Display.setTextColor(TFT_CYAN);
        M5.Display.print("[B] Clear");
        M5.Display.setTextColor(TFT_WHITE);
    }
}

void drawErrorScreen(bool forceRedraw) {
    if (forceRedraw) {
        drawCenteredText("ERROR", 40, TFT_RED);
        
        // Иконка ошибки
        int centerX = SCREEN_WIDTH / 2;
        M5.Display.fillCircle(centerX, 75, 15, TFT_RED);
        M5.Display.fillCircle(centerX, 75, 10, TFT_BLACK);
        M5.Display.fillRect(centerX - 3, 65, 6, 20, TFT_WHITE);
    }
    
    // Текст ошибки
    if (lastError.length() > 0) {
        M5.Display.fillRect(20, 95, SCREEN_WIDTH - 40, 30, TFT_BLACK);
        M5.Display.setCursor(20, 95);
        drawRussianText(lastError, 95, 2);
    }
    
    // Инструкции
    if (forceRedraw) {
        M5.Display.setCursor(20, 120);
        M5.Display.setTextColor(TFT_YELLOW);
        M5.Display.println("Press A to retry");
        M5.Display.setTextColor(TFT_WHITE);
    }
}

void drawIdleScreen(bool forceRedraw) {
    if (forceRedraw) {
        drawCenteredText("WOW AI", 40, TFT_MAGENTA);
        
        M5.Display.setCursor(30, 65);
        M5.Display.println("Whisperer on");
        M5.Display.setCursor(30, 80);
        M5.Display.println("the Wrist");
        
        M5.Display.setCursor(30, 100);
        M5.Display.println("AI Bridge v2.0");
        
        M5.Display.setCursor(30, 120);
        M5.Display.println("Press A to start");
    }
}

// --- WiFi подключение ---
void connectToWiFi() {
    Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
    
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        
        currentState = STATE_READY;
        lastError = "";
    } else {
        lastError = "WiFi connection failed";
        currentState = STATE_ERROR;
        Serial.println(lastError);
    }
    stateStartTime = millis();
    screenNeedsRedraw = true;
}

// --- Отправка запроса к Ollama ---
void sendToOllama(String prompt) {
    if (WiFi.status() != WL_CONNECTED) {
        lastError = "WiFi not connected";
        currentState = STATE_ERROR;
        screenNeedsRedraw = true;
        return;
    }
    
    currentState = STATE_THINKING;
    stateStartTime = millis();
    lastResponse = "";
    lastError = "";
    screenNeedsRedraw = true;
    
    HTTPClient http;
    
    String url = "http://" + String(OLLAMA_HOST) + ":" + String(OLLAMA_PORT) + "/api/generate";
    Serial.println("Sending request to: " + url);
    Serial.println("Prompt: " + prompt);
    
    StaticJsonDocument<200> requestDoc;
    requestDoc["model"] = OLLAMA_MODEL;
    requestDoc["prompt"] = prompt;
    requestDoc["stream"] = false;
    
    String requestBody;
    serializeJson(requestDoc, requestBody);
    
    http.setTimeout(AI_TIMEOUT);
    http.setConnectTimeout(15000);
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST(requestBody);
    
    if (httpCode > 0) {
        Serial.printf("HTTP code: %d\n", httpCode);
        
        if (httpCode == HTTP_CODE_OK) {
            String response = http.getString();
            Serial.println("Response length: " + String(response.length()) + " chars");
            
            // Отладка русского текста
            Serial.println("=== RAW RESPONSE (first 200 chars) ===");
            Serial.println(response.substring(0, min(200, (int)response.length())));
            Serial.println("=== END RESPONSE ===");
            
            StaticJsonDocument<4096> responseDoc; // 4KB для ответов
            DeserializationError error = deserializeJson(responseDoc, response);
            
            if (!error && responseDoc.containsKey("response")) {
                lastResponse = responseDoc["response"].as<String>();
                Serial.println("Success! Response saved.");
                currentState = STATE_RESPONSE;
            } else {
                lastError = error ? String("JSON error: ") + error.c_str() : "No response field";
                Serial.println("Error: " + lastError);
                currentState = STATE_ERROR;
            }
        } else {
            lastError = "HTTP " + String(httpCode);
            Serial.println("Error: " + lastError);
            currentState = STATE_ERROR;
        }
    } else {
        lastError = "Connection failed: " + String(http.errorToString(httpCode));
        Serial.println("Error: " + lastError);
        currentState = STATE_ERROR;
    }
    
    http.end();
    screenNeedsRedraw = true;
}