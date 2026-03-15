# Web App as Source of Truth — Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move reminder storage and business logic from ESP32 firmware to the SvelteKit web app, making the ESP32 a thin display + touch client.

**Architecture:** SvelteKit switches from `adapter-static` to `adapter-node` with server-side API routes. Reminders are stored in a JSON file on a Docker volume. The ESP32 fetches reminders from the web app and receives push notifications via a `/sync` endpoint. All CRUD, recurrence, and status transition logic lives server-side.

**Tech Stack:** SvelteKit (adapter-node), TypeScript, Node.js, ArduinoJson, ESP32 HTTPClient

**Spec:** `docs/superpowers/specs/2026-03-15-web-app-source-of-truth-design.md`

---

## File Structure

### Web App — New/Modified Files

| Action | File | Responsibility |
|---|---|---|
| Create | `src/lib/server/store.ts` | In-memory reminder cache + JSON file persistence |
| Create | `src/lib/server/recurrence.ts` | Next occurrence calculation |
| Create | `src/lib/server/esp32.ts` | Fire-and-forget POST to ESP32 /sync |
| Create | `src/routes/api/reminders/+server.ts` | GET (list) + POST (create) |
| Create | `src/routes/api/reminders/active/+server.ts` | GET (currently firing reminders) |
| Create | `src/routes/api/reminders/[id]/+server.ts` | DELETE |
| Create | `src/routes/api/reminders/[id]/dismiss/+server.ts` | POST (complete + recurrence) |
| Modify | `src/lib/api.ts` | Switch to relative URLs, remove mock, remove patchReminder |
| Modify | `src/lib/types.ts` | Add server-side StoredReminder (no name field) |
| Modify | `svelte.config.js` | Switch to adapter-node |
| Modify | `package.json` | Replace adapter-static with adapter-node, add uuid |
| Modify | `Dockerfile` | Replace nginx with node runtime |
| Modify | `docker-compose.yml` | Add volume, env vars, new port |
| Delete | `src/lib/mock-api.ts` | No longer needed |
| Delete | `nginx.conf` | No longer needed |

### Firmware — New/Modified/Deleted Files

| Action | File | Responsibility |
|---|---|---|
| Create | `src/sync_client.cpp` | HTTP client: fetch reminders, send dismiss, receive /sync |
| Create | `src/sync_client.h` | Header for sync client |
| Modify | `src/main.cpp` | New sync-based loop replacing local store logic |
| Modify | `src/config.h` | Add WEB_APP_URL, change poll interval to 5 min |
| Modify | `src/display_manager.h` | Update to accept JSON data instead of Reminder struct |
| Delete | `src/api_server.cpp` | No longer needed |
| Delete | `src/api_server.h` | No longer needed |
| Delete | `src/reminder_store.cpp` | No longer needed |
| Delete | `src/reminder_store.h` | No longer needed |
| Delete | `src/recurrence.cpp` | Moves to web app |
| Delete | `src/recurrence.h` | Moves to web app |

---

## Chunk 1: Web App Server-Side Backend

### Task 1: Switch SvelteKit to adapter-node

**Files:**
- Modify: `web/svelte.config.js`
- Modify: `web/package.json`

- [ ] **Step 1: Install adapter-node, remove adapter-static**

```bash
cd web
npm uninstall @sveltejs/adapter-static
npm install @sveltejs/adapter-node
```

- [ ] **Step 2: Update svelte.config.js**

Replace the adapter import and config:

```js
import adapter from '@sveltejs/adapter-node';
import { vitePreprocess } from '@sveltejs/vite-plugin-svelte';

/** @type {import('@sveltejs/kit').Config} */
const config = {
  preprocess: vitePreprocess(),
  compilerOptions: {
    runes: true,
  },
  kit: {
    adapter: adapter({
      out: 'build',
    }),
  },
};

export default config;
```

- [ ] **Step 3: Verify build works**

```bash
cd web && npm run build
```

Expected: Builds successfully with node adapter output in `build/`.

