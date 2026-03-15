import { json } from '@sveltejs/kit';
import type { RequestHandler } from './$types';
import type { StoredReminder, Reminder } from '$lib/types';
import * as store from '$lib/server/store';
import { getTypeMeta } from '$lib/reminder-type-meta';

function toApiReminder(r: StoredReminder): Reminder {
  return { ...r, name: getTypeMeta(r.type).label };
}

export const GET: RequestHandler = async () => {
  // Activate any overdue pending reminders
  store.activateOverdue();

  const active = store.getAll()
    .filter((r) => r.status === 'active')
    .sort((a, b) => new Date(a.scheduled_at).getTime() - new Date(b.scheduled_at).getTime())
    .map(toApiReminder);

  return json(active);
};
