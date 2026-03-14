#pragma once
#include "reminder_store.h"

// Returns the next scheduled_at epoch for a recurring reminder,
// calculated from the original scheduled time (not from now).
time_t nextOccurrence(time_t original_scheduled_at, Recurrence r);