- [ ] **Step 4: Commit**

```bash
git add web/svelte.config.js web/package.json web/package-lock.json
git commit -m "Switch SvelteKit from adapter-static to adapter-node"
```

---

### Task 2: Create the reminder store module

**Files:**
- Create: `web/src/lib/server/store.ts`
- Modify: `web/src/lib/types.ts`

- [ ] **Step 1: Add StoredReminder type to types.ts**

Add a type for what's stored in the JSON file (no `name` field):

```typescript
export interface StoredReminder {
  id: string;
  type: ReminderType;
  scheduled_at: string;   // ISO 8601
  recurrence: Recurrence;
  status: ReminderStatus;
  created_at: string;     // ISO 8601
}
```

- [ ] **Step 2: Create store.ts**

```typescript
import { readFileSync, writeFileSync, existsSync, mkdirSync } from 'fs';
import { dirname } from 'path';
import type { StoredReminder } from '$lib/types';

const DATA_PATH = '/app/data/reminders.json';
const DEV_DATA_PATH = './data/reminders.json';

function getDataPath(): string {
  return existsSync('/app/data') ? DATA_PATH : DEV_DATA_PATH;
}

let cache: StoredReminder[] | null = null;

function load(): StoredReminder[] {
  if (cache) return cache;
  const path = getDataPath();
  if (!existsSync(path)) {
    cache = [];
    return cache;
  }
  try {
    cache = JSON.parse(readFileSync(path, 'utf-8'));
  } catch {
    cache = [];
  }
  return cache!;
}

function save(): void {
  const path = getDataPath();
  const dir = dirname(path);
  if (!existsSync(dir)) mkdirSync(dir, { recursive: true });
  writeFileSync(path, JSON.stringify(load(), null, 2));
}

export function getAll(): StoredReminder[] {
  return load();
}

export function getById(id: string): StoredReminder | undefined {
  return load().find((r) => r.id === id);
}

export function add(reminder: StoredReminder): void {
  load().push(reminder);
  save();
}

export function remove(id: string): boolean {
  const reminders = load();
  const idx = reminders.findIndex((r) => r.id === id);
  if (idx === -1) return false;
  reminders.splice(idx, 1);
  save();
  return true;
}

export function update(id: string, fields: Partial<StoredReminder>): StoredReminder | null {
  const r = getById(id);
  if (!r) return null;
  Object.assign(r, fields);
  save();
  return r;
}

export function activateOverdue(): StoredReminder[] {
  const now = new Date().toISOString();
  const reminders = load();
  const activated: StoredReminder[] = [];
  for (const r of reminders) {
    if (r.status === 'pending' && r.scheduled_at <= now) {
      r.status = 'active';
      activated.push(r);
    }
  }
  if (activated.length > 0) save();
  return activated;
}
```

- [ ] **Step 3: Commit**

```bash
git add web/src/lib/types.ts web/src/lib/server/store.ts
git commit -m "Add server-side reminder store with JSON file persistence"
```

---

### Task 3: Create the recurrence module

**Files:**
- Create: `web/src/lib/server/recurrence.ts`

- [ ] **Step 1: Create recurrence.ts**

```typescript
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
```

- [ ] **Step 2: Commit**

```bash
git add web/src/lib/server/recurrence.ts
git commit -m "Add server-side recurrence calculation"
```

---

### Task 4: Create the ESP32 push helper

**Files:**
- Create: `web/src/lib/server/esp32.ts`

- [ ] **Step 1: Create esp32.ts**

```typescript
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
```

- [ ] **Step 2: Commit**

```bash
git add web/src/lib/server/esp32.ts
git commit -m "Add ESP32 push notification helper"
```

---

### Task 5: Create API route — GET & POST /api/reminders

**Files:**
- Create: `web/src/routes/api/reminders/+server.ts`

- [ ] **Step 1: Create the route**

