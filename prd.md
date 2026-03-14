# PRD: that reminds me...

**Version:** 1.0 — Draft  
**Author:** TBD  
**Status:** In Progress  
**Last Updated:** 2026-03-14

---

## 1. Overview

**that reminds me...** is a minimal household reminder system consisting of two components:

- **A physical desk gadget** — a Seeed Studio Round Display (GC9A01) driven by an ESP32, sitting on a desk and showing upcoming reminders at a glance.
- **A local web app** — served directly from the ESP32, accessible from any browser on the home network, used to create and manage reminders.

The system is intentionally simple: reminders fire at a scheduled time, appear on the device screen, and are dismissed with a tap. No accounts, no cloud, no push notifications — just a glanceable, always-on reminder surface.

---

## 2. Goals

- Make it easy to add a reminder from any device on the home network without installing anything.
- Give the desk gadget a calm, useful idle state that surfaces what's coming up next.
- When a reminder fires, make it impossible to miss without being annoying.
- Tap once to dismiss. That's it.

---

## 3. Non-Goals (v1)

- No user accounts or authentication.
- No cloud connectivity or remote access outside the home network.
- No push notifications to phones or other devices.
- No multi-device display sync.
- No reminder types beyond "generic" for now.
- No snooze functionality.
- No history or completed reminders log.
- No sound or haptic output.

---

## 4. Users

**Primary user:** One person (or household members) on the same home Wi-Fi network.

Anyone on the network can open the web app and add or manage reminders. There is no concept of ownership or per-user reminders in v1.

---

## 5. Hardware

