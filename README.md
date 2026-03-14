# that reminds me…

A minimal household reminder system: a physical desk gadget with a round display + a local web app to manage reminders from any device on your home network.

No accounts. No cloud. No notifications. Just a glanceable, always-on reminder surface you can tap to dismiss.

---

## Hardware

| Part | Details |
|---|---|
| [Seeed Studio XIAO ESP32-C3](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/) | Microcontroller (RISC-V, Wi-Fi) |
| [Round Display for XIAO](https://wiki.seeedstudio.com/get_start_round_display/) | 1.28" 240×240 round display + capacitive touch |
| GC9A01 | Display driver (SPI) |
| CST816S | Touch controller (I2C) |

---

## Architecture

```
Browser (phone / laptop)
        │  HTTP
        ▼
  Svelte web app          ◄── hosted on Unraid (Docker)
        │  REST API
        ▼
    ESP32-C3              ◄── serves API, drives display, handles touch
        │
   LittleFS               ◄── reminder storage (survives power cycles)
```

The ESP32 is the source of truth. The web app is just a client — it talks to the ESP32's REST API over the local network.

---

## Repo structure

```
trm/
├── firmware/          # PlatformIO project (ESP32-C3)
│   └── src/
│       ├── config.h            # Wi-Fi, timezone, pins — edit this
│       ├── main.cpp
│       ├── api_server.*        # REST API + CORS
│       ├── reminder_store.*    # CRUD + LittleFS persistence
│       ├── display_manager.*   # GC9A01 rendering
│       ├── touch_manager.*     # CST816S tap detection
│       ├── recurrence.*        # Next-occurrence logic
│       ├── wifi_manager.*
│       └── time_manager.*      # NTP sync
└── web/               # Svelte web app
    ├── src/
    │   ├── lib/
    │   │   ├── api.ts           # HTTP client (or mock)
    │   │   ├── mock-api.ts      # In-memory mock for dev
    │   │   ├── types.ts         # Shared types
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
npm run dev        # → http://localhost:5173
```

Mock data is enabled by default in dev mode (`VITE_USE_MOCK=true` in `.env.development`), so everything works without an ESP32.

### Firmware

#### 1. Prerequisites

- [VS Code](https://code.visualstudio.com/) + [PlatformIO extension](https://platformio.org/install/ide?install=vscode)

#### 2. Configure

Edit [`firmware/src/config.h`](firmware/src/config.h):

```cpp
#define WIFI_SSID "YourSSID"
#define WIFI_PASS "YourPassword"
```

Everything else (timezone, NTP, pins) is already set for the Seeed Round Display + Stockholm timezone.

#### 3. Flash

Open the `firmware/` folder in VS Code, connect the XIAO ESP32-C3 via USB, then click **Upload** in the PlatformIO toolbar.

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

> **Note:** The ESP32 URL (`VITE_ESP32_URL`) is baked into the image at build time. To change it, update the `VITE_ESP32_URL` secret in GitHub → Actions → Secrets and re-run the workflow.

---

## CI / CD

Pushing to `main` with changes in `web/` triggers a GitHub Actions workflow that builds and pushes `kliszaj/trm-web:latest` to Docker Hub.

Required repository secrets:

| Secret | Value |
|---|---|
| `DOCKERHUB_USERNAME` | Your Docker Hub username |
| `DOCKERHUB_TOKEN` | Docker Hub access token |
| `VITE_ESP32_URL` | ESP32 IP, e.g. `http://192.168.1.XX` |

---

## API reference

The ESP32 exposes a simple REST API on port 80:

| Method | Path | Description |
|---|---|---|
| `GET` | `/api/reminders` | List all pending/active reminders |
| `POST` | `/api/reminders` | Create a reminder |
| `DELETE` | `/api/reminders/:id` | Delete a reminder |
| `PATCH` | `/api/reminders/:id` | Update status (used internally) |
| `GET` | `/api/time` | Current ESP32 time (debug) |

All endpoints return JSON and include CORS headers.

### Reminder schema

```json
{
  "id": "uuid",
  "name": "Take out the trash",
  "scheduled_at": "2025-06-15T18:00:00Z",
  "type": "generic",
  "recurrence": "weekly",
  "status": "pending",
  "created_at": "2025-06-10T09:00:00Z"
}
```

`recurrence`: `none` | `daily` | `weekly` | `weekdays`
`status`: `pending` | `active` | `completed`

---

## Device display

**Idle** — shows the next upcoming reminder (name + time). If nothing is scheduled, shows "No reminders."

**Active** — shown when a reminder fires. Displays the name full-screen with an orange ring. Tap anywhere to dismiss. If multiple reminders have fired, they queue and must be dismissed one at a time.

Recurring reminders automatically schedule their next occurrence (calculated from the original time, not the dismissal time) when dismissed.
