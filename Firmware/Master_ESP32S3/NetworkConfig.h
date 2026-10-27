#pragma once
// Copy Secrets.example.h to Secrets.h for optional router/MQTT configuration.
#if __has_include("Secrets.h")
#include "Secrets.h"
#else
constexpr const char* STA_SSID="";
constexpr const char* STA_PASSWORD="";
constexpr const char* MQTT_HOST="";
constexpr const char* MQTT_USER="";
constexpr const char* MQTT_PASSWORD="";
constexpr uint16_t MQTT_PORT=1883;
#endif
constexpr const char* AP_SSID="SCADA_BOILER_PLTU";
