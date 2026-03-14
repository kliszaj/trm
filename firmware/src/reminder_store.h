#pragma once
#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>

enum class ReminderStatus { Pending, Active, Completed };
enum class Recurrence { None, Daily, Weekly, Weekdays };

struct Reminder {
    String id;
    String name;
    time_t scheduled_at;   // UTC epoch
    Recurrence recurrence;
    ReminderStatus status;
    time_t created_at;     // UTC epoch
};

class ReminderStore {
public:
    bool begin();                                      // Mount LittleFS, load from file
    const std::vector<Reminder>& getAll() const;
    bool add(const Reminder& r);
    bool remove(const String& id);
    bool updateStatus(const String& id, ReminderStatus status);
    Reminder* findById(const String& id);

    // Serialization helpers
    static String statusToString(ReminderStatus s);
    static ReminderStatus statusFromString(const String& s);
    static String recurrenceToString(Recurrence r);
    static Recurrence recurrenceFromString(const String& s);
    static String toISO8601(time_t t);
    static time_t fromISO8601(const String& s);

private:
    std::vector<Reminder> _reminders;
    bool save();
    void serializeReminder(JsonObject& obj, const Reminder& r);
    bool deserializeReminder(JsonObject& obj, Reminder& r);
};

extern ReminderStore reminderStore;
