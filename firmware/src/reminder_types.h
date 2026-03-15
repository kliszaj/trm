#pragma once
#include <Arduino.h>

// ── Reminder types ────────────────────────────────────────────────────────────
// Each type has a fixed label, background color (RGB565), and image file on LittleFS.

enum class ReminderType {
    FeedEvie,
    WaterPlants,
    EatVitamins,
    TakeOutTrash,
    PayBills,
    Unknown
};

struct TypeInfo {
    const char* id;       // used in API JSON,  e.g. "feed_evie"
    const char* label;    // shown on idle screen, e.g. "Feed Evie"
    const char* imgFile;  // LittleFS path to RGB565 binary, e.g. "/img_feed_evie.bin"
    uint32_t    bgColor;  // RGB888 background color for active screen (LovyanGFX)
};

static const TypeInfo REMINDER_TYPES[] = {
    { "feed_evie",     "Feed Evie",     "/img_feed_evie.bin",      0xEC4E89 }, // pink
    { "water_plants",  "Water plants",  "/img_water_plants.bin",   0xF8B352 }, // orange
    { "eat_vitamins",  "Eat vitamins",  "/img_eat_vitamins.bin",   0x41AEFF }, // blue
    { "take_out_trash","Take out trash","/img_take_out_trash.bin", 0x7261F3 }, // purple
    { "pay_bills",     "Pay bills",     "/img_pay_bills.bin",      0xE94E51 }, // red
};
static const int REMINDER_TYPE_COUNT = 5;

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
