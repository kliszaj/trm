<script lang="ts">
  import { format, isToday, isTomorrow } from 'date-fns';
  import type { Reminder } from '$lib/types';
  import RecurrenceBadge from './RecurrenceBadge.svelte';

  let {
    reminder,
    onDelete,
  }: {
    reminder: Reminder;
    onDelete: (id: string) => void;
  } = $props();

  const scheduled = $derived(new Date(reminder.scheduled_at));

  const timeLabel = $derived(
    isToday(scheduled)
      ? `Today, ${format(scheduled, 'h:mm a')}`
      : isTomorrow(scheduled)
        ? `Tomorrow, ${format(scheduled, 'h:mm a')}`
        : format(scheduled, "EEE d MMM, h:mm a")
  );

  let deleting = $state(false);

  async function handleDelete() {
    deleting = true;
    onDelete(reminder.id);
  }
</script>

<article class="card" class:active={reminder.status === 'active'}>
  <div class="main">
    <span class="name">{reminder.name}</span>
    <div class="meta">
      <span class="time">{timeLabel}</span>
      <RecurrenceBadge recurrence={reminder.recurrence} />
    </div>
  </div>
  <button
    class="delete"
    onclick={handleDelete}
    disabled={deleting}
    aria-label="Delete reminder"
  >
    <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <polyline points="3 6 5 6 21 6"></polyline>
      <path d="M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"></path>
      <path d="M10 11v6M14 11v6"></path>
      <path d="M9 6V4a1 1 0 0 1 1-1h4a1 1 0 0 1 1 1v2"></path>
    </svg>
  </button>
</article>

<style>
  .card {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 14px 16px;
    border-radius: 12px;
    background: var(--surface);
    border: 1px solid var(--border);
    transition: border-color 0.2s;
  }

  .card.active {
    border-color: var(--accent);
    background: color-mix(in srgb, var(--accent) 8%, var(--surface));
  }

  .main {
    flex: 1;
    min-width: 0;
    display: flex;
    flex-direction: column;
    gap: 4px;
  }

  .name {
    font-size: 1rem;
    font-weight: 500;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  .meta {
    display: flex;
    align-items: center;
    gap: 8px;
    flex-wrap: wrap;
  }

  .time {
    font-size: 0.8rem;
    color: var(--text-muted);
  }

  .delete {
    flex-shrink: 0;
    background: none;
    border: none;
    cursor: pointer;
    color: var(--text-muted);
    padding: 6px;
    border-radius: 6px;
    display: flex;
    align-items: center;
    justify-content: center;
    transition: color 0.15s, background 0.15s;
  }

  .delete:hover {
    color: var(--danger);
    background: color-mix(in srgb, var(--danger) 10%, transparent);
  }

  .delete:disabled {
    opacity: 0.4;
    cursor: not-allowed;
  }
</style>
