#pragma once
#include <Arduino.h>
#include "reminder_store.h"

enum class DisplayState { Boot, Idle, Active, Confirmation };

class DisplayManager {
public:
    bool begin();
    void showBoot();
    void showIdle(const Reminder* next);
    void showActive(const Reminder& r);
    void showConfirmation();           // Brief checkmark flash after dismiss
    void tick();                       // Call from loop() — drives ring pulse
    DisplayState getState() const { return _state; }

private:
    DisplayState _state      = DisplayState::Boot;
    uint32_t     _lastPulseMs = 0;
    bool         _pulseHigh   = false;

    void circleWipeIn(uint16_t color, uint16_t steps = 20);
    void drawImageFromFS(const char* path, uint16_t bgColor);
    void drawIdleContent(const Reminder* next);
    void drawActiveRing(bool high);
    String formatScheduledAt(time_t t);
    void wrapText(const String& text, int maxChars, std::vector<String>& lines);
};

extern DisplayManager displayManager;
