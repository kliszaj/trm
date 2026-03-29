import { readFileSync, writeFileSync, existsSync, mkdirSync } from 'fs';
import { dirname } from 'path';
import type { StoredReminder } from '$lib/types';

const DATA_PATH = '/app/data/reminders.json';
const DEV_DATA_PATH = './data/reminders.json';

function getDataPath(): string {
  return existsSync('/app/data') ? DATA_PATH : DEV_DATA_PATH;
}

let cache: StoredReminder[] | null = null;

function load(): StoredReminder[] {
  if (cache) return cache;
  const path = getDataPath();
  if (!existsSync(path)) {
    cache = [];
    return cache;
  }
  try {
    cache = JSON.parse(readFileSync(path, 'utf-8'));
  } catch {
    cache = [];
  }
  return cache!;
}

function save(): void {
  const path = getDataPath();
  const dir = dirname(path);
  if (!existsSync(dir)) mkdirSync(dir, { recursive: true });
  writeFileSync(path, JSON.stringify(load(), null, 2));
}

export function getAll(): StoredReminder[] {
  return load();
}

export function getById(id: string): StoredReminder | undefined {
  return load().find((r) => r.id === id);
}

export function add(reminder: StoredReminder): void {
  load().push(reminder);
  save();
}

export function remove(id: string): boolean {
  const reminders = load();
  const idx = reminders.findIndex((r) => r.id === id);
  if (idx === -1) return false;
  reminders.splice(idx, 1);
  save();
  return true;
}

export function update(id: string, fields: Partial<StoredReminder>): StoredReminder | null {
  const r = getById(id);
  if (!r) return null;
  Object.assign(r, fields);
  save();
  return r;
}

export function purgeOld(): void {
  const cutoff = new Date(Date.now() - 24 * 60 * 60 * 1000).toISOString();
  const reminders = load();
  const before = reminders.length;
  // Remove completed reminders dismissed more than 24h ago
  // Remove non-recurring pending/active reminders scheduled more than 24h ago
  for (let i = reminders.length - 1; i >= 0; i--) {
    const r = reminders[i];
    if (r.status === 'completed' && r.completed_at && r.completed_at < cutoff) {
      reminders.splice(i, 1);
    } else if (r.status !== 'completed' && r.recurrence === 'none' && r.scheduled_at < cutoff) {
      reminders.splice(i, 1);
    }
  }
  if (reminders.length !== before) save();
}

export function activateOverdue(): StoredReminder[] {
  const now = new Date().toISOString();
  const reminders = load();
  const activated: StoredReminder[] = [];
  for (const r of reminders) {
    if (r.status === 'pending' && r.scheduled_at <= now) {
      r.status = 'active';
      activated.push(r);
    }
  }
  if (activated.length > 0) save();
  return activated;
}
