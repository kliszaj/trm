export type ReminderStatus = 'pending' | 'active' | 'completed';
export type Recurrence = 'none' | 'daily' | 'weekly' | 'weekdays';
export type ReminderType = 'feed_evie' | 'water_plants' | 'eat_vitamins' | 'take_out_trash' | 'pay_bills';

export interface Reminder {
  id: string;
  name: string;           // derived from type on the device, returned for display
  scheduled_at: string;   // ISO 8601
  type: ReminderType;
  recurrence: Recurrence;
  status: ReminderStatus;
  created_at: string;     // ISO 8601
  completed_at?: string;  // ISO 8601, set when dismissed
}

export interface StoredReminder {
  id: string;
  type: ReminderType;
  scheduled_at: string;   // ISO 8601
  recurrence: Recurrence;
  status: ReminderStatus;
  created_at: string;     // ISO 8601
  completed_at?: string;  // ISO 8601, set when dismissed
}

export interface CreateReminderPayload {
  type: ReminderType;
  scheduled_at: string;
  recurrence: Recurrence;
}
