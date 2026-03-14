#include "time_manager.h"
#include "config.h"

void timeSyncInit() {
    configTzTime(TZ_STRING, NTP_SERVER);
    Serial.print("[time] Waiting for NTP sync");
    struct tm tm;
    int tries = 0;
    while (!getLocalTime(&tm, 1000) && tries++ < 20) {
        Serial.print(".");
    }
    if (tries < 20) {
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        Serial.printf("\n[time] Synced: %s (Stockholm)\n", buf);
    } else {
        Serial.println("\n[time] NTP sync failed — time may be wrong");
    }
}

bool isTimeSynced() {
    time_t now = time(nullptr);
    return now > 1700000000UL;  // After Nov 2023 = synced
}

String currentTimeISO8601() {
    time_t now = time(nullptr);
    struct tm tm;
    gmtime_r(&now, &tm);
    char buf[25];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return String(buf);
}
