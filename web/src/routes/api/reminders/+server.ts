import { json } from '@sveltejs/kit';
import type { RequestHandler } from './$types';
import type { StoredReminder, CreateReminderPayload, Reminder } from '$lib/types';
import * as store from '$lib/server/store';
import { nudgeEsp32 } from '$lib/server/esp32';
import { nextOccurrence } from '$lib/server/recurrence';
import { getTypeMeta } from '$lib/reminder-type-meta';
import { randomUUID } from 'crypto';

function toApiReminder(r: StoredReminder): Reminder {
  return { ...r, name: getTypeMeta(r.type).label };
}

export const GET: RequestHandler = async () => {
  // Activate any overdue reminders
  store.activateOverdue();

  const reminders = store.getAll().map(toApiReminder);

  // Sort: completed last, then by scheduled_at
  reminders.sort((a, b) => {
    const aDone = a.status === 'completed' ? 1 : 0;
    const bDone = b.status === 'completed' ? 1 : 0;
    if (aDone !== bDone) return aDone - bDone;
    return new Date(a.scheduled_at).getTime() - new Date(b.scheduled_at).getTime();
  });

  return json(reminders);
};

export const POST: RequestHandler = async ({ request }) => {
  const body: CreateReminderPayload = await request.json();

  if (!body.type || !body.scheduled_at || !body.recurrence) {
    return json({ error: 'Missing required fields' }, { status: 400 });
  }

  let scheduledAt = body.scheduled_at;

  // If recurring and scheduled in the past, advance to next occurrence
  if (body.recurrence !== 'none' && new Date(scheduledAt) <= new Date()) {
    scheduledAt = nextOccurrence(scheduledAt, body.recurrence);
  }

  const reminder: StoredReminder = {
    id: randomUUID(),
    type: body.type,
    scheduled_at: scheduledAt,
    recurrence: body.recurrence,
    status: 'pending',
    created_at: new Date().toISOString(),
  };

  store.add(reminder);
  nudgeEsp32();

  return json(toApiReminder(reminder), { status: 201 });
};
