# Monotub Environment Controller

Un controlador de ambiente automatizado para el cultivo de hongos, basado en ESP8266. Controla la humedad mediante un sistema de nebulización y ventilación temporizada.

## Características

- Control automático de humedad mediante nebulizador
- Ventilación temporizada con control PWM
- Monitoreo de temperatura y humedad
- Interfaz web responsive
- Gráficas históricas de temperatura y humedad
- Almacenamiento de datos en memoria flash
- Control manual y automático del ventilador
- Threshold de ±5% para el setpoint de humedad

## Hardware Necesario

- NodeMCU ESP8266
- Sensor AHT10 (temperatura y humedad)
- Nebulizador ultrasónico 24V
- Ventilador 12V DC
- Fuente de alimentación 24V
- Módulo MOSFET/Relay para control del nebulizador
- Módulo MOSFET para control PWM del ventilador

## Conexiones

AHT10 -> NodeMCU
VCC    -> 3.3V
GND    -> GND
SDA    -> D2 (GPIO4)
SCL    -> D1 (GPIO5)

Nebulizador -> NodeMCU
Signal   -> D5 (GPIO14)
GND      -> GND

Ventilador -> NodeMCU
PWM      -> D6 (GPIO12)
GND      -> GND

## Configuración

1. Humedad
   - Setpoint ajustable (valor por defecto: 90%)
   - Threshold de ±5% para evitar oscilaciones
   - Control ON/OFF con pulsos para el nebulizador

2. Ventilación
   - PWM al 10% de potencia
   - Ciclo automático: 5 minutos cada 4 horas
   - Activación automática cuando el nebulizador está activo
   - Control manual disponible

3. Registro de Datos
   - Almacenamiento cada 5 minutos
   - Visualización por día/semana/mes
   - Gráficas separadas para temperatura y humedad

## Interfaz Web

- Monitoreo en tiempo real
- Configuración de parámetros
- Gráficas históricas
- Control manual del ventilador
- Gestión de datos almacenados

## Instalación

1. Clonar el repositorio
2. Instalar PlatformIO
3. Configurar `platformio.ini`:
```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
monitor_speed = 115200
board_build.filesystem = littlefs
upload_speed = 921600

lib_deps = 
    enjoyneering/AHT10@^1.1.0
    bblanchon/ArduinoJson@^6.21.3
    ESP8266mDNS
```
4. Subir el código
5. Subir el sistema de archivos

## Uso

1. Conectar a la red WiFi "Monotub"
2. Acceder a http://monotub.local o la IP asignada
3. Configurar el setpoint de humedad deseado
4. Monitorear los valores en tiempo real
5. Revisar las gráficas históricas según necesidad

## Mantenimiento

- Limpiar el sensor de humedad periódicamente
- Verificar el funcionamiento del nebulizador
- Revisar el ventilador por obstrucciones
- Hacer respaldo de datos históricos según necesidad

## Contribuir

Las contribuciones son bienvenidas. Por favor, abrir un issue para discutir cambios mayores.

## Licencia

MIT License