```typescript
import { json } from '@sveltejs/kit';
import type { RequestHandler } from './$types';
import type { StoredReminder, CreateReminderPayload, Reminder } from '$lib/types';
import * as store from '$lib/server/store';
import { nudgeEsp32 } from '$lib/server/esp32';
import { nextOccurrence } from '$lib/server/recurrence';
import { getTypeMeta } from '$lib/reminder-type-meta';
import { randomUUID } from 'crypto';

function toApiReminder(r: StoredReminder): Reminder {
  return { ...r, name: getTypeMeta(r.type).label };
}

export const GET: RequestHandler = async () => {
  // Activate any overdue reminders
  store.activateOverdue();

  const reminders = store.getAll().map(toApiReminder);

  // Sort: completed last, then by scheduled_at
  reminders.sort((a, b) => {
    const aDone = a.status === 'completed' ? 1 : 0;
    const bDone = b.status === 'completed' ? 1 : 0;
    if (aDone !== bDone) return aDone - bDone;
    return new Date(a.scheduled_at).getTime() - new Date(b.scheduled_at).getTime();
  });

  return json(reminders);
};

export const POST: RequestHandler = async ({ request }) => {
  const body: CreateReminderPayload = await request.json();

  if (!body.type || !body.scheduled_at || !body.recurrence) {
    return json({ error: 'Missing required fields' }, { status: 400 });
  }

  let scheduledAt = body.scheduled_at;

  // If recurring and scheduled in the past, advance to next occurrence
  if (body.recurrence !== 'none' && new Date(scheduledAt) <= new Date()) {
    scheduledAt = nextOccurrence(scheduledAt, body.recurrence);
  }

  const reminder: StoredReminder = {
    id: randomUUID(),
    type: body.type,
    scheduled_at: scheduledAt,
    recurrence: body.recurrence,
    status: 'pending',
    created_at: new Date().toISOString(),
  };

  store.add(reminder);
  nudgeEsp32();

  return json(toApiReminder(reminder), { status: 201 });
};
```

- [ ] **Step 2: Commit**

```bash
git add web/src/routes/api/reminders/+server.ts
git commit -m "Add GET/POST /api/reminders server routes"
```

---

### Task 6: Create API route — GET /api/reminders/active

**Files:**
- Create: `web/src/routes/api/reminders/active/+server.ts`

- [ ] **Step 1: Create the route**

```typescript
import { json } from '@sveltejs/kit';
import type { RequestHandler } from './$types';
import type { StoredReminder, Reminder } from '$lib/types';
import * as store from '$lib/server/store';
import { getTypeMeta } from '$lib/reminder-type-meta';

function toApiReminder(r: StoredReminder): Reminder {
  return { ...r, name: getTypeMeta(r.type).label };
}

export const GET: RequestHandler = async () => {
  // Activate any overdue pending reminders
  store.activateOverdue();

  const active = store.getAll()
    .filter((r) => r.status === 'active')
    .sort((a, b) => new Date(a.scheduled_at).getTime() - new Date(b.scheduled_at).getTime())
    .map(toApiReminder);

  return json(active);
};
```

- [ ] **Step 2: Commit**

```bash
git add web/src/routes/api/reminders/active/+server.ts
git commit -m "Add GET /api/reminders/active server route"
```

---

### Task 7: Create API route — DELETE /api/reminders/[id]

**Files:**
- Create: `web/src/routes/api/reminders/[id]/+server.ts`

- [ ] **Step 1: Create the route**

```typescript
import { json } from '@sveltejs/kit';
import type { RequestHandler } from './$types';
import * as store from '$lib/server/store';
import { nudgeEsp32 } from '$lib/server/esp32';

export const DELETE: RequestHandler = async ({ params }) => {
  const removed = store.remove(params.id);
  if (!removed) {
    return json({ error: 'Not found' }, { status: 404 });
  }

  nudgeEsp32();
  return new Response(null, { status: 204 });
};
```

- [ ] **Step 2: Commit**

```bash
git add web/src/routes/api/reminders/\[id\]/+server.ts
git commit -m "Add DELETE /api/reminders/[id] server route"
```

