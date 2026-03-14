#include "display_manager.h"
#include <TFT_eSPI.h>
#include <LittleFS.h>
#include <vector>
#include <time.h>

DisplayManager displayManager;

static TFT_eSPI tft = TFT_eSPI();

// Palette
static const uint16_t COLOR_BG       = TFT_BLACK;
static const uint16_t COLOR_TEXT      = TFT_WHITE;
static const uint16_t COLOR_MUTED     = 0x7BEF;  // mid-grey
static const uint16_t COLOR_ACTIVE_BG = 0x0010;  // very dark blue
static const uint16_t COLOR_RING      = 0xFD20;  // orange ring

// Font names (VLW files on LittleFS, no extension)
static const char* FONT_SMALL  = "PPMondwest-20";   // "Next" label + time string
static const char* FONT_LARGE  = "PPMondwest-36";   // reminder name (idle)
static const char* FONT_ACTIVE = "PPMondwest-42";   // reminder name (active)

bool DisplayManager::begin() {
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(COLOR_BG);
    tft.setTextDatum(MC_DATUM);
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
    tft.setTextDatum(MC_DATUM);

    if (!next) {
        tft.loadFont(FONT_SMALL, LittleFS);
        tft.setTextColor(COLOR_MUTED, COLOR_BG);
        tft.drawString("No reminders", 120, 120);
        tft.unloadFont();
        return;
    }

    // "Next" label — small, muted, top
    tft.loadFont(FONT_SMALL, LittleFS);
    tft.setTextColor(COLOR_MUTED, COLOR_BG);
    tft.drawString("Next", 120, 72);
    tft.unloadFont();

    // Reminder name — large, centered
    tft.loadFont(FONT_LARGE, LittleFS);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);

    std::vector<String> lines;
    wrapText(next->name, tft.textWidth("W") > 0 ? 180 / tft.textWidth("W") : 10, lines);

    int lineH = tft.fontHeight() + 4;
    int startY = 120 - ((int)lines.size() - 1) * lineH / 2;
    for (int i = 0; i < (int)lines.size(); i++) {
        tft.drawString(lines[i], 120, startY + i * lineH);
    }
    tft.unloadFont();

    // Time string — small, muted, bottom
    tft.loadFont(FONT_SMALL, LittleFS);
    tft.setTextColor(COLOR_MUTED, COLOR_BG);
    tft.drawString(formatScheduledAt(next->scheduled_at), 120, 170);
    tft.unloadFont();
}

// ── Active screen ─────────────────────────────────────────────────────────────
void DisplayManager::drawActiveScreen(const Reminder& r) {
    tft.fillScreen(COLOR_ACTIVE_BG);

    // Orange ring
    tft.drawCircle(120, 120, 115, COLOR_RING);
    tft.drawCircle(120, 120, 114, COLOR_RING);
    tft.drawCircle(120, 120, 113, COLOR_RING);

    // Reminder name — large, center
    tft.setTextDatum(MC_DATUM);
    tft.loadFont(FONT_ACTIVE, LittleFS);
    tft.setTextColor(COLOR_TEXT, COLOR_ACTIVE_BG);

    std::vector<String> lines;
    wrapText(r.name, tft.textWidth("W") > 0 ? 160 / tft.textWidth("W") : 9, lines);

    int lineH = tft.fontHeight() + 4;
    int startY = 112 - ((int)lines.size() - 1) * lineH / 2;
    for (int i = 0; i < (int)lines.size(); i++) {
        tft.drawString(lines[i], 120, startY + i * lineH);
    }
    tft.unloadFont();

    // "tap to dismiss" hint — small, muted, bottom
    tft.loadFont(FONT_SMALL, LittleFS);
    tft.setTextColor(COLOR_MUTED, COLOR_ACTIVE_BG);
    tft.drawString("tap to dismiss", 120, 178);
    tft.unloadFont();
}

// ── Helpers ───────────────────────────────────────────────────────────────────
String DisplayManager::formatScheduledAt(time_t t) {
    time_t now = time(nullptr);
    struct tm tm_t, tm_now;
    localtime_r(&t,   &tm_t);
    localtime_r(&now, &tm_now);

    char timeBuf[10];
    int hour = tm_t.tm_hour;
    const char* ampm = hour >= 12 ? "PM" : "AM";
    if (hour == 0) hour = 12;
    else if (hour > 12) hour -= 12;
    snprintf(timeBuf, sizeof(timeBuf), "%d:%02d %s", hour, tm_t.tm_min, ampm);

    // ≤60 min away: show "in X min"
    long minsUntil = (long)(t - now) / 60;
    if (minsUntil >= 0 && minsUntil <= 60) {
        if (minsUntil <= 1) return String("in 1 min");
        char buf[16];
        snprintf(buf, sizeof(buf), "in %ld min", minsUntil);
        return String(buf);
    }

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
    if (maxChars < 1) maxChars = 10;
    String remaining = text;
    while (remaining.length() > (size_t)maxChars) {
        int breakAt = maxChars;
        for (int i = maxChars; i > 0; i--) {
            if (remaining.charAt(i) == ' ') { breakAt = i; break; }
        }
        lines.push_back(remaining.substring(0, breakAt));
        remaining = remaining.substring(breakAt + (remaining.charAt(breakAt) == ' ' ? 1 : 0));
    }
    if (remaining.length() > 0) lines.push_back(remaining);
}
