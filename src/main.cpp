#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <AHT10.h>
#include <Wire.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Definir pines para I2C y FOG
#define SDA_PIN D2          // GPIO4
#define SCL_PIN D1          // GPIO5
#define FOG_PULSE_PIN D5    // GPIO14
#define FOG_PULSE_DURATION 100  // ms
#define FOG_PULSE_DELAY 200     // ms

// Configuración WiFi
const char* ssid = "Vasval";
const char* password = "1234ABCD";
const char* hostname = "monotub";

// Crear servidor web
ESP8266WebServer server(80);

// Sensor AHT10
#define AHT10_ADDRESS 0x38
AHT10 myAHT10(AHT10_ADDRESS);
float lastTemp = 0;
float lastHumidity = 0;

// Variables de control
float humiditySetpoint = 90.0;
bool fogPulseState = false;

// Estructuras y constantes para el registro
struct SensorData {
    time_t timestamp;
    float temp;
    float humidity;
};

#define MAX_DAILY_RECORDS 288    // 24h * 12 (cada 5 min)
#define MAX_WEEKLY_RECORDS 2016  // 7 días * 288
#define MAX_MONTHLY_RECORDS 8640 // 30 días * 288

// Variables para NTP y registro
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
unsigned long lastRecord = 0;
unsigned long recordInterval = 300000; // 5 minutos por defecto

// Agregar definiciones
#define FAN_PIN D6          // GPIO12
#define FAN_DEFAULT_ON 300000    // 5 minutos en ms
#define FAN_DEFAULT_INTERVAL 14400000  // 4 horas en ms

// Variables globales para el ventilador
unsigned long fanOnDuration = FAN_DEFAULT_ON;
unsigned long fanInterval = FAN_DEFAULT_INTERVAL;
bool fanManualMode = false;
bool fanIsOn = false;
unsigned long lastFanStart = 0;

// Función para generar los pulsos del nebulizador
void generateFogPulses(bool isOn) {
    if (isOn) {
        // Single pulse for ON
        digitalWrite(FOG_PULSE_PIN, LOW);
        delay(FOG_PULSE_DURATION);
        digitalWrite(FOG_PULSE_PIN, HIGH);
    } else {
        // Double pulse for OFF
        digitalWrite(FOG_PULSE_PIN, LOW);
        delay(FOG_PULSE_DURATION);
        digitalWrite(FOG_PULSE_PIN, HIGH);
        delay(FOG_PULSE_DELAY);
        digitalWrite(FOG_PULSE_PIN, LOW);
        delay(FOG_PULSE_DURATION);
        digitalWrite(FOG_PULSE_PIN, HIGH);
    }
}

// Control del nebulizador basado en humedad
void controlFog(float humidity) {
    bool shouldBeOn = humidity < humiditySetpoint;
    if (shouldBeOn != fogPulseState) {
        fogPulseState = shouldBeOn;
        generateFogPulses(fogPulseState);
    }
}

// Cargar configuración
void loadConfig() {
    if (LittleFS.exists("/config.json")) {
        File configFile = LittleFS.open("/config.json", "r");
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, configFile);
        if (!error) {
            humiditySetpoint = doc["setpoint"] | 90.0;
            recordInterval = (doc["record_interval"] | 5) * 60000;
            fanOnDuration = doc["fan_duration"] | FAN_DEFAULT_ON;
            fanInterval = doc["fan_interval"] | FAN_DEFAULT_INTERVAL;
            fanManualMode = doc["fan_manual"] | false;
            fanIsOn = doc["fan_is_on"] | false;
        }
        configFile.close();
    }
}

// Guardar configuración
void saveConfig() {
    StaticJsonDocument<512> doc;
    doc["setpoint"] = humiditySetpoint;
    doc["record_interval"] = recordInterval / 60000;
    doc["fan_duration"] = fanOnDuration;
    doc["fan_interval"] = fanInterval;
    doc["fan_manual"] = fanManualMode;
    doc["fan_is_on"] = fanIsOn;
    
    File configFile = LittleFS.open("/config.json", "w");
    if (configFile) {
        serializeJson(doc, configFile);
        configFile.close();
    }
}

// Manejar datos del sensor
void handleData() {
    String json = "{";
    json += "\"temperature\":" + String(lastTemp, 1) + ",";
    json += "\"humidity\":" + String(lastHumidity, 1) + ",";
    json += "\"fogState\":" + String(fogPulseState ? "true" : "false") + ",";
    json += "\"fanState\":" + String(fanIsOn ? "true" : "false") + ",";
    json += "\"fanManual\":" + String(fanManualMode ? "true" : "false");
    json += "}";
    server.send(200, "application/json", json);
}

