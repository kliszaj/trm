import { json } from '@sveltejs/kit';
import type { RequestHandler } from './$types';
import type { StoredReminder } from '$lib/types';
import * as store from '$lib/server/store';
import { nudgeEsp32 } from '$lib/server/esp32';
import { nextOccurrence } from '$lib/server/recurrence';
import { randomUUID } from 'crypto';

export const POST: RequestHandler = async ({ params }) => {
  const reminder = store.getById(params.id);
  if (!reminder) {
    return json({ error: 'Not found' }, { status: 404 });
  }

  // Mark as completed
  store.update(params.id, { status: 'completed' });

  // If recurring, create next occurrence
  if (reminder.recurrence !== 'none') {
    const next: StoredReminder = {
      id: randomUUID(),
      type: reminder.type,
      scheduled_at: nextOccurrence(reminder.scheduled_at, reminder.recurrence),
      recurrence: reminder.recurrence,
      status: 'pending',
      created_at: new Date().toISOString(),
    };
    store.add(next);
  }

  nudgeEsp32();

  return json({ dismissed: params.id });
};
