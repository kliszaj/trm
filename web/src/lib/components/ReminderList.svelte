<script lang="ts">
  import type { Reminder } from '$lib/types';
  import ReminderCard from './ReminderCard.svelte';

  let {
    reminders,
    loading,
    onDelete,
  }: {
    reminders: Reminder[];
    loading: boolean;
    onDelete: (id: string) => void;
  } = $props();

  const sorted = $derived(
    [...reminders].sort((a, b) => {
      // Completed always sink to the bottom
      const aDone = a.status === 'completed' ? 1 : 0;
      const bDone = b.status === 'completed' ? 1 : 0;
      if (aDone !== bDone) return aDone - bDone;
      return new Date(a.scheduled_at).getTime() - new Date(b.scheduled_at).getTime();
    })
  );
</script>

<div class="list">
  {#if loading}
    <div class="empty">Loading…</div>
  {:else if sorted.length === 0}
    <div class="empty">No upcoming reminders.</div>
  {:else}
    {#each sorted as reminder (reminder.id)}
      <ReminderCard {reminder} {onDelete} />
    {/each}
  {/if}
</div>

<style>
  .list {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }

  .empty {
    text-align: center;
    color: var(--text-muted);
    padding: 48px 0;
    font-size: 0.9rem;
  }
</style>