// Manejar configuración
void handleConfig() {
    if (server.hasArg("setpoint")) {
        humiditySetpoint = server.arg("setpoint").toFloat();
        saveConfig();
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

// Modificar el código para servir el archivo HTML desde LittleFS
void handleRoot() {
    File file = LittleFS.open("/index.html", "r");
    if (file) {
        server.streamFile(file, "text/html");
        file.close();
    } else {
        server.send(500, "text/plain", "Error loading index.html");
    }
}

void handleHistory() {
    String range = server.hasArg("range") ? server.arg("range") : "day";
    String type = server.hasArg("type") ? server.arg("type") : "temp";
    
    // Obtener tiempo actual para calcular límites
    time_t now = timeClient.getEpochTime();
    time_t rangeLimit;
    
    if (range == "month") {
        rangeLimit = now - (30 * 24 * 3600);
    } else if (range == "week") {
        rangeLimit = now - (7 * 24 * 3600);
    } else { // day
        rangeLimit = now - (24 * 3600);
    }
    
    String json = "{\"labels\":[";
    String values = "],\"values\":[";
    
    File dataFile = LittleFS.open("/sensor_data.bin", "r");
    if (dataFile) {
        SensorData data;
        while(dataFile.read((uint8_t*)&data, sizeof(SensorData))) {
            if (data.timestamp >= rangeLimit) {
                char timeStr[30];
                if (range == "day") {
                    strftime(timeStr, sizeof(timeStr), "%H:%M", localtime(&data.timestamp));
                } else {
                    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", localtime(&data.timestamp));
                }
                
                json += "\"" + String(timeStr) + "\",";
                if (type == "temp") {
                    values += String(data.temp, 1) + ",";
                } else {
                    values += String(data.humidity, 1) + ",";
                }
            }
        }
        dataFile.close();
    }
    
    // Remover última coma y cerrar arrays
    if (json.endsWith(",")) json.remove(json.length()-1);
    if (values.endsWith(",")) values.remove(values.length()-1);
    
    json += values + "]}";
    server.send(200, "application/json", json);
}

// Función para guardar datos
void saveDataPoint() {
    SensorData data = {
        .timestamp = timeClient.getEpochTime(),
        .temp = lastTemp,
        .humidity = lastHumidity
    };
    
    File dataFile = LittleFS.open("/sensor_data.bin", "a");
    if (dataFile) {
        dataFile.write((uint8_t*)&data, sizeof(SensorData));
        dataFile.close();
    }
    
    // Mantener tamaño máximo del archivo
    File tempFile = LittleFS.open("/sensor_data.bin", "r");
    if (tempFile.size() > sizeof(SensorData) * MAX_MONTHLY_RECORDS) {
        // Implementar rotación de archivo
        // ... código para mantener solo los datos más recientes ...
    }
    tempFile.close();
}

void handleStorageConfig() {
    if (server.hasArg("interval")) {
        int minutes = server.arg("interval").toInt();
        if (minutes >= 1 && minutes <= 60) {
            recordInterval = minutes * 60000;
            
            // Guardar en configuración
            StaticJsonDocument<512> doc;
            File configFile = LittleFS.open("/config.json", "r");
            if (configFile) {
                deserializeJson(doc, configFile);
                configFile.close();
            }
            doc["record_interval"] = minutes;
            configFile = LittleFS.open("/config.json", "w");
            serializeJson(doc, configFile);
            configFile.close();
        }
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

void handleDeleteData() {
    if (LittleFS.exists("/sensor_data.bin")) {
        LittleFS.remove("/sensor_data.bin");
        server.send(200, "text/plain", "OK");
    } else {
        server.send(404, "text/plain", "No data file");
    }
}

// Función para controlar el ventilador
void controlFan(bool forcedOn = false) {
    static unsigned long fanStartTime = 0;
    unsigned long currentMillis = millis();
    
    // Modo manual
    if (fanManualMode) {
        digitalWrite(FAN_PIN, fanIsOn ? HIGH : LOW);
        return;
    }
    
    // Control basado en fog y tiempo
    if (fogPulseState) {
        // Encender con fog
        digitalWrite(FAN_PIN, HIGH);
        fanIsOn = true;
        fanStartTime = currentMillis;
        lastFanStart = currentMillis;
    } else if (!fogPulseState && currentMillis - lastFanStart >= fanInterval) {
        // Ciclo normal cada 4 horas
        digitalWrite(FAN_PIN, HIGH);
        fanIsOn = true;
        fanStartTime = currentMillis;
        lastFanStart = currentMillis;
    } else if (fanIsOn && !fogPulseState && currentMillis - fanStartTime >= fanOnDuration) {
        // Apagar después de la duración si no hay fog
        digitalWrite(FAN_PIN, LOW);
        fanIsOn = false;
    } else if (fanIsOn && !fogPulseState) {
        // Apagar inmediatamente si el fog se apaga
        digitalWrite(FAN_PIN, LOW);
        fanIsOn = false;
    }
}

// Agregar manejador para el control del ventilador
void handleFanControl() {
    if (server.hasArg("action")) {
        String action = server.arg("action");
        if (action == "toggle") {
            fanIsOn = !fanIsOn;
        } else if (action == "mode") {
            fanManualMode = !fanManualMode;
        }
    }
    
    if (server.hasArg("duration")) {
        int minutes = server.arg("duration").toInt();
        if (minutes >= 1 && minutes <= 60) {
            fanOnDuration = minutes * 60000;
        }
    }
    
    if (server.hasArg("interval")) {
        int hours = server.arg("interval").toInt();
        if (hours >= 1 && hours <= 24) {
            fanInterval = hours * 3600000;
        }
    }
    
    saveConfig();
    server.send(200);
}

void setup() {
    Serial.begin(115200);
    
    // Configurar hostname antes de iniciar WiFi
    WiFi.hostname(hostname);
    
    // Inicializar I2C
    Wire.begin(SDA_PIN, SCL_PIN);
    
    // Inicializar sensor
    if (!myAHT10.begin()) {
        Serial.println("Error al iniciar sensor AHT10!");
        while (1);
    }
    
    // Configurar y conectar WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    Serial.println("\nConectando a WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    // Inicializar mDNS
    if (MDNS.begin(hostname)) {
        Serial.println("mDNS iniciado");
        MDNS.addService("http", "tcp", 80);
    } else {
        Serial.println("Error iniciando mDNS!");
    }
    
    Serial.println("\nWiFi connected");
    Serial.println("Hostname: " + String(hostname));
    Serial.println("IP: " + WiFi.localIP().toString());
    
    // Inicializar LittleFS
    if (!LittleFS.begin()) {
        Serial.println("Error mounting LittleFS");
        return;
    }
    
    // Configurar pin del nebulizador
    pinMode(FOG_PULSE_PIN, OUTPUT);
    digitalWrite(FOG_PULSE_PIN, HIGH);
    
    // Cargar configuración
    if (LittleFS.exists("/config.json")) {
        File configFile = LittleFS.open("/config.json", "r");
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, configFile);
        if (!error) {
            humiditySetpoint = doc["setpoint"] | 90.0;
            recordInterval = (doc["record_interval"] | 5) * 60000;
            fanOnDuration = doc["fan_duration"] | FAN_DEFAULT_ON;
            fanInterval = doc["fan_interval"] | FAN_DEFAULT_INTERVAL;
            fanManualMode = doc["fan_manual"] | false;
            fanIsOn = doc["fan_is_on"] | false;
        }
        configFile.close();
    }
    
    // Inicializar NTP con zona horaria correcta
    timeClient.begin();
    timeClient.setTimeOffset(-5 * 3600); // -5 para Colombia (ajusta según tu zona horaria)
    
    // Configurar rutas del servidor web
    server.on("/", handleRoot);
    server.on("/config", HTTP_POST, handleConfig);
    server.on("/data", HTTP_GET, handleData);
    server.on("/history", HTTP_GET, handleHistory);
    server.on("/storage-config", HTTP_POST, handleStorageConfig);
    server.on("/delete-data", HTTP_POST, handleDeleteData);
    server.on("/fan-control", HTTP_POST, handleFanControl);
    server.on("/styles.css", HTTP_GET, []() {
        File file = LittleFS.open("/styles.css", "r");
        if (file) {
            server.streamFile(file, "text/css");
            file.close();
        } else {
            server.send(404, "text/plain", "CSS not found");
        }
    });

    server.on("/script.js", HTTP_GET, []() {
        File file = LittleFS.open("/script.js", "r");
        if (file) {
            server.streamFile(file, "application/javascript");
            file.close();
        } else {
            server.send(404, "text/plain", "JavaScript not found");
        }
    });
    server.begin();
    
    // Configurar pin del ventilador
    pinMode(FAN_PIN, OUTPUT);
    digitalWrite(FAN_PIN, LOW);
}

void loop() {
    static unsigned long lastRead = 0;
    unsigned long currentMillis = millis();
    
    // Actualizar NTP
    timeClient.update();
    
    // Registrar datos según el intervalo configurado
    if (currentMillis - lastRecord >= recordInterval) {
        saveDataPoint();
        lastRecord = currentMillis;
    }
    
    // Leer sensor cada 2 segundos
    if (currentMillis - lastRead >= 2000) {
        lastTemp = myAHT10.readTemperature();
        lastHumidity = myAHT10.readHumidity();
        
        // Control fog based on new readings
        controlFog(lastHumidity);
        
        lastRead = currentMillis;
    }
    
    // Agregar esto al loop
    MDNS.update();
    
    // Control del ventilador
    controlFan(fogPulseState);
    
    server.handleClient();
    yield();
} 