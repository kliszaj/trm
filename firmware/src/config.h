#pragma once

// ── Wi-Fi ─────────────────────────────────────────────────────────────────────
#define WIFI_SSID "YourSSID"
#define WIFI_PASS "YourPassword"

// ── Time ──────────────────────────────────────────────────────────────────────
// POSIX TZ string for Europe/Stockholm (CET-1 / CEST with DST)
#define TZ_STRING  "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_SERVER "pool.ntp.org"

// ── Hostname / mDNS ───────────────────────────────────────────────────────────
#define HOSTNAME "trm"

// ── Reminders ─────────────────────────────────────────────────────────────────
#define MAX_REMINDERS     50
#define REMINDERS_PATH    "/reminders.json"
#define POLL_INTERVAL_MS  30000   // 30 seconds

// ── Touch (CHSC6X, I2C on Seeed Round Display) ───────────────────────────────
#define TOUCH_SDA  6    // D4
#define TOUCH_SCL  7    // D5
// RST shared with display (D0 / GPIO2), INT on D7 / GPIO20 — both unused here

// ── Display (GC9A01, SPI) — see lgfx_config.h ────────────────────────────────
// Pin definitions are in lgfx_config.h for LovyanGFX
 