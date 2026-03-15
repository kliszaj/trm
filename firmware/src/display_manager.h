#pragma once
#include <Arduino.h>
#include "reminder_store.h"

enum class DisplayState { Boot, Idle, Active, Confirmation, Screensaver };

class DisplayManager {
public:
    bool begin();
    void showBoot();
    void showIdle(const Reminder* next);
    void showActive(const Reminder& r);
    void showConfirmation();
    void showScreensaver();
    void tick();
    DisplayState getState() const { return _state; }
    void resetInactivityTimer();
    bool isInactiveFor(uint32_t ms) const { return (millis() - _lastActivityMs) >= ms; }

private:
    DisplayState _state = DisplayState::Boot;
    unsigned long _lastActivityMs = 0;

    // Screensaver animation state
    uint16_t _animFrameCount = 0;
    uint16_t _animFrameW = 0;
    uint16_t _animFrameH = 0;
    uint16_t _animDurations[32] = {};
    uint32_t _animDataOffset = 0;   // byte offset to first frame pixel data in file
    int      _animCurrentFrame = 0;
    unsigned long _animLastFrameMs = 0;

    void circleWipeIn(uint32_t color, uint16_t steps = 20);
    void drawImageFromFS(const char* path, uint32_t bgColor, bool halfSize = false);
    void drawIdleContent(const Reminder* next);
    void drawAnimFrame(int frameIndex);
    String formatScheduledAt(time_t t);
    void wrapText(const String& text, int maxChars, std::vector<String>& lines);
};

extern DisplayManager displayManager;
