#include "sync_client.h"
#include "config.h"
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>

SyncClient syncClient;

static WebServer syncServer(80);

bool SyncClient::fetchReminders(const String& path, std::vector<RemoteReminder>& out) {
    HTTPClient http;
    String url = String(WEB_APP_URL) + path;

    http.begin(url);
    http.setTimeout(5000);
    int code = http.GET();

    if (code != 200) {
        Serial.printf("[sync] GET %s failed: %d\n", path.c_str(), code);
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[sync] JSON parse error: %s\n", err.c_str());
        return false;
    }

    JsonArray arr = doc.as<JsonArray>();
    out.clear();
    for (JsonObject obj : arr) {
        RemoteReminder r;
        r.id = obj["id"].as<String>();
        r.type = reminderTypeFromString(obj["type"].as<String>());
        r.scheduled_at = obj["scheduled_at"].as<String>();
        r.status = obj["status"].as<String>();
        out.push_back(r);
    }

    Serial.printf("[sync] GET %s: %d reminders\n", path.c_str(), (int)out.size());
    return true;
}

bool SyncClient::fetchActive(std::vector<RemoteReminder>& out) {
    return fetchReminders("/api/reminders/active", out);
}

bool SyncClient::fetchNextPending(RemoteReminder& out) {
    std::vector<RemoteReminder> all;
    if (!fetchReminders("/api/reminders", all)) return false;

    // Find earliest pending
    RemoteReminder* earliest = nullptr;
    for (auto& r : all) {
        if (r.status == "pending") {
            if (!earliest || r.scheduled_at < earliest->scheduled_at) {
                earliest = &r;
            }
        }
    }

    if (earliest) {
        out = *earliest;
        return true;
    }
    return false;
}

bool SyncClient::dismiss(const String& id) {
    HTTPClient http;
    String url = String(WEB_APP_URL) + "/api/reminders/" + id + "/dismiss";

    http.begin(url);
    http.setTimeout(5000);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST("{}");
    http.end();

    Serial.printf("[sync] Dismiss %s: %d\n", id.c_str(), code);
    return code == 200;
}

void SyncClient::beginSyncServer() {
    syncServer.on("/sync", HTTP_POST, [this]() {
        _syncRequested = true;
        syncServer.send(200, "text/plain", "OK");
        Serial.println("[sync] Nudge received");
    });

    syncServer.on("/sync", HTTP_OPTIONS, []() {
        syncServer.sendHeader("Access-Control-Allow-Origin", "*");
        syncServer.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        syncServer.send(204);
    });

    syncServer.begin();
    Serial.println("[sync] Sync server started on port 80");
}

void SyncClient::handleSyncServer() {
    syncServer.handleClient();
}

bool SyncClient::wasSyncRequested() {
    if (_syncRequested) {
        _syncRequested = false;
        return true;
    }
    return false;
}
