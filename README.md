# that reminds me...

A minimal household reminder system: a physical desk gadget with a round display + a local web app to manage reminders from any device on your home network.

No accounts. No cloud. No notifications. Just a glanceable, always-on reminder surface you can tap to dismiss.

---

## How it works

1. Open the web app from your phone or laptop
2. Pick a reminder type, set a time, and optionally make it recurring
3. The desk gadget shows what's coming up next
4. When a reminder fires, the screen changes to a color-coded pixel art screen
5. Tap anywhere to dismiss — recurring reminders automatically schedule the next one

---

## Hardware

| Part | Details |
|---|---|
| [Seeed Studio XIAO ESP32-C3](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/) | Microcontroller (RISC-V, Wi-Fi) |
| [Round Display for XIAO](https://wiki.seeedstudio.com/get_start_round_display/) | 1.28" 240x240 GC9A01 round display + CHSC6X capacitive touch |

---

## Reminder types

The system has four hardcoded reminder types, each with its own pixel art and color:

| Type | Color |
|---|---|
| Feed Evie | Pink |
| Water plants | Orange |
| Eat vitamins | Blue |
| Take out trash | Purple |

---

## Architecture

```
Browser (phone / laptop)
        |  HTTP
        v
  Svelte web app          <-- hosted on Unraid (Docker)
        |  REST API
        v
    ESP32-C3              <-- serves API, drives display, handles touch
        |
   LittleFS               <-- reminder storage (survives power cycles)
```

The ESP32 is the source of truth. The web app is a separate static site hosted on Unraid that talks to the ESP32's REST API over the local network.

The display updates instantly when reminders are created or deleted via the API — no waiting for the poll cycle.

---

## Repo structure

```
trm/
├── firmware/              # PlatformIO project (ESP32-C3)
│   ├── src/
│   │   ├── config.h               # Wi-Fi, timezone, pins
│   │   ├── main.cpp               # Boot + main loop
│   │   ├── api_server.*           # REST API + CORS
│   │   ├── reminder_store.*       # CRUD + LittleFS persistence
│   │   ├── reminder_types.h       # Type definitions (label, color, image)
│   │   ├── display_manager.*      # LovyanGFX rendering (boot/idle/active/confirm)
│   │   ├── touch_manager.*        # CHSC6X tap detection (I2C)
│   │   ├── recurrence.*           # Next-occurrence calculation
│   │   ├── wifi_manager.*
│   │   ├── time_manager.*         # NTP sync
│   │   └── lgfx_config.h          # LovyanGFX pin/driver config
│   ├── tools/
│   │   ├── convert_fonts.py       # TTF/OTF -> VLW smooth font converter
│   │   └── convert_images.py      # PNG pixel art -> RGB565 binary converter
│   └── data/                      # LittleFS filesystem (fonts, images)
└── web/                   # SvelteKit web app
    ├── src/
    │   ├── lib/
    │   │   ├── api.ts             # HTTP client
    │   │   ├── mock-api.ts        # In-memory mock for dev
    │   │   ├── types.ts           # Shared types
    │   │   └── components/
    │   └── routes/
    │       └── +page.svelte
    ├── Dockerfile
    ├── docker-compose.yml
    └── nginx.conf
```

---

## Getting started

### Web app (no hardware needed)

```bash
cd web
npm install
npm run dev        # -> http://localhost:5173
```

Mock data is enabled by default in dev mode (`VITE_USE_MOCK=true` in `.env.development`), so everything works without an ESP32.

### Firmware

#### 1. Prerequisites

- [VS Code](https://code.visualstudio.com/) + [PlatformIO extension](https://platformio.org/install/ide?install=vscode)
- Python 3 + `pip install freetype-py` (for font conversion)
- PP Mondwest Bold font file in `firmware/tools/fonts/`

#### 2. Configure

Edit [`firmware/src/config.h`](firmware/src/config.h):

```cpp
#define WIFI_SSID "YourSSID"
#define WIFI_PASS "YourPassword"
```

Everything else (timezone, NTP, pins) is already set for the Seeed Round Display + Stockholm timezone.

#### 3. Build assets

```bash
cd firmware/tools
python convert_fonts.py     # generates VLW fonts in firmware/data/
python convert_images.py    # generates RGB565 binaries in firmware/data/
```

#### 4. Flash

Connect the XIAO ESP32-C3 via USB, then:

```bash
cd firmware
pio run -t uploadfs    # upload LittleFS (fonts, images)
pio run -t upload      # upload firmware
```

> **First flash tip:** If the device isn't detected, hold the **BOOT** button and press **RESET** to enter bootloader mode before uploading.

---

## Deploying to Unraid

The web app is published to Docker Hub automatically on every push to `main`.

On your Unraid server, use **Compose Manager** with the [`web/docker-compose.yml`](web/docker-compose.yml):

```yaml
services:
  trm-web:
    image: kliszaj/trm-web:latest
    ports:
      - "8080:8080"
    restart: unless-stopped
```

The app will be available at `http://<unraid-ip>:8080`.

> **Note:** The ESP32 URL (`VITE_ESP32_URL`) is baked into the image at build time. To change it, update the `VITE_ESP32_URL` secret in GitHub Actions and re-run the workflow.

---

## Display states

**Boot** — shows the logo on a white background while connecting to Wi-Fi and syncing time.

**Idle** — shows the next upcoming reminder: "Next" label at top, reminder name centered, and time at bottom (e.g. "in 15 min", "Today, 6:30 PM", "Tomorrow"). If nothing is scheduled, shows "No reminders".

**Active** — shown when a reminder fires. Displays color-coded pixel art for the reminder type. Tap anywhere to dismiss. If multiple reminders have fired, they queue and must be dismissed one at a time.

**Confirmation** — brief checkmark screen after dismissing a reminder (3 seconds).

All screen transitions use a circle-wipe animation.

---

## Scheduling behavior

- Recurring reminders with a scheduled time in the past are automatically advanced to the next occurrence on creation (e.g. setting a daily 8:00 AM reminder at 2:00 PM schedules it for tomorrow).
- When a recurring reminder is dismissed, the next occurrence is calculated from the original time (not the dismissal time).
- Recurrence options: daily, weekly, weekdays (Mon-Fri).

---

## CI / CD

Pushing to `main` with changes in `web/` triggers a GitHub Actions workflow that builds and pushes `kliszaj/trm-web:latest` to Docker Hub.

Required repository secrets:

| Secret | Value |
|---|---|
| `DOCKERHUB_USERNAME` | Your Docker Hub username |
| `DOCKERHUB_TOKEN` | Docker Hub access token |
| `VITE_ESP32_URL` | ESP32 IP, e.g. `http://10.0.0.111` |

---

## API reference

The ESP32 exposes a REST API on port 80:

| Method | Path | Description |
|---|---|---|
| `GET` | `/api/reminders` | List all pending/active reminders |
| `POST` | `/api/reminders` | Create a reminder |
| `DELETE` | `/api/reminders/:id` | Delete a reminder |
| `PATCH` | `/api/reminders/:id` | Update status |
| `GET` | `/api/time` | Current ESP32 time (debug) |

All endpoints return JSON and include CORS headers.

### Reminder schema

```json
{
  "id": "uuid",
  "name": "Feed Evie",
  "scheduled_at": "2026-03-16T08:00:00Z",
  "type": "feed_evie",
  "recurrence": "daily",
  "status": "pending",
  "created_at": "2026-03-15T14:00:00Z"
}
```

**type**: `feed_evie` | `water_plants` | `eat_vitamins` | `take_out_trash`

**recurrence**: `none` | `daily` | `weekly` | `weekdays`

**status**: `pending` | `active` | `completed`
