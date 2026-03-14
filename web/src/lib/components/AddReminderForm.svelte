<script lang="ts">
  import type { CreateReminderPayload, Recurrence, ReminderType } from '$lib/types';
  import { format } from 'date-fns';
  import TypePicker from './TypePicker.svelte';

  let { onSubmit, onCancel }: {
    onSubmit: (payload: CreateReminderPayload) => Promise<void>;
    onCancel: () => void;
  } = $props();

  // Default date/time: next full hour
  function defaultDateTime() {
    const d = new Date();
    d.setHours(d.getHours() + 1, 0, 0, 0);
    return d;
  }

  const defaultDt = defaultDateTime();

  let selectedType = $state<ReminderType | null>(null);
  let date         = $state(format(defaultDt, 'yyyy-MM-dd'));
  let time         = $state(format(defaultDt, 'HH:mm'));
  let recurrence   = $state<Recurrence>('none');
  let submitting   = $state(false);
  let error        = $state('');

  const canSubmit = $derived(selectedType !== null && !submitting);

  async function handleSubmit(e: Event) {
    e.preventDefault();
    if (!selectedType) return;
    error = '';
    submitting = true;
    try {
      const scheduled_at = new Date(`${date}T${time}:00`).toISOString();
      await onSubmit({ type: selectedType, scheduled_at, recurrence });
    } catch (err) {
      error = err instanceof Error ? err.message : 'Something went wrong';
      submitting = false;
    }
  }
</script>

<form class="form" onsubmit={handleSubmit}>
  <h2 class="title">New reminder</h2>

  <div class="field">
    <span class="label">What is it?</span>
    <TypePicker value={selectedType} onSelect={(t) => (selectedType = t)} />
  </div>

  <div class="row">
    <label class="field">
      <span class="label">Date</span>
      <input type="date" bind:value={date} required />
    </label>

    <label class="field">
      <span class="label">Time</span>
      <input type="time" bind:value={time} required />
    </label>
  </div>

  <label class="field">
    <span class="label">Repeat</span>
    <select bind:value={recurrence}>
      <option value="none">No repeat</option>
      <option value="daily">Daily</option>
      <option value="weekly">Weekly</option>
      <option value="weekdays">Weekdays (Mon–Fri)</option>
    </select>
  </label>

  {#if error}
    <p class="error-msg">{error}</p>
  {/if}

  <div class="actions">
    <button type="button" class="btn secondary" onclick={onCancel} disabled={submitting}>
      Cancel
    </button>
    <button type="submit" class="btn primary" disabled={!canSubmit}>
      {submitting ? 'Saving…' : 'Save reminder'}
    </button>
  </div>
</form>

<style>
  .form {
    display: flex;
    flex-direction: column;
    gap: 16px;
  }

  .title {
    font-size: 1.1rem;
    font-weight: 600;
    margin: 0;
  }

  .field {
    display: flex;
    flex-direction: column;
    gap: 6px;
    flex: 1;
  }

  .label {
    font-size: 0.8rem;
    font-weight: 500;
    color: var(--text-muted);
    text-transform: uppercase;
    letter-spacing: 0.04em;
  }

  input, select {
    padding: 10px 12px;
    border-radius: 8px;
    border: 1px solid var(--border);
    background: var(--bg);
    color: var(--text);
    font-size: 1rem;
    font-family: inherit;
    width: 100%;
    box-sizing: border-box;
    transition: border-color 0.15s;
  }

  input:focus, select:focus {
    outline: none;
    border-color: var(--accent);
  }

  .row {
    display: flex;
    gap: 12px;
  }

  .error-msg {
    color: var(--danger);
    font-size: 0.85rem;
    margin: 0;
  }

  .actions {
    display: flex;
    gap: 8px;
    justify-content: flex-end;
    margin-top: 4px;
  }

  .btn {
    padding: 10px 20px;
    border-radius: 8px;
    font-size: 0.95rem;
    font-family: inherit;
    font-weight: 500;
    cursor: pointer;
    border: none;
    transition: opacity 0.15s;
  }

  .btn:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }

  .primary {
    background: var(--accent);
    color: #fff;
  }

  .secondary {
    background: var(--surface);
    color: var(--text);
    border: 1px solid var(--border);
  }
</style>