---

### Task 8: Create API route — POST /api/reminders/[id]/dismiss

**Files:**
- Create: `web/src/routes/api/reminders/[id]/dismiss/+server.ts`

- [ ] **Step 1: Create the route**

```typescript
import { json } from '@sveltejs/kit';
import type { RequestHandler } from './$types';
import type { StoredReminder } from '$lib/types';
import * as store from '$lib/server/store';
import { nudgeEsp32 } from '$lib/server/esp32';
import { nextOccurrence } from '$lib/server/recurrence';
import { getTypeMeta } from '$lib/reminder-type-meta';
import { randomUUID } from 'crypto';

export const POST: RequestHandler = async ({ params }) => {
  const reminder = store.getById(params.id);
  if (!reminder) {
    return json({ error: 'Not found' }, { status: 404 });
  }

  // Mark as completed
  store.update(params.id, { status: 'completed' });

  // If recurring, create next occurrence
  if (reminder.recurrence !== 'none') {
    const next: StoredReminder = {
      id: randomUUID(),
      type: reminder.type,
      scheduled_at: nextOccurrence(reminder.scheduled_at, reminder.recurrence),
      recurrence: reminder.recurrence,
      status: 'pending',
      created_at: new Date().toISOString(),
    };
    store.add(next);
  }

  nudgeEsp32();

  return json({ dismissed: params.id });
};
```

- [ ] **Step 2: Commit**

```bash
git add web/src/routes/api/reminders/\[id\]/dismiss/+server.ts
git commit -m "Add POST /api/reminders/[id]/dismiss server route"
```

---

### Task 9: Update client-side API and clean up

**Files:**
- Modify: `web/src/lib/api.ts`
- Delete: `web/src/lib/mock-api.ts`
- Delete: `web/.env.development` (or update)
- Delete: `web/.env.example` (or update)

- [ ] **Step 1: Rewrite api.ts for same-origin requests**

```typescript
import type { Reminder, CreateReminderPayload } from './types';

async function request<T>(path: string, options?: RequestInit): Promise<T> {
  const res = await fetch(path, {
    ...options,
    headers: { 'Content-Type': 'application/json', ...options?.headers },
  });
  if (!res.ok) {
    const text = await res.text().catch(() => '');
    throw new Error(`API error ${res.status}: ${text}`);
  }
  if (res.status === 204) return undefined as T;
  return res.json();
}

export const api = {
  getReminders(): Promise<Reminder[]> {
    return request<Reminder[]>('/api/reminders');
  },

  createReminder(payload: CreateReminderPayload): Promise<Reminder> {
    return request<Reminder>('/api/reminders', {
      method: 'POST',
      body: JSON.stringify(payload),
    });
  },

  deleteReminder(id: string): Promise<void> {
    return request<void>(`/api/reminders/${id}`, { method: 'DELETE' });
  },

  dismissReminder(id: string): Promise<void> {
    return request<void>(`/api/reminders/${id}/dismiss`, { method: 'POST' });
  },
};
```

- [ ] **Step 2: Delete mock-api.ts**

```bash
rm web/src/lib/mock-api.ts
```

- [ ] **Step 3: Update .env.development**

Remove `VITE_USE_MOCK` and `VITE_ESP32_URL`. Add server-side env:

```
ESP32_URL=http://10.0.0.111
```

- [ ] **Step 4: Verify build**

```bash
cd web && npm run build
```

- [ ] **Step 5: Commit**

```bash
git add web/src/lib/api.ts web/.env.development
git rm web/src/lib/mock-api.ts
git commit -m "Switch API client to same-origin, remove mock API"
```

---

### Task 10: Update Dockerfile and docker-compose

**Files:**
- Modify: `web/Dockerfile`
- Modify: `web/docker-compose.yml`
- Delete: `web/nginx.conf`

- [ ] **Step 1: Rewrite Dockerfile for node adapter**

