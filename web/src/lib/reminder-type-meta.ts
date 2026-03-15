import type { ReminderType } from './types';

export interface ReminderTypeMeta {
  id: ReminderType;
  label: string;
  color: string;   // CSS color for the web UI
}

export const REMINDER_TYPES: ReminderTypeMeta[] = [
  { id: 'feed_evie',      label: 'Feed Evie',      color: '#EC4E89' },
  { id: 'water_plants',   label: 'Water plants',   color: '#F8B352' },
  { id: 'eat_vitamins',   label: 'Eat vitamins',   color: '#41AEFF' },
  { id: 'take_out_trash', label: 'Take out trash', color: '#7261F3' },
  { id: 'pay_bills',     label: 'Pay bills',      color: '#E94E51' },
];

export function getTypeMeta(id: ReminderType): ReminderTypeMeta {
  return REMINDER_TYPES.find((t) => t.id === id) ?? REMINDER_TYPES[0];
}
