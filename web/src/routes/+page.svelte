<script lang="ts">
  import { onMount, onDestroy } from 'svelte';
  import { api } from '$lib/api';
  import type { Reminder } from '$lib/types';
  import ReminderList from '$lib/components/ReminderList.svelte';
  import AddReminderForm from '$lib/components/AddReminderForm.svelte';

  let reminders = $state<Reminder[]>([]);
  let loading = $state(true);
  let showForm = $state(false);
  let pollTimer: ReturnType<typeof setInterval>;

  async function fetchReminders() {
    try {
      reminders = await api.getReminders();
    } catch (e) {
      console.error('Failed to fetch reminders:', e);
    } finally {
      loading = false;
    }
  }

  onMount(() => {
    fetchReminders();
    pollTimer = setInterval(fetchReminders, 30_000);
  });

  onDestroy(() => clearInterval(pollTimer));

  async function handleCreate(payload: Parameters<typeof api.createReminder>[0]) {
    await api.createReminder(payload);
    showForm = false;
    await fetchReminders();
  }

  async function handleDelete(id: string) {
    await api.deleteReminder(id);
    reminders = reminders.filter((r) => r.id !== id);
  }
</script>

<svelte:head>
  <title>that reminds me…</title>
</svelte:head>

<div class="app">
  <header class="header">
    <h1 class="wordmark">that reminds me…</h1>
    {#if !showForm}
      <button class="btn-add" onclick={() => (showForm = true)}>
        <span class="plus">+</span> Add reminder
      </button>
    {/if}
  </header>

  <main class="content">
    {#if showForm}
      <div class="form-wrap">
        <AddReminderForm
          onSubmit={handleCreate}
          onCancel={() => (showForm = false)}
        />
      </div>
    {:else}
      <ReminderList {reminders} {loading} onDelete={handleDelete} />
    {/if}
  </main>
</div>

<style>
  .app {
    max-width: 480px;
    margin: 0 auto;
    padding: 0 16px 40px;
    min-height: 100dvh;
  }

  .header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 24px 0 20px;
    gap: 12px;
  }

  .wordmark {
    font-size: 1.15rem;
    font-weight: 600;
    margin: 0;
    letter-spacing: -0.01em;
  }

  .btn-add {
    display: flex;
    align-items: center;
    gap: 6px;
    padding: 8px 16px;
    border-radius: 999px;
    background: var(--accent);
    color: #fff;
    border: none;
    font-size: 0.9rem;
    font-weight: 500;
    font-family: inherit;
    cursor: pointer;
    white-space: nowrap;
    transition: opacity 0.15s;
  }

  .btn-add:hover {
    opacity: 0.85;
  }

  .plus {
    font-size: 1.1rem;
    line-height: 1;
  }

  .content {
    /* push content below header */
  }

  .form-wrap {
    padding: 8px 0;
  }
</style>