```dockerfile
FROM node:20-alpine AS build
WORKDIR /app
COPY package*.json ./
RUN npm ci
COPY . .
RUN npm run build

FROM node:20-alpine
WORKDIR /app
COPY --from=build /app/build ./build
COPY --from=build /app/package.json ./
COPY --from=build /app/node_modules ./node_modules
EXPOSE 3000
CMD ["node", "build"]
```

- [ ] **Step 2: Update docker-compose.yml**

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

- [ ] **Step 3: Delete nginx.conf**

```bash
rm web/nginx.conf
```

- [ ] **Step 4: Test Docker build locally**

```bash
cd web && docker build -t trm-web-test .
```

- [ ] **Step 5: Commit**

```bash
git add web/Dockerfile web/docker-compose.yml
git rm web/nginx.conf
git commit -m "Switch Docker from nginx static to node runtime with volume"
```

---

### Task 11: Update GitHub Actions workflow

**Files:**
- Modify: `.github/workflows/docker.yml`

- [ ] **Step 1: Update workflow — remove VITE_ESP32_URL build arg**

The ESP32 URL is now a runtime env var, not a build-time arg. Remove `build-args` from the workflow:

```yaml
name: Build & push Docker image

on:
  push:
    branches: [main]
    paths:
      - 'web/**'
      - '.github/workflows/docker.yml'

jobs:
  build-and-push:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Log in to Docker Hub
        uses: docker/login-action@v3
        with:
          username: ${{ secrets.DOCKERHUB_USERNAME }}
          password: ${{ secrets.DOCKERHUB_TOKEN }}

      - name: Build and push
        uses: docker/build-push-action@v6
        with:
          context: ./web
          push: true
          tags: kliszaj/trm-web:latest
```

- [ ] **Step 2: Commit**

```bash
git add .github/workflows/docker.yml
git commit -m "Remove build-time ESP32 URL from Docker workflow"
```

---

## Chunk 2: Firmware — Thin Client

### Task 12: Create sync client module

**Files:**
- Create: `firmware/src/sync_client.h`
- Create: `firmware/src/sync_client.cpp`

- [ ] **Step 1: Create sync_client.h**

```cpp
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "reminder_types.h"

struct RemoteReminder {
    String id;
    ReminderType type;
    String scheduled_at;
    String status;  // "pending", "active", "completed"
};

class SyncClient {
public:
    bool fetchActive(std::vector<RemoteReminder>& out);
    bool fetchNextPending(RemoteReminder& out);
    bool dismiss(const String& id);
    void beginSyncServer();   // Start minimal /sync endpoint
    void handleSyncServer();  // Call from loop()
    bool wasSyncRequested();  // Check if /sync was hit

private:
    bool _syncRequested = false;
    bool fetchReminders(const String& path, std::vector<RemoteReminder>& out);
};

extern SyncClient syncClient;
```

- [ ] **Step 2: Create sync_client.cpp**

