#pragma once
#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>
#include "reminder_types.h"

struct RemoteReminder {
    String id;
    ReminderType type;
    String scheduled_at;
    String status;  // "pending", "active", "completed"
};

class SyncClient {
public:
    bool fetchActive(std::vector<RemoteReminder>& out);
    bool fetchNextPending(RemoteReminder& out);
    bool dismiss(const String& id);
    void beginSyncServer();   // Start minimal /sync endpoint
    void handleSyncServer();  // Call from loop()
    bool wasSyncRequested();  // Check if /sync was hit

private:
    bool _syncRequested = false;
    bool fetchReminders(const String& path, std::vector<RemoteReminder>& out);
};

extern SyncClient syncClient;
