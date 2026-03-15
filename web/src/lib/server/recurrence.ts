import type { Recurrence } from '$lib/types';

export function nextOccurrence(scheduledAt: string, recurrence: Recurrence): string {
  const now = new Date();
  let next = new Date(scheduledAt);

  switch (recurrence) {
    case 'daily':
      while (next <= now) {
        next.setDate(next.getDate() + 1);
      }
      break;

    case 'weekly':
      while (next <= now) {
        next.setDate(next.getDate() + 7);
      }
      break;

    case 'weekdays':
      while (next <= now) {
        next.setDate(next.getDate() + 1);
        // Skip Saturday (6) and Sunday (0)
        while (next.getDay() === 0 || next.getDay() === 6) {
          next.setDate(next.getDate() + 1);
        }
      }
      break;

    default:
      break;
  }

  return next.toISOString();
}