```cpp
#include "sync_client.h"
#include "config.h"
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>

SyncClient syncClient;

static WebServer syncServer(80);

bool SyncClient::fetchReminders(const String& path, std::vector<RemoteReminder>& out) {
    HTTPClient http;
    String url = String(WEB_APP_URL) + path;

    http.begin(url);
    http.setTimeout(5000);
    int code = http.GET();

    if (code != 200) {
        Serial.printf("[sync] GET %s failed: %d\n", path.c_str(), code);
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[sync] JSON parse error: %s\n", err.c_str());
        return false;
    }

    JsonArray arr = doc.as<JsonArray>();
    out.clear();
    for (JsonObject obj : arr) {
        RemoteReminder r;
        r.id = obj["id"].as<String>();
        r.type = reminderTypeFromString(obj["type"].as<String>());
        r.scheduled_at = obj["scheduled_at"].as<String>();
        r.status = obj["status"].as<String>();
        out.push_back(r);
    }

    Serial.printf("[sync] GET %s: %d reminders\n", path.c_str(), (int)out.size());
    return true;
}

bool SyncClient::fetchActive(std::vector<RemoteReminder>& out) {
    return fetchReminders("/api/reminders/active", out);
}

bool SyncClient::fetchNextPending(RemoteReminder& out) {
    std::vector<RemoteReminder> all;
    if (!fetchReminders("/api/reminders", all)) return false;

    // Find earliest pending
    RemoteReminder* earliest = nullptr;
    for (auto& r : all) {
        if (r.status == "pending") {
            if (!earliest || r.scheduled_at < earliest->scheduled_at) {
                earliest = &r;
            }
        }
    }

    if (earliest) {
        out = *earliest;
        return true;
    }
    return false;
}

bool SyncClient::dismiss(const String& id) {
    HTTPClient http;
    String url = String(WEB_APP_URL) + "/api/reminders/" + id + "/dismiss";

    http.begin(url);
    http.setTimeout(5000);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST("{}");
    http.end();

    Serial.printf("[sync] Dismiss %s: %d\n", id.c_str(), code);
    return code == 200;
}

void SyncClient::beginSyncServer() {
    syncServer.on("/sync", HTTP_POST, [this]() {
        _syncRequested = true;
        syncServer.send(200, "text/plain", "OK");
        Serial.println("[sync] Nudge received");
    });

    syncServer.on("/sync", HTTP_OPTIONS, []() {
        syncServer.sendHeader("Access-Control-Allow-Origin", "*");
        syncServer.sendHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        syncServer.send(204);
    });

    syncServer.begin();
    Serial.println("[sync] Sync server started on port 80");
}

void SyncClient::handleSyncServer() {
    syncServer.handleClient();
}

bool SyncClient::wasSyncRequested() {
    if (_syncRequested) {
        _syncRequested = false;
        return true;
    }
    return false;
}
```

- [ ] **Step 3: Commit**

```bash
git add firmware/src/sync_client.h firmware/src/sync_client.cpp
git commit -m "Add sync client for fetching reminders from web app"
```

---

### Task 13: Update config.h

**Files:**
- Modify: `firmware/src/config.h`

- [ ] **Step 1: Add WEB_APP_URL and update poll interval**

Add to config.h:

```cpp
// ── Web App (source of truth) ─────────────────────────────────────────────
#define WEB_APP_URL    "http://10.0.0.100:8082"  // Unraid server
#define SYNC_INTERVAL_MS  300000   // 5 minutes
```

Change `POLL_INTERVAL_MS` to `SYNC_INTERVAL_MS` (or just remove the old one).

- [ ] **Step 2: Commit**

```bash
git add firmware/src/config.h
git commit -m "Add WEB_APP_URL config, change poll to 5 min sync interval"
```

---

### Task 14: Rewrite main.cpp for sync-based loop

**Files:**
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: Rewrite main.cpp**

Replace the current main.cpp with a sync-based version. Key changes:
- Remove `#include "reminder_store.h"`, `#include "api_server.h"`, `#include "recurrence.h"`
- Add `#include "sync_client.h"`
- Remove `getNextActiveReminder()`, `getNextPendingReminder()`, `checkAndFireReminders()`
- Add `syncFromServer()` that fetches active/pending from web app and updates display
- `dismissCurrentReminder()` now calls `syncClient.dismiss()` then re-syncs
- Loop: handle sync server, touch, screensaver timeout, periodic sync

