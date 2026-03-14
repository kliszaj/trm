#pragma once
#include <Arduino.h>

class TouchManager {
public:
    bool begin();
    void update();       // Call from loop()
    bool wasTapped();    // Returns true once per tap, then resets
private:
    bool _tapped = false;
    uint32_t _lastTouchTime = 0;
    static const uint32_t DEBOUNCE_MS = 300;
};

extern TouchManager touchManager;
