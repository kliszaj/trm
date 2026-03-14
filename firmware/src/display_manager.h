#pragma once
#include <Arduino.h>
#include "reminder_store.h"

enum class DisplayState { Idle, Active };

class DisplayManager {
public:
    bool begin();
    void showIdle(const Reminder* next);   // nullptr = "No reminders"
    void showActive(const Reminder& r);
    DisplayState getState() const { return _state; }

private:
    DisplayState _state = DisplayState::Idle;
    void drawIdleScreen(const Reminder* next);
    void drawActiveScreen(const Reminder& r);
    String formatScheduledAt(time_t t);
    void wrapText(const String& text, int maxWidth, std::vector<String>& lines);
};

extern DisplayManager displayManager;
