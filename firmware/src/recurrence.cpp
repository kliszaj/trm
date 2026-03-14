#include "recurrence.h"
#include <time.h>

time_t nextOccurrence(time_t original_scheduled_at, Recurrence r) {
    time_t now = time(nullptr);
    time_t next = original_scheduled_at;

    switch (r) {
        case Recurrence::Daily:
            while (next <= now) next += 86400;
            break;

        case Recurrence::Weekly:
            while (next <= now) next += 7 * 86400;
            break;

        case Recurrence::Weekdays: {
            while (next <= now) {
                next += 86400;
                struct tm tm;
                localtime_r(&next, &tm);
                // Skip Saturday (6) and Sunday (0)
                while (tm.tm_wday == 0 || tm.tm_wday == 6) {
                    next += 86400;
                    localtime_r(&next, &tm);
                }
            }
            break;
        }

        default:
            break;
    }
    return next;
}
