# Design: Web App as Source of Truth

**Date:** 2026-03-15
**Status:** Draft

## Problem

The ESP32 currently serves as both the API server and data store. This means:
- Reminders are lost if the ESP32 is reflashed or reset
- No persistence across firmware updates
- Can't manage reminders when the ESP32 is offline
- All logic (recurrence, firing, CRUD) is in constrained firmware

## Solution

Move the source of truth to the web app running on Unraid. The ESP32 becomes a thin display + touch client.

## Architecture

```
Browser (phone/laptop)
        |  HTTP
        v
  SvelteKit server          <-- Unraid Docker (adapter-node)
    |           |
  JSON file     |  HTTP push (POST /sync) + ESP32 polls every 5 min
  (volume)      v
            ESP32-C3        <-- display + touch only (no local storage)
```

### Web App (Unraid)

- **Runtime**: SvelteKit with `adapter-node` (replaces `adapter-static`)
- **Storage**: JSON file at `/app/data/reminders.json`, persisted via Docker volume
- **Responsibilities**: All CRUD, recurrence scheduling, status transitions, firing detection
- **Push**: After any mutation, sends `POST http://<ESP32_IP>/sync` to nudge the device (fire-and-forget with 2s timeout; failures are logged but do not affect the API response)

### ESP32 (Thin Client)

- **No local REST API** (api_server removed)
- **No local reminder store** (reminder_store removed)
- **No recurrence logic** (recurrence removed)
- **Keeps**: Display, touch, WiFi, NTP, screensaver, type definitions (colors/images)
- **New**: HTTP client to fetch from web app + minimal `/sync` endpoint to receive nudges

## Web App API Routes

Server-side SvelteKit API routes:

| Method | Path | Description |
|---|---|---|
| `GET` | `/api/reminders` | List all reminders (includes completed for web UI) |
| `POST` | `/api/reminders` | Create a reminder |
| `DELETE` | `/api/reminders/[id]` | Delete a reminder |
| `POST` | `/api/reminders/[id]/dismiss` | Mark completed, schedule next if recurring |
| `GET` | `/api/reminders/active` | Returns currently firing reminders (scheduled_at <= now) |

All endpoints derive `name` from `type` when building JSON responses (not stored in the file).

CORS is not needed: the browser talks same-origin, and the ESP32's Arduino HTTP client does not enforce CORS.

### Create Behavior

- Accepts `{ type, scheduled_at, recurrence }`
- Generates UUID server-side
- If `recurrence != 'none'` and `scheduled_at` is in the past, advances to next occurrence
- Persists to JSON file
- Nudges ESP32 via `POST /sync` (fire-and-forget)
- Returns created reminder

### Dismiss Behavior

- Marks reminder as `completed`
- If recurring: creates new `pending` reminder with next occurrence, calculated by advancing from the dismissed reminder's `scheduled_at` until the result is in the future. The new reminder's `scheduled_at` is set to that future time.
- Persists to JSON file
- Nudges ESP32 via `POST /sync` (fire-and-forget)
- Returns updated state

### Active Endpoint Behavior

This endpoint is called by the ESP32 (on boot, on sync, every 5 min). It:

- Scans all `pending` reminders where `scheduled_at <= now`
- Transitions matching reminders to `active` status (persists change)
- Returns all `active` reminders sorted by `scheduled_at`

Additionally, `GET /api/reminders` also performs this pending-to-active transition check before returning results. This ensures the web UI always shows correct statuses regardless of whether the ESP32 is polling.

### ESP32 Push (Nudge)

After any mutation (create, delete, dismiss), the server sends `POST http://<ESP32_IP>/sync`:
- **Timeout**: 2 seconds
- **On failure**: Log warning, do not retry, do not affect the API response
- **Rationale**: The ESP32 also polls every 5 minutes, so missed pushes self-heal

## ESP32 Endpoints

| Method | Path | Description |
|---|---|---|
| `POST` | `/sync` | Nudge: triggers ESP32 to fetch latest from web app |

Single endpoint. No request body needed. Returns 200 OK. No authentication (low risk — worst case triggers a re-fetch).

## ESP32 Behavior

### On Boot
1. Connect WiFi, sync NTP
2. Show boot screen
3. Fetch `GET <WEB_APP_URL>/api/reminders/active` from web app
4. If active reminders exist: show active screen
5. Else fetch `GET <WEB_APP_URL>/api/reminders` for next pending, show idle or screensaver

### Every 5 Minutes
- Same fetch cycle as boot step 3-5
- Safety net for missed pushes

### On `/sync` Received
- Same fetch cycle as boot step 3-5
- Provides instant display update after web app mutations

### On Tap (Dismiss)
1. Show checkmark immediately (visual feedback)
2. Send `POST <WEB_APP_URL>/api/reminders/<id>/dismiss`
3. If request fails: log error, continue to next step (dismiss is best-effort; the 5-min poll will correct state)
4. Fetch latest state and update display (next active, idle, or screensaver)

### Screensaver
- No change from current behavior
- Activates after 2 min idle, or when no reminders exist
- Tap exits to idle

## Data Model

### Reminder (JSON file)

