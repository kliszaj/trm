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
static const uint32_t SCREENSAVER_TIMEOUT_MS = 120000;  // 2 minutes

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
        if (fired || (displayManager.getState() != DisplayState::Active)) {
            displayManager.showActive(*active);
            displayManager.resetInactivityTimer();
        }
    } else {
        DisplayState st = displayManager.getState();
        const Reminder* nextPending = getNextPendingReminder();
        if (nextPending) {
            // Have upcoming reminders — show idle if not already there
            if (st != DisplayState::Idle && st != DisplayState::Screensaver) {
                displayManager.showIdle(nextPending);
                displayManager.resetInactivityTimer();
            }
        } else {
            // No reminders at all — go to screensaver
            if (st != DisplayState::Screensaver) {
                displayManager.showScreensaver();
            }
        }
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
        const Reminder* nextPending = getNextPendingReminder();
        if (nextPending) {
            displayManager.showIdle(nextPending);
        } else {
            displayManager.showScreensaver();
        }
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
        DisplayState st = displayManager.getState();
        displayManager.resetInactivityTimer();

        if (st == DisplayState::Screensaver) {
            // Exit screensaver back to idle
            displayManager.showIdle(getNextPendingReminder());
        } else if (st == DisplayState::Active) {
            dismissCurrentReminder();
        }
    }

    // Enter screensaver after inactivity on idle screen
    if (displayManager.getState() == DisplayState::Idle) {
        if (displayManager.isInactiveFor(SCREENSAVER_TIMEOUT_MS)) {
            displayManager.showScreensaver();
        }
    }

    if (millis() - lastPollMs >= POLL_INTERVAL_MS) {
        lastPollMs = millis();
        checkAndFireReminders();
    }
}
