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
    DisplayState _state = DisplayState::Boot;

    void circleWipeIn(uint32_t color, uint16_t steps = 20);
    void drawImageFromFS(const char* path, uint32_t bgColor, bool halfSize = false);
    void drawIdleContent(const Reminder* next);
    String formatScheduledAt(time_t t);
    void wrapText(const String& text, int maxChars, std::vector<String>& lines);
};

extern DisplayManager displayManager;