```cpp
#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"
#include "wifi_manager.h"
#include "time_manager.h"
#include "display_manager.h"
#include "touch_manager.h"
#include "sync_client.h"

static const uint32_t CONFIRM_HOLD_MS = 3000;
static const uint32_t SCREENSAVER_TIMEOUT_MS = 120000;

static unsigned long lastSyncMs = 0;

// Currently displayed active reminder ID (so we know what to dismiss)
static String activeReminderId;

static void syncFromServer() {
    std::vector<RemoteReminder> active;
    if (syncClient.fetchActive(active) && !active.empty()) {
        // Show first active reminder
        const RemoteReminder& r = active[0];
        activeReminderId = r.id;
        if (r.type != ReminderType::Unknown) {
            // Create a temporary Reminder-like object for display
            Reminder displayR;
            displayR.id = r.id;
            displayR.type = r.type;
            displayR.scheduled_at = 0;  // not used for active display
            displayR.recurrence = Recurrence::None;
            displayR.status = ReminderStatus::Active;
            displayR.created_at = 0;

            if (displayManager.getState() != DisplayState::Active) {
                displayManager.showActive(displayR);
            }
        }
        displayManager.resetInactivityTimer();
        return;
    }

    // No active reminders — show next pending or screensaver
    activeReminderId = "";
    RemoteReminder nextPending;
    if (syncClient.fetchNextPending(nextPending)) {
        // Build a Reminder for idle display
        Reminder displayR;
        displayR.id = nextPending.id;
        displayR.type = nextPending.type;
        // Parse ISO 8601 to time_t for display formatting
        struct tm tm = {};
        sscanf(nextPending.scheduled_at.c_str(),
               "%d-%d-%dT%d:%d:%d",
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
        displayR.scheduled_at = mktime(&tm);
        displayR.recurrence = Recurrence::None;
        displayR.status = ReminderStatus::Pending;
        displayR.created_at = 0;

        DisplayState st = displayManager.getState();
        if (st != DisplayState::Idle && st != DisplayState::Screensaver) {
            displayManager.showIdle(&displayR);
            displayManager.resetInactivityTimer();
        } else if (st == DisplayState::Idle) {
            // Refresh idle content
            displayManager.showIdle(&displayR);
        }
    } else {
        // No reminders at all — screensaver
        if (displayManager.getState() != DisplayState::Screensaver) {
            displayManager.showScreensaver();
        }
    }
}

static void dismissCurrentReminder() {
    if (activeReminderId.isEmpty()) return;

    String id = activeReminderId;

    // Show checkmark immediately
    displayManager.showConfirmation();

    // Tell web app to dismiss
    syncClient.dismiss(id);

    // Hold checkmark
    delay(CONFIRM_HOLD_MS);

    // Re-sync to show next state
    syncFromServer();
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[boot] that reminds me... starting");

    if (!LittleFS.begin(true)) {
        Serial.println("[boot] LittleFS mount failed");
    }

    displayManager.begin();
    displayManager.showBoot();

    if (!wifiConnect()) {
        Serial.println("[boot] Wi-Fi failed");
    }

    timeSyncInit();
    touchManager.begin();
    syncClient.beginSyncServer();

    // Initial sync from web app
    syncFromServer();
    lastSyncMs = millis();

    Serial.println("[boot] Ready");
}

void loop() {
    syncClient.handleSyncServer();
    touchManager.update();
    displayManager.tick();

    // Handle sync nudge from web app
    if (syncClient.wasSyncRequested()) {
        syncFromServer();
        lastSyncMs = millis();
    }

    // Handle touch
    if (touchManager.wasTapped()) {
        DisplayState st = displayManager.getState();
        displayManager.resetInactivityTimer();

        if (st == DisplayState::Screensaver) {
            syncFromServer();
        } else if (st == DisplayState::Active) {
            dismissCurrentReminder();
        }
    }

    // Screensaver timeout
    if (displayManager.getState() == DisplayState::Idle) {
        if (displayManager.isInactiveFor(SCREENSAVER_TIMEOUT_MS)) {
            displayManager.showScreensaver();
        }
    }

    // Periodic sync (safety net)
    if (millis() - lastSyncMs >= SYNC_INTERVAL_MS) {
        lastSyncMs = millis();
        syncFromServer();
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "Rewrite main loop for web app sync instead of local store"
```

---

### Task 15: Delete removed firmware files

**Files:**
- Delete: `firmware/src/api_server.cpp`
- Delete: `firmware/src/api_server.h`
- Delete: `firmware/src/reminder_store.cpp`
- Delete: `firmware/src/reminder_store.h`
- Delete: `firmware/src/recurrence.cpp`
- Delete: `firmware/src/recurrence.h`