```json
{
  "id": "string (UUID)",
  "type": "feed_evie | water_plants | eat_vitamins | take_out_trash",
  "scheduled_at": "ISO 8601 string",
  "recurrence": "none | daily | weekly | weekdays",
  "status": "pending | active | completed",
  "created_at": "ISO 8601 string"
}
```

The `name` field is NOT stored. It is derived from `type` when building API responses using the type metadata lookup.

### Status Transitions

- `pending` -> `active`: server-side, when `scheduled_at <= now` (checked on GET /api/reminders and GET /api/reminders/active)
- `active` -> `completed`: via `POST /api/reminders/[id]/dismiss`
- Completed reminders with recurrence spawn a new `pending` reminder

### Completed Reminder Retention

Completed reminders are kept in the JSON file for web UI display (shown with visual distinction). No automatic cleanup in v1. At ~4 daily reminders this produces ~1,460 records/year — acceptable for a JSON file.

## Recurrence Logic

Moves from firmware (`recurrence.cpp`) to web app server-side code. Same algorithm:

- `daily`: add 24h until future
- `weekly`: add 7 days until future
- `weekdays`: add 1 day, skip Saturday/Sunday, until future
- Calculated by advancing from the dismissed reminder's `scheduled_at` (not the dismissal time, and not some separately-stored "original" time)

## Docker Configuration

### docker-compose.yml

```yaml
services:
  trm-web:
    image: kliszaj/trm-web:latest
    container_name: trm-web
    restart: unless-stopped
    ports:
      - "8082:3000"
    volumes:
      - /mnt/user/appdata/trm/data:/app/data
    environment:
      - ESP32_URL=http://10.0.0.111
      - TZ=Europe/Stockholm
```

### Key Changes from Current Docker Setup

- **Base image**: Node.js runtime (not nginx static)
- **Port**: 3000 (SvelteKit node server) instead of 8080 (nginx)
- **Volume**: `/app/data` for persistent JSON storage
- **Environment**: `ESP32_URL` is a runtime env var (not build-time `VITE_ESP32_URL`). `TZ` is set to Stockholm for correct weekday calculations in recurrence logic.
- **No nginx**: SvelteKit node adapter serves both the app and API

## JSON File Concurrency

SvelteKit with `adapter-node` runs in a single Node.js process. JavaScript execution is single-threaded, but async read-modify-write cycles can interleave. The store module (`store.ts`) will:

- Keep an in-memory cache of the reminders array
- Load from disk on startup
- Write to disk after every mutation (async, non-blocking)
- All mutations go through the in-memory cache (no interleaving risk since JS is single-threaded between awaits, and all cache reads/writes are synchronous)

## Firmware Changes

### Removed
- `api_server.cpp / .h` — no local REST API
- `reminder_store.cpp / .h` — no local JSON persistence
- `recurrence.cpp / .h` — moves to web app

### Modified
- `main.cpp` — new sync loop (fetch from web app instead of local store check)
- `config.h` — add `WEB_APP_URL` define, change `POLL_INTERVAL_MS` from 30s to 300s (5 min)

### Added
- HTTP client logic — fetch reminders from web app, send dismiss requests
- Minimal WebServer with single `/sync` POST endpoint

### Unchanged
- `display_manager.*` — same display logic
- `touch_manager.*` — same touch handling
- `wifi_manager.*` — same WiFi connection
- `time_manager.*` — same NTP sync
- `reminder_types.h` — same type definitions (colors, images, labels)
- All LittleFS assets (fonts, images, animation) — unchanged

## Web App Changes

### SvelteKit Config
- Switch from `adapter-static` to `adapter-node`
- Remove nginx config (node serves directly)

### New Server Routes
- `src/routes/api/reminders/+server.ts` — GET, POST
- `src/routes/api/reminders/active/+server.ts` — GET (static route, takes precedence over [id])
- `src/routes/api/reminders/[id]/+server.ts` — DELETE
- `src/routes/api/reminders/[id]/dismiss/+server.ts` — POST

### New Server Modules
- `src/lib/server/store.ts` — in-memory cache + JSON file persistence
- `src/lib/server/recurrence.ts` — next occurrence calculation
- `src/lib/server/esp32.ts` — push notification helper (POST to ESP32 /sync)

### Client Changes
- `api.ts` — base URL changes to relative paths (same origin), remove `patchReminder` (replaced by dismiss endpoint), remove mock toggle
- Remove `VITE_ESP32_URL` and `VITE_USE_MOCK` environment variables
- Remove `mock-api.ts` (server handles everything now)

### Dockerfile
- Replace nginx stage with node runtime
- Add volume mount point
- Expose port 3000

## Configuration

| Setting | Location | Value |
|---|---|---|
| ESP32 URL | Docker env `ESP32_URL` | `http://10.0.0.111` |
| Timezone | Docker env `TZ` | `Europe/Stockholm` |
| Web app URL | Firmware `config.h` | `http://<unraid-ip>:8082` |
| Data path | Docker volume | `/mnt/user/appdata/trm/data:/app/data` |
| WiFi | Firmware `config.h` | SSID + password |
| Timezone | Firmware `config.h` | Stockholm CET/CEST |
| Sync interval | Firmware `config.h` | 300000ms (5 min) |
