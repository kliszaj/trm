import { v4 as uuidv4 } from 'uuid';
import type { Reminder, CreateReminderPayload, ReminderStatus } from './types';

// Seed with a couple of sample reminders
const store: Reminder[] = [
  {
    id: uuidv4(),
    name: 'Take out the trash',
    scheduled_at: new Date(Date.now() + 2 * 60 * 60 * 1000).toISOString(),
    type: 'generic',
    recurrence: 'weekly',
    status: 'pending',
    created_at: new Date().toISOString(),
  },
  {
    id: uuidv4(),
    name: 'Call the dentist',
    scheduled_at: new Date(Date.now() + 26 * 60 * 60 * 1000).toISOString(),
    type: 'generic',
    recurrence: 'none',
    status: 'pending',
    created_at: new Date().toISOString(),
  },
];

const delay = (ms = 150) => new Promise((r) => setTimeout(r, ms));

export const mockApi = {
  async getReminders(): Promise<Reminder[]> {
    await delay();
    return [...store].filter((r) => r.status !== 'completed');
  },

  async createReminder(payload: CreateReminderPayload): Promise<Reminder> {
    await delay();
    const r: Reminder = {
      id: uuidv4(),
      name: payload.name,
      scheduled_at: payload.scheduled_at,
      type: 'generic',
      recurrence: payload.recurrence,
      status: 'pending',
      created_at: new Date().toISOString(),
    };
    store.push(r);
    return r;
  },

  async deleteReminder(id: string): Promise<void> {
    await delay();
    const idx = store.findIndex((r) => r.id === id);
    if (idx !== -1) store.splice(idx, 1);
  },

  async patchReminder(id: string, status: ReminderStatus): Promise<Reminder> {
    await delay();
    const r = store.find((r) => r.id === id);
    if (!r) throw new Error('Not found');
    r.status = status;
    return { ...r };
  },
};
