#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"

static const uint32_t CONFIRM_HOLD_MS = 3000;
#include "wifi_manager.h"
#include "time_manager.h"
#include "reminder_store.h"
#include "api_server.h"
#include "display_manager.h"
#include "touch_manager.h"

static unsigned long lastPollMs = 0;

static Reminder* getNextActiveReminder() {
    Reminder* earliest = nullptr;
    for (auto& r : const_cast<std::vector<Reminder>&>(reminderStore.getAll())) {
        if (r.status == ReminderStatus::Active) {
            if (!earliest || r.scheduled_at < earliest->scheduled_at)
                earliest = &r;
        }
    }
    return earliest;
}

static const Reminder* getNextPendingReminder() {
    const Reminder* earliest = nullptr;
    time_t t = LONG_MAX;
    for (const auto& r : reminderStore.getAll()) {
        if (r.status == ReminderStatus::Pending && r.scheduled_at < t) {
            t = r.scheduled_at;
            earliest = &r;
        }
    }
    return earliest;
}

static void checkAndFireReminders() {
    time_t now = time(nullptr);
    bool fired = false;
    for (auto& r : const_cast<std::vector<Reminder>&>(reminderStore.getAll())) {
        if (r.status == ReminderStatus::Pending && r.scheduled_at <= now) {
            reminderStore.updateStatus(r.id, ReminderStatus::Active);
            fired = true;
        }
    }

    Reminder* active = getNextActiveReminder();
    if (active) {
        // Show active screen if something just fired, or if we're not already showing it
        // (handles boot with pre-existing active reminders)
        if (fired || displayManager.getState() != DisplayState::Active) {
            displayManager.showActive(*active);
        }
    } else if (displayManager.getState() != DisplayState::Active) {
        displayManager.showIdle(getNextPendingReminder());
    }
}

static void dismissCurrentReminder() {
    Reminder* active = getNextActiveReminder();
    if (!active) return;

    String id = active->id;  // copy before modifying store

    // Show checkmark immediately for instant feedback
    displayManager.showConfirmation();

    // Update storage while checkmark is visible (hides LittleFS latency)
    reminderStore.updateStatus(id, ReminderStatus::Completed);

    // Hold checkmark for 3 seconds
    delay(CONFIRM_HOLD_MS);

    Reminder* nextActive = getNextActiveReminder();
    if (nextActive) {
        displayManager.showActive(*nextActive);
    } else {
        displayManager.showIdle(getNextPendingReminder());
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[boot] that reminds me... starting");

    // LittleFS MUST init before display (fonts/images live on flash)
    if (!LittleFS.begin(true)) {
        Serial.println("[boot] LittleFS mount failed");
    }

    displayManager.begin();
    displayManager.showBoot();

    if (!wifiConnect()) {
        Serial.println("[boot] Wi-Fi failed");
    }

    timeSyncInit();
    reminderStore.begin();
    apiServerBegin();
    touchManager.begin();

    checkAndFireReminders();

    Serial.println("[boot] Ready");
}

void loop() {
    apiServerHandle();
    touchManager.update();
    displayManager.tick();

    if (touchManager.wasTapped()) {
        Serial.printf("[main] Tap detected, display state=%d\n", (int)displayManager.getState());
        if (displayManager.getState() == DisplayState::Active) {
            Serial.println("[main] Dismissing reminder");
            dismissCurrentReminder();
        }
    }

    if (millis() - lastPollMs >= POLL_INTERVAL_MS) {
        lastPollMs = millis();
        checkAndFireReminders();
    }
}
