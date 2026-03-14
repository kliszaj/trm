#include "reminder_store.h"
#include "config.h"
#include <LittleFS.h>
#include <time.h>

ReminderStore reminderStore;

bool ReminderStore::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("[store] LittleFS mount failed");
        return false;
    }
    if (!LittleFS.exists(REMINDERS_PATH)) {
        Serial.println("[store] No reminders file, starting fresh");
        return true;
    }
    File f = LittleFS.open(REMINDERS_PATH, "r");
    if (!f) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[store] JSON parse error: %s\n", err.c_str());
        return false;
    }

    _reminders.clear();
    for (JsonObject obj : doc.as<JsonArray>()) {
        Reminder r;
        if (deserializeReminder(obj, r)) {
            _reminders.push_back(r);
        }
    }
    Serial.printf("[store] Loaded %d reminders\n", _reminders.size());
    return true;
}

const std::vector<Reminder>& ReminderStore::getAll() const {
    return _reminders;
}

bool ReminderStore::add(const Reminder& r) {
    if ((int)_reminders.size() >= MAX_REMINDERS) return false;
    _reminders.push_back(r);
    return save();
}

bool ReminderStore::remove(const String& id) {
    for (auto it = _reminders.begin(); it != _reminders.end(); ++it) {
        if (it->id == id) {
            _reminders.erase(it);
            return save();
        }
    }
    return false;
}

bool ReminderStore::updateStatus(const String& id, ReminderStatus status) {
    Reminder* r = findById(id);
    if (!r) return false;
    r->status = status;
    return save();
}

Reminder* ReminderStore::findById(const String& id) {
    for (auto& r : _reminders) {
        if (r.id == id) return &r;
    }
    return nullptr;
}

bool ReminderStore::save() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (auto& r : _reminders) {
        JsonObject obj = arr.add<JsonObject>();
        serializeReminder(obj, r);
    }
    File f = LittleFS.open(REMINDERS_PATH, "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

void ReminderStore::serializeReminder(JsonObject& obj, const Reminder& r) {
    obj["id"]           = r.id;
    obj["name"]         = r.label();   // derived from type for API consumers
    obj["scheduled_at"] = toISO8601(r.scheduled_at);
    obj["type"]         = reminderTypeToString(r.type);
    obj["recurrence"]   = recurrenceToString(r.recurrence);
    obj["status"]       = statusToString(r.status);
    obj["created_at"]   = toISO8601(r.created_at);
}

bool ReminderStore::deserializeReminder(JsonObject& obj, Reminder& r) {
    if (!obj["id"].is<String>()) return false;
    r.id           = obj["id"].as<String>();
    r.type         = reminderTypeFromString(obj["type"] | "feed_evie");
    r.scheduled_at = fromISO8601(obj["scheduled_at"].as<String>());
    r.recurrence   = recurrenceFromString(obj["recurrence"] | "none");
    r.status       = statusFromString(obj["status"] | "pending");
    r.created_at   = fromISO8601(obj["created_at"].as<String>());
    return true;
}

// ── Static helpers ─────────────────────────────────────────────────────────────

String ReminderStore::statusToString(ReminderStatus s) {
    switch (s) {
        case ReminderStatus::Pending:   return "pending";
        case ReminderStatus::Active:    return "active";
        case ReminderStatus::Completed: return "completed";
        default: return "pending";
    }
}

ReminderStatus ReminderStore::statusFromString(const String& s) {
    if (s == "active")    return ReminderStatus::Active;
    if (s == "completed") return ReminderStatus::Completed;
    return ReminderStatus::Pending;
}

String ReminderStore::recurrenceToString(Recurrence r) {
    switch (r) {
        case Recurrence::Daily:    return "daily";
        case Recurrence::Weekly:   return "weekly";
        case Recurrence::Weekdays: return "weekdays";
        default: return "none";
    }
}

Recurrence ReminderStore::recurrenceFromString(const String& s) {
    if (s == "daily")    return Recurrence::Daily;
    if (s == "weekly")   return Recurrence::Weekly;
    if (s == "weekdays") return Recurrence::Weekdays;
    return Recurrence::None;
}

String ReminderStore::toISO8601(time_t t) {
    struct tm tm;
    gmtime_r(&t, &tm);
    char buf[25];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return String(buf);
}

time_t ReminderStore::fromISO8601(const String& s) {
    struct tm tm = {};
    // Accepts "2025-06-15T09:00:00Z" or "2025-06-15T09:00:00+02:00"
    int tz_offset_min = 0;
    int parsed = sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d",
        &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
        &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    if (parsed < 6) return 0;
    tm.tm_year -= 1900;
    tm.tm_mon  -= 1;

    // Parse timezone offset if present
    const char* tz = s.c_str();
    const char* plus  = strchr(tz + 10, '+');
    const char* minus = strrchr(tz + 10, '-');
    const char* z     = strchr(tz + 10, 'Z');
    if (!z) {
        const char* off = plus ? plus : minus;
        if (off) {
            int h, m;
            if (sscanf(off + 1, "%d:%d", &h, &m) == 2) {
                tz_offset_min = h * 60 + m;
                if (minus) tz_offset_min = -tz_offset_min;
            }
        }
    }

    time_t t = mktime(&tm);  // Treated as local; adjust below
    // mktime uses local TZ; we want UTC
    struct tm utc_tm = {};
    gmtime_r(&t, &utc_tm);
    time_t local_offset = (time_t)(mktime(&utc_tm) - t);  // seconds
    t -= local_offset;           // convert to UTC epoch
    t -= tz_offset_min * 60;     // apply input TZ offset
    return t;
}
