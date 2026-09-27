# Codex Limits Display for ESP32-C6

A standalone firmware for the Waveshare ESP32-C6-LCD-1.47 that displays the
current Codex quota windows, remaining percentages, reset times, and
connectivity/freshness state.

This project does not modify AirSiren. Flashing it replaces the currently
installed firmware on the board; AirSiren can be restored by flashing that
project again.

## Architecture

The ESP32 never receives ChatGPT or Codex account credentials. A small bridge
on a logged-in Mac starts the local Codex app-server, calls only
`account/rateLimits/read`, removes unrelated account fields, and exposes one
read-only endpoint to the display.

```text
Codex app-server (local stdio)
        |
Python read-only bridge
        |
trusted LAN HTTP + display-only shared secret
        |
ESP32-C6 LCD
```

The bridge must not be port-forwarded or exposed to the internet. The
display-only secret prevents accidental access but plain HTTP does not protect
against an attacker already able to capture trusted-LAN traffic.

## Display

The 320 x 172 landscape UI is an LVGL 9 cyberpunk dashboard rendered through
the native `esp_lcd` ST7789 driver. It uses two bounded cards, cyan progress
bars, a pink title, and an emerald `ACTIVE` state. When live bridge data is
available, the cards show bounded quota-window percentages and reset/update
information; stale or disconnected data changes the header state to `STALE`.

LVGL uses a static 320 x 20 RGB565 partial draw buffer (12,800 bytes), so a
full-screen framebuffer is not required. The flush callback validates every
region and waits for the SPI color transaction before releasing LVGL.

The firmware polls every 60 seconds. HTTP runs on a dedicated FreeRTOS task, so
the main loop remains responsive. Failed requests retry with a bounded
5-60-second exponential backoff.

## Configure the firmware

Copy `main/secrets.example.h` to `main/secrets.h` and set:

- your 2.4 GHz Wi-Fi SSID and password;
- the bridge URL using the Mac's private LAN address;
- a long random display-only shared secret.

`main/secrets.h` is ignored by Git.

## Start the bridge

Use the same secret configured on the ESP32. Read it without putting its value
in shell history, export it for the bridge process, and replace the sample
address with the Mac's private LAN address:

```sh
read -s CODEX_LIMITS_SHARED_SECRET
export CODEX_LIMITS_SHARED_SECRET
python3 -m bridge.codex_limits_bridge \
  --listen 192.168.1.10 \
  --port 8765
```

The bridge refuses to start when `CODEX_LIMITS_SHARED_SECRET` is absent or
empty. Run `unset CODEX_LIMITS_SHARED_SECRET` after stopping it if desired.

The `codex` executable must be available in `PATH`, and Codex must already
be logged in on that Mac. The bridge exposes only:

```text
GET /v1/limits
Authorization: Bearer <display-only-secret>
```

## Test and build

```sh
sh test/run_host_tests.sh
python3 -m unittest discover -s bridge/tests
/Users/sergejnomerovskij/.platformio/penv/bin/pio run
```

The configured hardware is ESP32-C6FH8 with 8 MB flash and an ST7789
172 x 320 LCD:

- MOSI GPIO6
- SCLK GPIO7
- CS GPIO14
- DC GPIO15
- RESET GPIO21
- backlight GPIO22 driven HIGH

Firmware upload and physical verification are intentionally separate from the
automated checks.
