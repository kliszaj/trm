#include "touch_manager.h"
#include "config.h"
#include <Wire.h>
#include "esp_log.h"

TouchManager touchManager;

// CHSC6X touch controller on Seeed Round Display
static const uint8_t CHSC6X_ADDR      = 0x2e;
static const uint8_t CHSC6X_READ_LEN  = 5;
static const int     TOUCH_INT_PIN    = 20;  // D7 = GPIO20

bool TouchManager::begin() {
    // Suppress noisy I2C error logs from Wire (touch reads fail when no touch)
    esp_log_level_set("Wire", ESP_LOG_NONE);

    pinMode(TOUCH_INT_PIN, INPUT_PULLUP);
    Wire.begin(TOUCH_SDA, TOUCH_SCL);

    // Warm up I2C bus — scan all addresses (this reliably wakes the CHSC6X)
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        Wire.endTransmission();
    }

    Wire.beginTransmission(CHSC6X_ADDR);
    uint8_t err = Wire.endTransmission();
    _available = (err == 0);
    Serial.printf("[touch] CHSC6X %s (0x%02X)\n", _available ? "OK" : "not found", CHSC6X_ADDR);
    return _available;
}

void TouchManager::update() {
    if (!_available) return;

    uint8_t buf[CHSC6X_READ_LEN] = {0};
    uint8_t len = Wire.requestFrom(CHSC6X_ADDR, CHSC6X_READ_LEN);
    if (len == CHSC6X_READ_LEN) {
        Wire.readBytes(buf, len);
    } else {
        while (Wire.available()) Wire.read();
        return;
    }

    if (buf[0] == 0x01) {
        uint32_t now = millis();
        if (!_tapped && (now - _lastTouchTime) > DEBOUNCE_MS) {
            _tapped = true;
            _lastTouchTime = now;
            Serial.println("[touch] TAP");
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
