#include "display_manager.h"
#include <LittleFS.h>
#include "lgfx_config.h"
#include <vector>
#include <time.h>

DisplayManager displayManager;

static LGFX tft;

static const char* ANIM_FILE = "/anim_eye.bin";

// Palette (RGB888 for LovyanGFX drawing APIs)
static const uint32_t COLOR_BG           = 0x000000;  // black
static const uint32_t COLOR_TEXT         = 0xFFFFFF;  // white
static const uint32_t COLOR_MUTED        = 0x7B7B7B;  // mid grey
static const uint32_t COLOR_BOOT_BG      = 0xFEFEFE;  // near-white
static const uint32_t COLOR_CONFIRM_BG   = 0x61C090;  // green

// Font names (VLW on LittleFS)
static const char* FONT_SMALL  = "/PPMondwest-Bold-22.vlw";
static const char* FONT_LARGE  = "/PPMondwest-Bold-38.vlw";

// ── Init ──────────────────────────────────────────────────────────────────────
bool DisplayManager::begin() {
    tft.init();
    tft.setRotation(3);
    tft.invertDisplay(true);   // GC9A01 on Seeed Round Display needs INVON
    tft.setBrightness(255);
    tft.fillScreen(COLOR_BG);
    tft.setTextDatum(middle_center);
    tft.setFileStorage(LittleFS);  // VLW fonts live on LittleFS
    _lastActivityMs = millis();
    Serial.println("[display] Initialized");
    return true;
}

void DisplayManager::resetInactivityTimer() {
    _lastActivityMs = millis();
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
    _lastActivityMs = millis();
    drawIdleContent(next);
}

void DisplayManager::drawIdleContent(const Reminder* next) {
    tft.fillScreen(COLOR_BG);
    tft.setTextDatum(middle_center);

    if (!next) {
        tft.loadFont(FONT_SMALL);
        tft.setTextColor(COLOR_MUTED, COLOR_BG);
        tft.drawString("No reminders", 120, 120);
        tft.unloadFont();
        return;
    }

    // "Next" — small, muted, top (20px padding from circle edge)
    tft.loadFont(FONT_SMALL);
    tft.setTextColor(COLOR_MUTED, COLOR_BG);
    tft.drawString("Next", 120, 45);
    tft.unloadFont();

    // Reminder label — large, bold, centered (max 2 lines, 200px wide)
    tft.loadFont(FONT_LARGE);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    const int MAX_WIDTH = 200;
    String label = next->label();

    if (tft.textWidth(label) <= MAX_WIDTH) {
        // Fits on one line
        tft.drawString(label, 120, 120);
    } else {
        // Word-wrap: find last space that keeps line 1 within MAX_WIDTH
        int breakAt = -1;
        for (int i = 0; i < (int)label.length(); i++) {
            if (label.charAt(i) == ' ') {
                String candidate = label.substring(0, i);
                if (tft.textWidth(candidate) <= MAX_WIDTH) breakAt = i;
                else break;
            }
        }
        String line1, line2;
        if (breakAt > 0) {
            line1 = label.substring(0, breakAt);
            line2 = label.substring(breakAt + 1);
        } else {
            line1 = label;
            line2 = "";
        }
        int lineH = tft.fontHeight() + 4;
        tft.drawString(line1, 120, 120 - lineH / 2);
        if (line2.length() > 0) tft.drawString(line2, 120, 120 + lineH / 2);
    }
    tft.unloadFont();

    // Time — small, muted, bottom (20px padding from circle edge)
    tft.loadFont(FONT_SMALL);
    tft.setTextColor(COLOR_MUTED, COLOR_BG);
    tft.drawString(formatScheduledAt(next->scheduled_at), 120, 195);
    tft.unloadFont();
}

// ── Active screen ─────────────────────────────────────────────────────────────
void DisplayManager::showActive(const Reminder& r) {
    const TypeInfo* info = getTypeInfo(r.type);
    circleWipeIn(info->bgColor);
    _state = DisplayState::Active;

    tft.fillScreen(info->bgColor);
    drawImageFromFS(info->imgFile, info->bgColor);
    Serial.printf("[display] Active screen shown, state=%d\n", (int)_state);
}

// ── Confirmation screen ───────────────────────────────────────────────────────
void DisplayManager::showConfirmation() {
    circleWipeIn(COLOR_CONFIRM_BG);
    _state = DisplayState::Confirmation;
    tft.fillScreen(COLOR_CONFIRM_BG);
    drawImageFromFS("/img_checkmark.bin", COLOR_CONFIRM_BG);
    // No blocking delay here — caller is responsible for hold timing
}

