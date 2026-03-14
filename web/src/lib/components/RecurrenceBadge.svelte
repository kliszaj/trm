<script lang="ts">
  import type { Recurrence } from '$lib/types';

  let { recurrence }: { recurrence: Recurrence } = $props();

  const labels: Record<Recurrence, string | null> = {
    none: null,
    daily: 'Daily',
    weekly: 'Weekly',
    weekdays: 'Weekdays',
  };

  const label = $derived(labels[recurrence]);
</script>

{#if label}
  <span class="badge">{label}</span>
{/if}

<style>
  .badge {
    display: inline-block;
    padding: 2px 8px;
    border-radius: 999px;
    font-size: 0.7rem;
    font-weight: 600;
    letter-spacing: 0.04em;
    text-transform: uppercase;
    background: color-mix(in srgb, var(--accent) 15%, transparent);
    color: var(--accent);
    border: 1px solid color-mix(in srgb, var(--accent) 30%, transparent);
  }
</style>
