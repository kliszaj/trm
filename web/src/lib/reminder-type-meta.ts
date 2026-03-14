import type { ReminderType } from './types';

export interface ReminderTypeMeta {
  id: ReminderType;
  label: string;
  color: string;   // CSS color for the web UI
}

export const REMINDER_TYPES: ReminderTypeMeta[] = [
  { id: 'feed_evie',      label: 'Feed Evie',      color: '#E91D8F' },
  { id: 'water_plants',   label: 'Water plants',   color: '#F5A623' },
  { id: 'eat_vitamins',   label: 'Eat vitamins',   color: '#4BAEE4' },
  { id: 'take_out_trash', label: 'Take out trash', color: '#7B6CC6' },
];

export function getTypeMeta(id: ReminderType): ReminderTypeMeta {
  return REMINDER_TYPES.find((t) => t.id === id) ?? REMINDER_TYPES[0];
}