// ── Screensaver ──────────────────────────────────────────────────────────────
void DisplayManager::showScreensaver() {
    // Load animation header from file
    fs::File f = LittleFS.open(ANIM_FILE, "r");
    if (!f) {
        Serial.println("[display] Screensaver animation not found");
        return;
    }

    f.read((uint8_t*)&_animFrameCount, 2);
    f.read((uint8_t*)&_animFrameW, 2);
    f.read((uint8_t*)&_animFrameH, 2);

    if (_animFrameCount > 32) _animFrameCount = 32;

    for (int i = 0; i < _animFrameCount; i++) {
        f.read((uint8_t*)&_animDurations[i], 2);
    }

    // Data offset = 6 bytes header + 2 bytes per frame duration
    _animDataOffset = 6 + _animFrameCount * 2;
    f.close();

    circleWipeIn(COLOR_BG);
    _state = DisplayState::Screensaver;
    _animCurrentFrame = 0;
    _animLastFrameMs = 0;  // force first frame to draw immediately
    Serial.printf("[display] Screensaver started (%d frames, %dx%d)\n",
                  _animFrameCount, _animFrameW, _animFrameH);
}

void DisplayManager::drawAnimFrame(int frameIndex) {
    fs::File f = LittleFS.open(ANIM_FILE, "r");
    if (!f) return;

    uint32_t frameBytes = _animFrameW * _animFrameH * 2;
    uint32_t offset = _animDataOffset + frameIndex * frameBytes;
    f.seek(offset);

    // Read and draw line by line, scaling 2x (120->240)
    uint16_t lineBuf[120];
    uint16_t scaledBuf[240];

    for (int y = 0; y < _animFrameH; y++) {
        if (f.read((uint8_t*)lineBuf, _animFrameW * 2) != (int)(_animFrameW * 2)) break;
        // Scale each pixel 2x horizontally
        for (int x = 0; x < _animFrameW; x++) {
            scaledBuf[x * 2]     = lineBuf[x];
            scaledBuf[x * 2 + 1] = lineBuf[x];
        }
        // Push same row twice for 2x vertical scaling
        tft.pushImage(0, y * 2,     240, 1, scaledBuf);
        tft.pushImage(0, y * 2 + 1, 240, 1, scaledBuf);
    }
    f.close();
}

// ── Tick (call from loop) ─────────────────────────────────────────────────────
void DisplayManager::tick() {
    if (_state == DisplayState::Screensaver) {
        unsigned long now = millis();
        if (_animLastFrameMs == 0 || (now - _animLastFrameMs) >= _animDurations[_animCurrentFrame]) {
            drawAnimFrame(_animCurrentFrame);
            _animLastFrameMs = now;
            _animCurrentFrame = (_animCurrentFrame + 1) % _animFrameCount;
        }
    }
}

// ── Circle wipe transition ────────────────────────────────────────────────────
void DisplayManager::circleWipeIn(uint32_t color, uint16_t steps) {
    const int MAX_R = 122;
    for (int i = 1; i <= (int)steps; i++) {
        int r = (MAX_R * i) / steps;
        tft.fillCircle(120, 120, r, color);
        delay(12);
    }
}

// ── Image drawing (line-by-line from LittleFS, 240×240 RGB565 big-endian) ─────
// File stores big-endian RGB565. On little-endian ESP32, the uint16_t values are
// byte-swapped, but SPI sends bytes in memory order — so the panel receives
// correct big-endian RGB565 without any manual byte swap.
// Transparent pixels (magenta 0xF81F BE = 0x1FF8 in LE buffer) are replaced
// with bgColor before pushing.
void DisplayManager::drawImageFromFS(const char* path, uint32_t bgColor, bool halfSize) {
    fs::File img = LittleFS.open(path, "r");
    if (!img) {
        Serial.printf("[display] Image not found: %s\n", path);
        return;
    }
    // Convert RGB888 bgColor to big-endian RGB565 stored as LE uint16_t
    uint8_t r = (bgColor >> 16) & 0xFF;
    uint8_t g = (bgColor >> 8) & 0xFF;
    uint8_t b = bgColor & 0xFF;
    uint16_t bg565 = __builtin_bswap16(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));

    static const uint16_t MAGENTA_LE = 0x1FF8;  // 0xF81F big-endian read as LE
    uint16_t lineBuf[240];

    if (!halfSize) {
        for (int y = 0; y < 240; y++) {
            if (img.read((uint8_t*)lineBuf, 480) != 480) break;
            for (int x = 0; x < 240; x++) {
                if (lineBuf[x] == MAGENTA_LE) lineBuf[x] = bg565;
            }
            tft.pushImage(0, y, 240, 1, lineBuf);
        }
    } else {
        // Half size (120x120), centered on 240x240 display
        uint16_t scaledBuf[120];
        for (int y = 0; y < 240; y++) {
            if (img.read((uint8_t*)lineBuf, 480) != 480) break;
            if (y & 1) continue;  // skip odd rows
            for (int x = 0; x < 120; x++) {
                uint16_t px = lineBuf[x * 2];
                scaledBuf[x] = (px == MAGENTA_LE) ? bg565 : px;
            }
            tft.pushImage(60, 60 + y / 2, 120, 1, scaledBuf);
        }
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
        return String(timeBuf);
    }
    time_t tomorrow = now + 86400;
    struct tm tm_tom;
    localtime_r(&tomorrow, &tm_tom);
    if (tm_t.tm_year == tm_tom.tm_year && tm_t.tm_yday == tm_tom.tm_yday) {
        return String("Tomorrow");
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
