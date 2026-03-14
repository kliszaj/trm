import { mockApi } from './mock-api';
import type { Reminder, CreateReminderPayload, ReminderStatus } from './types';

const USE_MOCK = import.meta.env.VITE_USE_MOCK === 'true';
const BASE_URL = (import.meta.env.VITE_ESP32_URL ?? 'http://trm.local').replace(/\/$/, '');

async function request<T>(path: string, options?: RequestInit): Promise<T> {
  const res = await fetch(`${BASE_URL}${path}`, {
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
    if (USE_MOCK) return mockApi.getReminders();
    return request<Reminder[]>('/api/reminders');
  },

  createReminder(payload: CreateReminderPayload): Promise<Reminder> {
    if (USE_MOCK) return mockApi.createReminder(payload);
    return request<Reminder>('/api/reminders', {
      method: 'POST',
      body: JSON.stringify(payload),
    });
  },

  deleteReminder(id: string): Promise<void> {
    if (USE_MOCK) return mockApi.deleteReminder(id);
    return request<void>(`/api/reminders/${id}`, { method: 'DELETE' });
  },

  patchReminder(id: string, status: ReminderStatus): Promise<Reminder> {
    if (USE_MOCK) return mockApi.patchReminder(id, status);
    return request<Reminder>(`/api/reminders/${id}`, {
      method: 'PATCH',
      body: JSON.stringify({ status }),
    });
  },
};
