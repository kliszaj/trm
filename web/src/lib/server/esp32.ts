import { env } from '$env/dynamic/private';

export async function nudgeEsp32(): Promise<void> {
  const url = env.ESP32_URL;
  if (!url) {
    console.warn('[esp32] ESP32_URL not set, skipping nudge');
    return;
  }

  try {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 2000);

    await fetch(`${url}/sync`, {
      method: 'POST',
      signal: controller.signal,
    });

    clearTimeout(timeout);
    console.log('[esp32] Nudge sent');
  } catch (err) {
    console.warn('[esp32] Nudge failed (ESP32 may be offline):', (err as Error).message);
  }
}
