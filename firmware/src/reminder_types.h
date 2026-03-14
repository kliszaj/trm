#pragma once
#include <Arduino.h>

// ── Reminder types ────────────────────────────────────────────────────────────
// Each type has a fixed label, background color (RGB565), and image file on LittleFS.

enum class ReminderType {
    FeedEvie,
    WaterPlants,
    EatVitamins,
    TakeOutTrash,
    Unknown
};

struct TypeInfo {
    const char* id;       // used in API JSON,  e.g. "feed_evie"
    const char* label;    // shown on idle screen, e.g. "Feed Evie"
    const char* imgFile;  // LittleFS path to RGB565 binary, e.g. "/img_feed_evie.bin"
    uint16_t    bgColor;  // RGB565 background color for active screen
};

// RGB565: ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
static const TypeInfo REMINDER_TYPES[] = {
    { "feed_evie",     "Feed Evie",     "/img_feed_evie.bin",      0xE8F1 }, // #E91D8F pink
    { "water_plants",  "Water plants",  "/img_water_plants.bin",   0xF924 }, // #F5A623 orange
    { "eat_vitamins",  "Eat vitamins",  "/img_eat_vitamins.bin",   0x4D7C }, // #4BAEE4 blue
    { "take_out_trash","Take out trash","/img_take_out_trash.bin", 0x7B78 }, // #7B6CC6 purple
};
static const int REMINDER_TYPE_COUNT = 4;

inline const TypeInfo* getTypeInfo(ReminderType t) {
    int idx = (int)t;
    if (idx < 0 || idx >= REMINDER_TYPE_COUNT) return &REMINDER_TYPES[0];
    return &REMINDER_TYPES[idx];
}

inline ReminderType reminderTypeFromString(const String& s) {
    for (int i = 0; i < REMINDER_TYPE_COUNT; i++) {
        if (s == REMINDER_TYPES[i].id) return (ReminderType)i;
    }
    return ReminderType::Unknown;
}

inline String reminderTypeToString(ReminderType t) {
    int idx = (int)t;
    if (idx < 0 || idx >= REMINDER_TYPE_COUNT) return "feed_evie";
    return String(REMINDER_TYPES[idx].id);
}
