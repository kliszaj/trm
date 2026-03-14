export type ReminderStatus = 'pending' | 'active' | 'completed';
export type Recurrence = 'none' | 'daily' | 'weekly' | 'weekdays';

export interface Reminder {
  id: string;
  name: string;
  scheduled_at: string;   // ISO 8601
  type: 'generic';
  recurrence: Recurrence;
  status: ReminderStatus;
  created_at: string;     // ISO 8601
}

export interface CreateReminderPayload {
  name: string;
  scheduled_at: string;
  recurrence: Recurrence;
}
