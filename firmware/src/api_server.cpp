#include "api_server.h"
#include "reminder_store.h"
#include "recurrence.h"
#include "time_manager.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include <esp_random.h>

static WebServer server(80);

// ── CORS ──────────────────────────────────────────────────────────────────────
static void addCors() {
    server.sendHeader("Access-Control-Allow-Origin",  "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, PATCH, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void handleOptions() {
    addCors();
    server.send(204);
}

// ── UUID generation ───────────────────────────────────────────────────────────
static String generateUUID() {
    uint32_t r[4];
    for (int i = 0; i < 4; i++) r[i] = esp_random();
    // Version 4, variant bits
    r[1] = (r[1] & 0xffff0fff) | 0x00004000;
    r[2] = (r[2] & 0x3fffffff) | 0x80000000;
    char buf[37];
    snprintf(buf, sizeof(buf),
        "%08x-%04x-%04x-%04x-%04x%08x",
        r[0],
        (r[1] >> 16) & 0xffff, r[1] & 0xffff,
        (r[2] >> 16) & 0xffff,
        r[2] & 0xffff, r[3]);
    return String(buf);
}

// ── Reminder JSON serializer ──────────────────────────────────────────────────
static void reminderToJson(JsonObject& obj, const Reminder& r) {
    obj["id"]           = r.id;
    obj["name"]         = r.name;
    obj["scheduled_at"] = ReminderStore::toISO8601(r.scheduled_at);
    obj["type"]         = "generic";
    obj["recurrence"]   = ReminderStore::recurrenceToString(r.recurrence);
    obj["status"]       = ReminderStore::statusToString(r.status);
    obj["created_at"]   = ReminderStore::toISO8601(r.created_at);
}

// ── GET /api/reminders ────────────────────────────────────────────────────────
static void handleGetReminders() {
    addCors();
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& r : reminderStore.getAll()) {
        if (r.status == ReminderStatus::Completed) continue;
        JsonObject obj = arr.add<JsonObject>();
        reminderToJson(obj, r);
    }
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

// ── POST /api/reminders ───────────────────────────────────────────────────────
static void handlePostReminder() {
    addCors();
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    String name = doc["name"] | "";
    name.trim();
    if (name.isEmpty() || name.length() > 40) {
        server.send(400, "application/json", "{\"error\":\"Invalid name\"}");
        return;
    }
    String scheduledStr = doc["scheduled_at"] | "";
    if (scheduledStr.isEmpty()) {
        server.send(400, "application/json", "{\"error\":\"Missing scheduled_at\"}");
        return;
    }

    Reminder r;
    r.id           = generateUUID();
    r.name         = name;
    r.scheduled_at = ReminderStore::fromISO8601(scheduledStr);
    r.recurrence   = ReminderStore::recurrenceFromString(doc["recurrence"] | "none");
    r.status       = ReminderStatus::Pending;
    r.created_at   = time(nullptr);

    if (!reminderStore.add(r)) {
        server.send(500, "application/json", "{\"error\":\"Store full or write failed\"}");
        return;
    }

    JsonDocument resp;
    JsonObject obj = resp.to<JsonObject>();
    reminderToJson(obj, r);
    String out;
    serializeJson(resp, out);
    server.send(201, "application/json", out);
}

// ── DELETE /api/reminders/:id ─────────────────────────────────────────────────
static void handleDeleteReminder() {
    addCors();
    String uri = server.uri();
    String id  = uri.substring(uri.lastIndexOf('/') + 1);
    if (reminderStore.remove(id)) {
        server.send(204);
    } else {
        server.send(404, "application/json", "{\"error\":\"Not found\"}");
    }
}

// ── PATCH /api/reminders/:id ──────────────────────────────────────────────────
static void handlePatchReminder() {
    addCors();
    String uri = server.uri();
    String id  = uri.substring(uri.lastIndexOf('/') + 1);

    JsonDocument doc;
    if (server.hasArg("plain")) {
        deserializeJson(doc, server.arg("plain"));
    }

    String statusStr = doc["status"] | "";
    ReminderStatus newStatus = ReminderStore::statusFromString(statusStr);

    Reminder* r = reminderStore.findById(id);
    if (!r) {
        server.send(404, "application/json", "{\"error\":\"Not found\"}");
        return;
    }

    // Handle recurrence: when marking completed, schedule next occurrence
    if (newStatus == ReminderStatus::Completed && r->recurrence != Recurrence::None) {
        Reminder next = *r;
        next.id           = generateUUID();
        next.scheduled_at = nextOccurrence(r->scheduled_at, r->recurrence);
        next.status       = ReminderStatus::Pending;
        next.created_at   = time(nullptr);
        reminderStore.add(next);
    }

    reminderStore.updateStatus(id, newStatus);

    Reminder* updated = reminderStore.findById(id);
    if (updated) {
        JsonDocument resp;
        JsonObject obj = resp.to<JsonObject>();
        reminderToJson(obj, *updated);
        String out;
        serializeJson(resp, out);
        server.send(200, "application/json", out);
    } else {
        server.send(200);
    }
}

// ── GET /api/time ─────────────────────────────────────────────────────────────
static void handleGetTime() {
    addCors();
    String body = "{\"time\":\"" + currentTimeISO8601() + "\",\"synced\":" +
                  (isTimeSynced() ? "true" : "false") + "}";
    server.send(200, "application/json", body);
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void apiServerBegin() {
    server.on("/api/reminders", HTTP_GET,     handleGetReminders);
    server.on("/api/reminders", HTTP_POST,    handlePostReminder);
    server.on("/api/reminders", HTTP_OPTIONS, handleOptions);
    server.on("/api/time",      HTTP_GET,     handleGetTime);

    // WebServer doesn't support path param wildcards; route /api/reminders/:id
    // via onNotFound, checking URI prefix and HTTP method.
    server.onNotFound([](){
        String uri = server.uri();
        HTTPMethod method = server.method();

        if (method == HTTP_OPTIONS) {
            handleOptions();
            return;
        }
        if (uri.startsWith("/api/reminders/")) {
            if (method == HTTP_DELETE) { handleDeleteReminder(); return; }
            if (method == HTTP_PATCH)  { handlePatchReminder();  return; }
        }
        server.send(404, "application/json", "{\"error\":\"Not found\"}");
    });

    server.begin();
    Serial.println("[api] HTTP server started on port 80");
}

void apiServerHandle() {
    server.handleClient();
}
