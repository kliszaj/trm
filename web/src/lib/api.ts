import type { Reminder, CreateReminderPayload } from './types';

async function request<T>(path: string, options?: RequestInit): Promise<T> {
  const res = await fetch(path, {
    ...options,
    headers: { 'Content-Type': 'application/json', ...options?.headers },
  });
  if (!res.ok) {
    const text = await res.text().catch(() => '');
    throw new Error(`API error ${res.status}: ${text}`);
  }
  if (res.status === 204) return undefined as T;
  return res.json();
}

export const api = {
  getReminders(): Promise<Reminder[]> {
    return request<Reminder[]>('/api/reminders');
  },

  createReminder(payload: CreateReminderPayload): Promise<Reminder> {
    return request<Reminder>('/api/reminders', {
      method: 'POST',
      body: JSON.stringify(payload),
    });
  },

  deleteReminder(id: string): Promise<void> {
    return request<void>(`/api/reminders/${id}`, { method: 'DELETE' });
  },

  dismissReminder(id: string): Promise<void> {
    return request<void>(`/api/reminders/${id}/dismiss`, { method: 'POST' });
  },
};
