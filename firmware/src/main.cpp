#include <Arduino.h>
#include "config.h"
#include "wifi_manager.h"
#include "time_manager.h"
#include "reminder_store.h"
#include "api_server.h"
#include "display_manager.h"
#include "touch_manager.h"

static unsigned long lastPollMs = 0;

// Returns the earliest pending reminder that has fired (scheduled_at <= now),
// or nullptr if none.
static Reminder* getNextActiveReminder() {
    time_t now = time(nullptr);
    Reminder* earliest = nullptr;
    for (auto& r : const_cast<std::vector<Reminder>&>(reminderStore.getAll())) {
        if (r.status == ReminderStatus::Active) {
            if (!earliest || r.scheduled_at < earliest->scheduled_at) {
                earliest = &r;
            }
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
        if (active) {
            displayManager.showActive(*active);
        }
    } else if (displayManager.getState() == DisplayState::Idle) {
        // Refresh idle screen in case reminders were added/removed
        const Reminder* next = nullptr;
        time_t earliest = LONG_MAX;
        for (const auto& r : reminderStore.getAll()) {
            if (r.status == ReminderStatus::Pending && r.scheduled_at < earliest) {
                earliest = r.scheduled_at;
                next = &r;
            }
        }
        displayManager.showIdle(next);
    }
}

static void dismissCurrentReminder() {
    Reminder* active = getNextActiveReminder();
    if (!active) return;

    // PATCH via internal call (marks completed, handles recurrence)
    Recurrence rec = active->recurrence;
    String id = active->id;
    reminderStore.updateStatus(id, ReminderStatus::Completed);

    // Advance to next active, or return to idle
    Reminder* next = getNextActiveReminder();
    if (next) {
        displayManager.showActive(*next);
    } else {
        // Find next pending
        const Reminder* nextPending = nullptr;
        time_t earliest = LONG_MAX;
        for (const auto& r : reminderStore.getAll()) {
            if (r.status == ReminderStatus::Pending && r.scheduled_at < earliest) {
                earliest = r.scheduled_at;
                nextPending = &r;
            }
        }
        displayManager.showIdle(nextPending);
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[boot] that reminds me... starting");

    displayManager.begin();
    displayManager.showIdle(nullptr);  // Blank idle while booting

    if (!wifiConnect()) {
        Serial.println("[boot] Wi-Fi failed — display will show offline state");
    }

    timeSyncInit();

    reminderStore.begin();

    apiServerBegin();

    touchManager.begin();

    // Initial render
    checkAndFireReminders();

    Serial.println("[boot] Ready");
}

void loop() {
    apiServerHandle();
    touchManager.update();

    if (touchManager.wasTapped() && displayManager.getState() == DisplayState::Active) {
        dismissCurrentReminder();
    }

    if (millis() - lastPollMs >= POLL_INTERVAL_MS) {
        lastPollMs = millis();
        checkAndFireReminders();
    }
}
