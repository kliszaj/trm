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

// ── Touch (CST816S, I2C) ──────────────────────────────────────────────────────
#define TOUCH_SDA  6
#define TOUCH_SCL  7
#define TOUCH_INT  -1   // Interrupt pin (not used in polling mode)
#define TOUCH_RST  -1

// ── Display (GC9A01, SPI) — see tft_setup.h ──────────────────────────────────
// Pin definitions are in tft_setup.h for TFT_eSPI compatibility
