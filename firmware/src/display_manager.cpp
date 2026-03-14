#include "display_manager.h"
#include <TFT_eSPI.h>
#include <LittleFS.h>
#include <vector>
#include <time.h>

DisplayManager displayManager;

static TFT_eSPI tft = TFT_eSPI();

// Palette
static const uint16_t COLOR_BG           = TFT_BLACK;
static const uint16_t COLOR_TEXT         = TFT_WHITE;
static const uint16_t COLOR_MUTED        = 0x7BEF;
static const uint16_t COLOR_BOOT_BG      = 0xFFFF;  // #FEFEFE
static const uint16_t COLOR_CONFIRM_BG   = 0x6612;  // #61C090 green
static const uint16_t COLOR_RING_HIGH    = 0xFD20;  // orange bright
static const uint16_t COLOR_RING_LOW     = 0xFC00;  // orange dim
static const uint32_t PULSE_INTERVAL_MS  = 700;
static const uint32_t CONFIRM_HOLD_MS    = 900;     // how long checkmark stays up

// Font names (VLW on LittleFS)
static const char* FONT_SMALL  = "PPMondwest-20";
static const char* FONT_LARGE  = "PPMondwest-36";

// ── Init ──────────────────────────────────────────────────────────────────────
bool DisplayManager::begin() {
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(COLOR_BG);
    tft.setTextDatum(MC_DATUM);
    Serial.println("[display] Initialized");
    return true;
}

// ── Boot screen ───────────────────────────────────────────────────────────────
void DisplayManager::showBoot() {
    _state = DisplayState::Boot;
    tft.fillScreen(COLOR_BOOT_BG);
    drawImageFromFS("/img_logo.bin", COLOR_BOOT_BG);
}

// ── Idle screen ───────────────────────────────────────────────────────────────
void DisplayManager::showIdle(const Reminder* next) {
    circleWipeIn(COLOR_BG);
    _state = DisplayState::Idle;
    drawIdleContent(next);
}

void DisplayManager::drawIdleContent(const Reminder* next) {
    tft.fillScreen(COLOR_BG);
    tft.setTextDatum(MC_DATUM);

    if (!next) {
        tft.loadFont(FONT_SMALL, LittleFS);
        tft.setTextColor(COLOR_MUTED, COLOR_BG);
        tft.drawString("No reminders", 120, 120);
        tft.unloadFont();
        return;
    }

    // "Next" — small, muted, top
    tft.loadFont(FONT_SMALL, LittleFS);
    tft.setTextColor(COLOR_MUTED, COLOR_BG);
    tft.drawString("Next", 120, 72);
    tft.unloadFont();

    // Reminder label — large, center
    tft.loadFont(FONT_LARGE, LittleFS);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    std::vector<String> lines;
    int charsPerLine = tft.textWidth("W") > 0 ? 180 / tft.textWidth("W") : 10;
    wrapText(next->label(), charsPerLine, lines);
    int lineH  = tft.fontHeight() + 4;
    int startY = 120 - ((int)lines.size() - 1) * lineH / 2;
    for (int i = 0; i < (int)lines.size(); i++) {
        tft.drawString(lines[i], 120, startY + i * lineH);
    }
    tft.unloadFont();

    // Time — small, muted, bottom
    tft.loadFont(FONT_SMALL, LittleFS);
    tft.setTextColor(COLOR_MUTED, COLOR_BG);
    tft.drawString(formatScheduledAt(next->scheduled_at), 120, 170);
    tft.unloadFont();
}

// ── Active screen ─────────────────────────────────────────────────────────────
void DisplayManager::showActive(const Reminder& r) {
    const TypeInfo* info = getTypeInfo(r.type);
    circleWipeIn(info->bgColor);
    _state        = DisplayState::Active;
    _lastPulseMs  = millis();
    _pulseHigh    = true;

    tft.fillScreen(info->bgColor);
    drawImageFromFS(info->imgFile, info->bgColor);
    drawActiveRing(true);
}

void DisplayManager::drawActiveRing(bool high) {
    uint16_t c = high ? COLOR_RING_HIGH : COLOR_RING_LOW;
    tft.drawCircle(120, 120, 115, c);
    tft.drawCircle(120, 120, 114, c);
    tft.drawCircle(120, 120, 113, c);
}

// ── Confirmation screen ───────────────────────────────────────────────────────
void DisplayManager::showConfirmation() {
    circleWipeIn(COLOR_CONFIRM_BG);
    _state = DisplayState::Confirmation;
    tft.fillScreen(COLOR_CONFIRM_BG);
    drawImageFromFS("/img_checkmark.bin", COLOR_CONFIRM_BG);
    delay(CONFIRM_HOLD_MS);
}

// ── Tick (call from loop) ─────────────────────────────────────────────────────
void DisplayManager::tick() {
    if (_state != DisplayState::Active) return;
    if (millis() - _lastPulseMs < PULSE_INTERVAL_MS) return;
    _lastPulseMs = millis();
    _pulseHigh   = !_pulseHigh;
    drawActiveRing(_pulseHigh);
}

// ── Circle wipe transition ────────────────────────────────────────────────────
// Expands a filled circle from the center outward, covering the current screen.
// Creates a smooth iris-in effect before the new content is drawn.
void DisplayManager::circleWipeIn(uint16_t color, uint16_t steps) {
    const int MAX_R = 122;
    for (int i = 1; i <= (int)steps; i++) {
        int r = (MAX_R * i) / steps;
        tft.fillCircle(120, 120, r, color);
        delay(12);
    }
}

// ── Image drawing (line-by-line from LittleFS, 240×240 RGB565 big-endian) ─────
// Pixels matching 0xF81F (magenta chroma key) are skipped (transparent).
void DisplayManager::drawImageFromFS(const char* path, uint16_t bgColor) {
    File img = LittleFS.open(path, "r");
    if (!img) {
        Serial.printf("[display] Image not found: %s\n", path);
        return;
    }
    uint16_t lineBuf[240];
    for (int y = 0; y < 240; y++) {
        if (img.read((uint8_t*)lineBuf, 480) != 480) break;
        for (int x = 0; x < 240; x++) {
            lineBuf[x] = __builtin_bswap16(lineBuf[x]);
        }
        tft.pushImage(0, y, 240, 1, lineBuf, (uint16_t)0xF81F);
    }
    img.close();
}

// ── Time formatting ───────────────────────────────────────────────────────────
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

    long minsUntil = (long)(t - now) / 60;
    if (minsUntil >= 0 && minsUntil <= 60) {
        if (minsUntil <= 1) return String("in 1 min");
        char buf[16];
        snprintf(buf, sizeof(buf), "in %ld min", minsUntil);
        return String(buf);
    }

    if (tm_t.tm_year == tm_now.tm_year && tm_t.tm_yday == tm_now.tm_yday) {
        return String("Today, ") + timeBuf;
    }
    time_t tomorrow = now + 86400;
    struct tm tm_tom;
    localtime_r(&tomorrow, &tm_tom);
    if (tm_t.tm_year == tm_tom.tm_year && tm_t.tm_yday == tm_tom.tm_yday) {
        return String("Tomorrow, ") + timeBuf;
    }
    const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    return String(days[tm_t.tm_wday]) + ", " + timeBuf;
}

// ── Text wrapping ─────────────────────────────────────────────────────────────
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