- [ ] **Step 1: Remove files**

```bash
git rm firmware/src/api_server.cpp firmware/src/api_server.h
git rm firmware/src/reminder_store.cpp firmware/src/reminder_store.h
git rm firmware/src/recurrence.cpp firmware/src/recurrence.h
```

- [ ] **Step 2: Verify firmware compiles**

```bash
cd firmware && pio run
```

Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git commit -m "Remove local API server, reminder store, and recurrence from firmware"
```

---

### Task 16: Update display_manager to work without Reminder struct dependency

**Files:**
- Modify: `firmware/src/display_manager.h`
- Modify: `firmware/src/display_manager.cpp`

The `display_manager` currently includes `reminder_store.h` for the `Reminder` struct. Since we're removing `reminder_store.h`, we need to keep a minimal `Reminder` struct for display purposes. The simplest approach: keep the `Reminder` struct definition in a new lightweight header, or directly in `display_manager.h`.

- [ ] **Step 1: Update display_manager.h**

Replace `#include "reminder_store.h"` with a minimal struct definition:

```cpp
#pragma once
#include <Arduino.h>
#include "reminder_types.h"

// Minimal reminder data for display purposes
enum class ReminderStatus { Pending, Active, Completed };
enum class Recurrence { None, Daily, Weekly, Weekdays };

struct Reminder {
    String id;
    ReminderType type;
    time_t scheduled_at;
    Recurrence recurrence;
    ReminderStatus status;
    time_t created_at;
    String label() const { return String(getTypeInfo(type)->label); }
};
```

Keep rest of the header unchanged.

- [ ] **Step 2: Verify build**

```bash
cd firmware && pio run
```

- [ ] **Step 3: Commit**

```bash
git add firmware/src/display_manager.h
git commit -m "Decouple display_manager from reminder_store, inline minimal Reminder struct"
```

---

## Chunk 3: Integration & Deploy

### Task 17: End-to-end test locally

- [ ] **Step 1: Start web app in dev mode**

```bash
cd web
mkdir -p data
ESP32_URL=http://10.0.0.111 npm run dev
```

- [ ] **Step 2: Test API endpoints with curl**

```bash
# Create a reminder
curl -X POST http://localhost:5173/api/reminders \
  -H "Content-Type: application/json" \
  -d '{"type":"feed_evie","scheduled_at":"2026-03-16T08:00:00.000Z","recurrence":"daily"}'

# List reminders
curl http://localhost:5173/api/reminders

# Check active
curl http://localhost:5173/api/reminders/active
```

- [ ] **Step 3: Test web UI in browser**

Open `http://localhost:5173`, create/delete reminders, verify they persist in `data/reminders.json`.

- [ ] **Step 4: Flash firmware and test ESP32**

```bash
cd firmware && pio run -t upload
```

Verify:
- ESP32 boots and fetches from web app
- Creating a reminder in web UI triggers sync nudge → ESP32 display updates
- Tapping ESP32 dismisses → web app updates → display shows next state

---

### Task 18: Build and deploy Docker image

- [ ] **Step 1: Test Docker build locally**

```bash
cd web && docker build -t trm-web-test .
docker run --rm -p 8082:3000 -v $(pwd)/data:/app/data -e ESP32_URL=http://10.0.0.111 -e TZ=Europe/Stockholm trm-web-test
```

- [ ] **Step 2: Push to GitHub to trigger Docker Hub build**

```bash
git push
```

- [ ] **Step 3: Update Unraid docker-compose**

Update the compose on Unraid with the new config (port, volume, env vars).

- [ ] **Step 4: Verify end-to-end on production**

Create reminder via web app on Unraid → verify ESP32 shows it → tap to dismiss → verify web app updates.

- [ ] **Step 5: Final commit**

```bash
git add -A
git commit -m "Complete migration: web app is now source of truth"
```
