#include "display_manager.h"
#include <TFT_eSPI.h>
#include <vector>
#include <time.h>

DisplayManager displayManager;

static TFT_eSPI tft = TFT_eSPI();

// Palette
static const uint16_t COLOR_BG       = TFT_BLACK;
static const uint16_t COLOR_TEXT      = TFT_WHITE;
static const uint16_t COLOR_MUTED     = 0x7BEF;  // ~mid-grey
static const uint16_t COLOR_ACCENT    = 0x07FF;  // Cyan
static const uint16_t COLOR_ACTIVE_BG = 0x0010;  // Very dark blue (active state)
static const uint16_t COLOR_RING      = 0xFD20;  // Orange ring for active state

bool DisplayManager::begin() {
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(COLOR_BG);
    tft.setTextDatum(MC_DATUM);  // Middle-Centre default
    Serial.println("[display] Initialized");
    return true;
}

void DisplayManager::showIdle(const Reminder* next) {
    _state = DisplayState::Idle;
    drawIdleScreen(next);
}

void DisplayManager::showActive(const Reminder& r) {
    _state = DisplayState::Active;
    drawActiveScreen(r);
}

// ── Idle screen ───────────────────────────────────────────────────────────────
void DisplayManager::drawIdleScreen(const Reminder* next) {
    tft.fillScreen(COLOR_BG);

    if (!next) {
        tft.setTextColor(COLOR_MUTED, COLOR_BG);
        tft.setTextFont(2);
        tft.setTextSize(1);
        tft.drawString("No reminders", 120, 120);
        return;
    }

    // "NEXT" label — small, muted, top area
    tft.setTextColor(COLOR_MUTED, COLOR_BG);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.drawString("NEXT", 120, 70);

    // Reminder name — large, center
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextFont(4);
    tft.setTextSize(1);

    // Word-wrap within ~180px circle chord at center
    std::vector<String> lines;
    wrapText(next->name, 16, lines);  // ~16 chars per line for font4

    int lineH = 28;
    int startY = 120 - ((int)lines.size() - 1) * lineH / 2;
    for (int i = 0; i < (int)lines.size(); i++) {
        tft.drawString(lines[i], 120, startY + i * lineH);
    }

    // Time string — below name, medium, accent colour
    tft.setTextColor(COLOR_ACCENT, COLOR_BG);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.drawString(formatScheduledAt(next->scheduled_at), 120, 170);
}

// ── Active screen ─────────────────────────────────────────────────────────────
void DisplayManager::drawActiveScreen(const Reminder& r) {
    tft.fillScreen(COLOR_ACTIVE_BG);

    // Outer pulsing ring — drawn as a thick circle
    tft.drawCircle(120, 120, 115, COLOR_RING);
    tft.drawCircle(120, 120, 114, COLOR_RING);
    tft.drawCircle(120, 120, 113, COLOR_RING);

    // Reminder name — large, center
    tft.setTextColor(COLOR_TEXT, COLOR_ACTIVE_BG);
    tft.setTextFont(4);
    tft.setTextSize(1);

    std::vector<String> lines;
    wrapText(r.name, 14, lines);  // Slightly narrower to stay within ring

    int lineH = 30;
    int startY = 110 - ((int)lines.size() - 1) * lineH / 2;
    for (int i = 0; i < (int)lines.size(); i++) {
        tft.drawString(lines[i], 120, startY + i * lineH);
    }

    // "TAP TO DISMISS" hint
    tft.setTextColor(COLOR_MUTED, COLOR_ACTIVE_BG);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.drawString("TAP TO DISMISS", 120, 175);
}

// ── Helpers ───────────────────────────────────────────────────────────────────
String DisplayManager::formatScheduledAt(time_t t) {
    time_t now = time(nullptr);
    struct tm tm_t, tm_now;
    localtime_r(&t,   &tm_t);
    localtime_r(&now, &tm_now);

    char timeBuf[10];
    // 12-hour format: "6:30 PM"
    int hour = tm_t.tm_hour;
    const char* ampm = hour >= 12 ? "PM" : "AM";
    if (hour == 0) hour = 12;
    else if (hour > 12) hour -= 12;
    snprintf(timeBuf, sizeof(timeBuf), "%d:%02d %s", hour, tm_t.tm_min, ampm);

    // Same day?
    if (tm_t.tm_year == tm_now.tm_year && tm_t.tm_yday == tm_now.tm_yday) {
        return String("Today, ") + timeBuf;
    }
    // Tomorrow?
    time_t tomorrow = now + 86400;
    struct tm tm_tom;
    localtime_r(&tomorrow, &tm_tom);
    if (tm_t.tm_year == tm_tom.tm_year && tm_t.tm_yday == tm_tom.tm_yday) {
        return String("Tomorrow, ") + timeBuf;
    }
    // Day of week
    const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    return String(days[tm_t.tm_wday]) + ", " + timeBuf;
}

void DisplayManager::wrapText(const String& text, int maxChars, std::vector<String>& lines) {
    String remaining = text;
    while (remaining.length() > (size_t)maxChars) {
        int breakAt = maxChars;
        // Prefer breaking at a space
        for (int i = maxChars; i > 0; i--) {
            if (remaining.charAt(i) == ' ') { breakAt = i; break; }
        }
        lines.push_back(remaining.substring(0, breakAt));
        remaining = remaining.substring(breakAt + (remaining.charAt(breakAt) == ' ' ? 1 : 0));
    }
    if (remaining.length() > 0) lines.push_back(remaining);
}
