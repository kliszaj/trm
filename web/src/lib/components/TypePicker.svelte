<script lang="ts">
  import { REMINDER_TYPES } from '$lib/reminder-type-meta';
  import type { ReminderType } from '$lib/types';

  let {
    value,
    onSelect,
  }: {
    value: ReminderType | null;
    onSelect: (type: ReminderType) => void;
  } = $props();
</script>

<div class="picker">
  {#each REMINDER_TYPES as t (t.id)}
    <button
      type="button"
      class="tile"
      class:selected={value === t.id}
      style="--type-color: {t.color}"
      onclick={() => onSelect(t.id)}
    >
      <span class="dot"></span>
      <span class="name">{t.label}</span>
    </button>
  {/each}
</div>

<style>
  .picker {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 8px;
  }

  .tile {
    display: flex;
    align-items: center;
    gap: 10px;
    padding: 12px 14px;
    border-radius: 10px;
    border: 2px solid var(--border);
    background: var(--surface);
    color: var(--text);
    font-family: inherit;
    font-size: 0.9rem;
    font-weight: 500;
    cursor: pointer;
    text-align: left;
    transition: border-color 0.15s, background 0.15s;
  }

  .tile:hover {
    border-color: var(--type-color);
  }

  .tile.selected {
    border-color: var(--type-color);
    background: color-mix(in srgb, var(--type-color) 12%, var(--surface));
  }

  .dot {
    flex-shrink: 0;
    width: 12px;
    height: 12px;
    border-radius: 50%;
    background: var(--type-color);
  }

  .name {
    line-height: 1.2;
  }
</style>