| Component | Part | Docs |
|---|---|---|
| Microcontroller | [Seeed Studio XIAO ESP32-C3](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/) | [Wiki](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/) |
| Display + Touch | [Round Display for Seeed Studio XIAO](https://wiki.seeedstudio.com/get_start_round_display/) | [Getting Started](https://wiki.seeedstudio.com/get_start_round_display/) · [Hardware Usage](https://wiki.seeedstudio.com/seeedstudio_round_display_usage/) |
| Display driver | GC9A01 (SPI, 240×240, 1.28") | — |
| Touch controller | CST816S (I2C, capacitive) | — |
| Power | USB-C | — |

> ⚠️ **Note:** The XIAO ESP32-C3 uses a **32-bit RISC-V** architecture (not the Xtensa core used in ESP32-S3/S2). Most Arduino/ESP-IDF libraries are compatible, but verify any firmware examples or libraries against the C3 specifically — particularly around LittleFS, mDNS (`ESPmDNS`), and the HTTP server (`WebServer.h`). The original Half-Pill reference project used an ESP32-S3; some code may need minor adaptation.

---

## 6. System Architecture

```
┌──────────────────────────────┐        Wi-Fi (local network)
│         Web Browser          │ ◄────────────────────────────►  ┌─────────────────────┐
│  (phone, laptop, tablet)     │                                  │       ESP32          │
│  Add / view / delete         │                                  │                      │
│  reminders via web UI        │                                  │  ┌───────────────┐  │
└──────────────────────────────┘                                  │  │  Web Server   │  │
                                                                  │  │  (HTTP API)   │  │
                                                                  │  └───────┬───────┘  │
                                                                  │          │           │
                                                                  │  ┌───────▼───────┐  │
                                                                  │  │ Reminder Store│  │
                                                                  │  │  (in memory / │  │
                                                                  │  │   LittleFS)   │  │
                                                                  │  └───────┬───────┘  │
                                                                  │          │           │
                                                                  │  ┌───────▼───────┐  │
                                                                  │  │ Display + Touch│  │
                                                                  │  │  (GC9A01 +    │  │
                                                                  │  │   CST816S)    │  │
                                                                  │  └───────────────┘  │
                                                                  └─────────────────────┘
```

### Connectivity Recommendation: HTTP Polling

The ESP32 acts as both the **web server** and the **display controller**. There is no separate backend.

The web app is a static page served from the ESP32 itself (stored in LittleFS). It calls a simple REST API also hosted on the ESP32 to read and write reminders.

The display firmware runs a polling loop — every ~30 seconds it checks the reminder store and compares timestamps against the current time. When a reminder's scheduled time is reached, it transitions to the active state. This keeps the architecture dead simple with no WebSockets or MQTT broker to manage.

**Time sync:** The ESP32 syncs its clock via NTP on boot and periodically thereafter, so scheduled reminders fire at the correct wall-clock time.

---

## 7. Data Model

### Reminder

| Field | Type | Required | Notes |
|---|---|---|---|
| `id` | string (UUID) | Yes | Generated on creation |
| `name` | string | Yes | Max 40 characters. Displayed on device. |
| `scheduled_at` | ISO 8601 datetime | Yes | The exact date and time the reminder fires |
| `type` | enum | Yes | `generic` (only type in v1) |
| `recurrence` | string or null | No | Simple recurrence rule (see §7.1) |
| `status` | enum | Yes | `pending`, `active`, `completed` |
| `created_at` | ISO 8601 datetime | Yes | Set on creation |

### 7.1 Recurrence (v1 scope)

Simple human-readable rules only. No complex RRULE syntax in v1.

Supported patterns:
- `daily` — repeats every day at the same time
- `weekly` — repeats every week on the same day and time
- `weekdays` — repeats Monday–Friday at the same time

When a recurring reminder is dismissed (completed), the system automatically creates the next occurrence based on the rule and schedules it as a new `pending` reminder.

---

## 8. Web App

### 8.1 Access

Available at `http://trm.local/` on the local network. No installation required — any browser works. The ESP32 broadcasts its hostname via mDNS as `trm.local`.

### 8.2 Screens / Views

#### Reminder List (default view)
- Shows all upcoming (`pending` + `active`) reminders sorted by `scheduled_at` ascending.
- Each row shows: reminder name, scheduled date/time, recurrence badge (if set).
- A single **"+ Add Reminder"** button opens the add form.
- Reminders can be deleted from this view.

#### Add Reminder Form
Fields:
- **Name** — text input, required, max 40 characters.
- **Date** — date picker.
- **Time** — time picker.
- **Recurrence** — dropdown: None / Daily / Weekly / Weekdays.

On submit, the reminder is `POST`ed to the ESP32 API and the user is returned to the list view.

### 8.3 API Endpoints (ESP32 HTTP Server)

| Method | Path | Description |
|---|---|---|
| `GET` | `/api/reminders` | Returns all reminders as JSON array |
| `POST` | `/api/reminders` | Creates a new reminder |
| `DELETE` | `/api/reminders/:id` | Deletes a reminder |
| `PATCH` | `/api/reminders/:id` | Updates a reminder (used internally to mark completed) |
| `GET` | `/api/time` | Returns current ESP32 time (for debugging) |

---

## 9. Device Display

### 9.1 Display States

The device has two display states:

---

#### State 1: Idle

Shown whenever there are no active reminders.

**Layout (240×240 round display):**
- **"NEXT"** label — small, muted, uppercase
- **Reminder name** — large, centered, wraps if needed (max 40 chars)
- **Time of reminder** — below the name, medium weight (e.g. "Today, 6:30 PM" or "Mon, 9:00 AM")
- If no upcoming reminders exist: show a minimal "No reminders" state.

*Design details TBD by designer — layout above is a starting point.*

---

#### State 2: Active (Reminder Firing)

Triggered when `current_time >= scheduled_at` for a `pending` reminder.

**Behaviour:**
- The display transitions to the active state immediately when the polling loop detects the time has been reached.
- The reminder name is shown prominently, full-screen, centered.
- A visual affordance (e.g. pulsing ring, color shift) indicates this is an actionable state.
- The display remains in this state indefinitely until dismissed — it does not auto-dismiss.

**Dismissal:**
- User taps anywhere on the screen.
- The reminder is marked `completed` via an internal API call.
- If another reminder is already past its `scheduled_at` (queued), the display immediately transitions to show that one next.
- Otherwise, returns to Idle state.

**Queue behaviour:**
- Only one reminder is shown at a time.
- If multiple reminders have fired (their time has passed and they are `pending`), they are shown one at a time in chronological order.
- Each must be individually dismissed before the next appears.

---

### 9.2 Touch Input

Touch is handled by the CST816S capacitive controller over I2C.

- **Single tap anywhere** on the active screen = dismiss current reminder.
- No touch interaction required or expected on the idle screen in v1.

---

## 10. Firmware Behaviour

### Boot Sequence
1. Connect to Wi-Fi (SSID + password stored in firmware config).
2. Sync time via NTP.
3. Load reminders from LittleFS.
4. Start HTTP server.
5. Enter main loop.

### Main Loop
- Every ~30 seconds: check all `pending` reminders against current time.
- If any reminder's `scheduled_at ≤ now`, mark it `active` and trigger display update.
- Render the appropriate display state.
- Handle any incoming touch events.

### Persistence
- Reminders are stored as a JSON file on LittleFS (the ESP32's flash filesystem).
- Written on every create, update, or delete operation.
- Survives power cycles.

---

## 11. Out of Scope / Future Considerations

These are intentionally deferred from v1 but worth noting for later:

- **Multiple reminder types** (medication, task, event, etc.) with distinct visual treatments on device.
- **Snooze** — tap once to snooze for N minutes, tap again (or swipe) to dismiss.
- **Sound** — piezo buzzer on active state.
- **Completed log** — history view in the web app.
- **Authentication** — PIN or simple password to prevent unwanted changes on a shared network.
- **OTA firmware updates** via the web UI.
- **Idle screen v2** — clock, date, peek at 2nd upcoming reminder.

---

## 12. Open Questions

| # | Question | Owner | Status |
|---|---|---|---|
| 1 | Idle screen visual design — layout, typography, color palette | Designer | Open |
| 2 | Active state visual treatment — what does "you need to act on this" look like on a round display? | Designer | Open |
| 3 | What Wi-Fi credentials management looks like (hardcoded vs. captive portal setup) | Engineering | Open |
| 4 | Maximum number of reminders the ESP32 can store before LittleFS becomes a concern | Engineering | Open |
| 5 | NTP server choice and fallback if no internet is available | Engineering | Open |
