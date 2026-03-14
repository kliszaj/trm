#include <Arduino.h>
#include "config.h"
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
    if (fired) {
        Reminder* active = getNextActiveReminder();
        if (active) displayManager.showActive(*active);
    } else if (displayManager.getState() == DisplayState::Idle) {
        // Refresh idle — next reminder may have changed
        displayManager.showIdle(getNextPendingReminder());
    }
}

static void dismissCurrentReminder() {
    Reminder* active = getNextActiveReminder();
    if (!active) return;

    reminderStore.updateStatus(active->id, ReminderStatus::Completed);

    // Show confirmation checkmark, then transition
    displayManager.showConfirmation();

    // After confirmation: check for more active reminders or go idle
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

    displayManager.begin();
    displayManager.showBoot();   // Logo while connecting

    if (!wifiConnect()) {
        Serial.println("[boot] Wi-Fi failed");
    }

    timeSyncInit();
    reminderStore.begin();
    apiServerBegin();
    touchManager.begin();

    // First render — show active if any fired, else idle
    checkAndFireReminders();

    Serial.println("[boot] Ready");
}

void loop() {
    apiServerHandle();
    touchManager.update();
    displayManager.tick();   // Ring pulse animation

    if (touchManager.wasTapped() && displayManager.getState() == DisplayState::Active) {
        dismissCurrentReminder();
    }

    if (millis() - lastPollMs >= POLL_INTERVAL_MS) {
        lastPollMs = millis();
        checkAndFireReminders();
    }
}
