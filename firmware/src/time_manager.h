#pragma once
#include <Arduino.h>
#include <time.h>

void timeSyncInit();          // Call after Wi-Fi connects
bool isTimeSynced();
String currentTimeISO8601();  // Returns current UTC time as ISO 8601 string
