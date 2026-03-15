#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"
#include "wifi_manager.h"
#include "time_manager.h"
#include "display_manager.h"
#include "touch_manager.h"
#include "sync_client.h"

static const uint32_t CONFIRM_HOLD_MS = 3000;
static const uint32_t SCREENSAVER_TIMEOUT_MS = 120000;

static unsigned long lastSyncMs = 0;

// Currently displayed active reminder ID (so we know what to dismiss)
static String activeReminderId;

static void syncFromServer() {
    std::vector<RemoteReminder> active;
    if (syncClient.fetchActive(active) && !active.empty()) {
        // Show first active reminder
        const RemoteReminder& r = active[0];
        activeReminderId = r.id;
        if (r.type != ReminderType::Unknown) {
            Reminder displayR;
            displayR.id = r.id;
            displayR.type = r.type;
            displayR.scheduled_at = 0;
            displayR.recurrence = Recurrence::None;
            displayR.status = ReminderStatus::Active;
            displayR.created_at = 0;

            if (displayManager.getState() != DisplayState::Active) {
                displayManager.showActive(displayR);
            }
        }
        displayManager.resetInactivityTimer();
        return;
    }

    // No active reminders — show next pending or screensaver
    activeReminderId = "";
    RemoteReminder nextPending;
    if (syncClient.fetchNextPending(nextPending)) {
        Reminder displayR;
        displayR.id = nextPending.id;
        displayR.type = nextPending.type;
        // Parse ISO 8601 to time_t for display formatting
        struct tm tm = {};
        sscanf(nextPending.scheduled_at.c_str(),
               "%d-%d-%dT%d:%d:%d",
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
        displayR.scheduled_at = mktime(&tm);
        displayR.recurrence = Recurrence::None;
        displayR.status = ReminderStatus::Pending;
        displayR.created_at = 0;

        DisplayState st = displayManager.getState();
        if (st != DisplayState::Idle && st != DisplayState::Screensaver) {
            displayManager.showIdle(&displayR);
            displayManager.resetInactivityTimer();
        } else if (st == DisplayState::Idle) {
            // Refresh idle content
            displayManager.showIdle(&displayR);
        }
    } else {
        // No reminders at all — screensaver
        if (displayManager.getState() != DisplayState::Screensaver) {
            displayManager.showScreensaver();
        }
    }
}

static void dismissCurrentReminder() {
    if (activeReminderId.isEmpty()) return;

    String id = activeReminderId;

    // Show checkmark immediately
    displayManager.showConfirmation();

    // Tell web app to dismiss
    syncClient.dismiss(id);

    // Hold checkmark
    delay(CONFIRM_HOLD_MS);

    // Re-sync to show next state
    syncFromServer();
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[boot] that reminds me... starting");

    if (!LittleFS.begin(true)) {
        Serial.println("[boot] LittleFS mount failed");
    }

    displayManager.begin();
    displayManager.showBoot();

    if (!wifiConnect()) {
        Serial.println("[boot] Wi-Fi failed");
    }

    timeSyncInit();
    touchManager.begin();
    syncClient.beginSyncServer();

    // Initial sync from web app
    syncFromServer();
    lastSyncMs = millis();

    Serial.println("[boot] Ready");
}

void loop() {
    syncClient.handleSyncServer();
    touchManager.update();
    displayManager.tick();

    // Handle sync nudge from web app
    if (syncClient.wasSyncRequested()) {
        syncFromServer();
        lastSyncMs = millis();
    }

    // Handle touch
    if (touchManager.wasTapped()) {
        DisplayState st = displayManager.getState();
        displayManager.resetInactivityTimer();

        if (st == DisplayState::Screensaver) {
            syncFromServer();
        } else if (st == DisplayState::Active) {
            dismissCurrentReminder();
        }
    }

    // Screensaver timeout
    if (displayManager.getState() == DisplayState::Idle) {
        if (displayManager.isInactiveFor(SCREENSAVER_TIMEOUT_MS)) {
            displayManager.showScreensaver();
        }
    }

    // Periodic sync (safety net)
    if (millis() - lastSyncMs >= SYNC_INTERVAL_MS) {
        lastSyncMs = millis();
        syncFromServer();
    }
}
