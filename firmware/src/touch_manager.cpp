#include "touch_manager.h"
#include "config.h"
#include <Wire.h>

TouchManager touchManager;

// CST816S I2C address
static const uint8_t CST816S_ADDR  = 0x15;
static const uint8_t REG_GESTURE   = 0x01;
static const uint8_t REG_FINGER    = 0x02;
static const uint8_t REG_X_H       = 0x03;

bool TouchManager::begin() {
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    // Quick check: try to read a byte from CST816S
    Wire.beginTransmission(CST816S_ADDR);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        Serial.printf("[touch] CST816S not found (err=%d)\n", err);
        return false;
    }
    Serial.println("[touch] CST816S initialized");
    return true;
}

void TouchManager::update() {
    // Poll finger count register
    Wire.beginTransmission(CST816S_ADDR);
    Wire.write(REG_FINGER);
    if (Wire.endTransmission(false) != 0) return;

    Wire.requestFrom(CST816S_ADDR, (uint8_t)1);
    if (!Wire.available()) return;
    uint8_t fingers = Wire.read() & 0x0F;

    if (fingers > 0) {
        uint32_t now = millis();
        if (!_tapped && (now - _lastTouchTime) > DEBOUNCE_MS) {
            _tapped = true;
            _lastTouchTime = now;
        }
    }
}

bool TouchManager::wasTapped() {
    if (_tapped) {
        _tapped = false;
        return true;
    }
    return false;
}
