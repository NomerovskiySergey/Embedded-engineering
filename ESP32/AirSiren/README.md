# AirSiren for ESP32-C6

An air-raid alert indicator for Dnipro, built for the Waveshare ESP32-C6-LCD-1.47. The device polls the public Tryvoha.online API over HTTPS and displays a fail-safe status on the LCD and onboard RGB LED.

> This is a supplementary information device. It does not replace official sirens, the official Air Alert app, State Emergency Service notifications, or instructions from public authorities.

## States

| State | Display | RGB LED |
| --- | --- | --- |
| Alert in the oblast or Dniprovskyi district | Red, `ALERT` | Red |
| Fresh, confirmed all-clear response | Green, `ALL CLEAR` | Green |
| Startup, no network, or API error | Amber, `STARTING` or `NO DATA` | Off |
| Data older than 90 seconds | Amber, `STALE` | Off |

The green LED is never enabled without a fresh, valid all-clear response. The API is polled every 30 seconds; failed requests use a bounded 5–60 second backoff.

The LCD backlight automatically dims from 40% to 8% between 23:00 and 07:00 Kyiv local time. An active alert always restores 40% brightness immediately. Wi-Fi, API polling, the LCD controller, and the RGB status LED remain active overnight; the firmware does not enter sleep mode.

During an official alert, the display also shows `THREAT: MISSILE`, `KAB`, `DRONE`, `RECON DRONE`, `WARNING`, `MULTIPLE`, or `UNKNOWN`.

## Hardware

- ESP32-C6FH8 with a verified 8 MB flash chip
- ST7789, 172 × 320: MOSI GPIO6, SCLK GPIO7, CS GPIO14, DC GPIO15, RESET GPIO21
- LCD backlight: GPIO22 at 40% brightness
- WS2812-compatible RGB LED: GPIO8, RGB component order
- Wi-Fi: 2.4 GHz networks only

The project uses a dedicated 4 MB factory application partition without OTA. This layout matches the verified 8 MB chip; do not flash this image onto a 4 MB board variant.

## Wi-Fi configuration

Copy `main/secrets.example.h` to `main/secrets.h` and replace the placeholder values:

```cpp
#define AIRSIREN_WIFI_SSID "YOUR_2_4_GHZ_WIFI_NAME"
#define AIRSIREN_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
```

`secrets.h` is ignored by Git. Never add real passwords to tracked files.

## Build, upload, and monitor

Open `ESP32/AirSiren` in VS Code with PlatformIO, or run:

```sh
/Users/sergejnomerovskij/.platformio/penv/bin/pio run
/Users/sergejnomerovskij/.platformio/penv/bin/pio run --target upload
/Users/sergejnomerovskij/.platformio/penv/bin/pio device monitor
```

Firmware upload is not part of the automated checks. Before uploading, confirm that the connected board is an ESP32-C6FH8 with 8 MB flash and that the correct USB port is selected.

## Tests

The host tests perform no network requests. They cover response parsing, fail-closed behavior, data freshness, and the polling schedule:

```sh
sh test/run_host_tests.sh
```

## Data sources

- Alert endpoint: `https://tryvoha.online/api/v1/alerts/dnipropetrovska`
- Secondary live threat feed: `https://tryvoha.online/api/events`
- Documentation: <https://tryvoha.online/api>
- On-screen attribution: `DATA: TRYVOHA.ONLINE`

Tryvoha.online is an independent, unofficial aggregator with no availability guarantee. Provider-specific code is isolated so that NEPTUN or an official API can be added later without changing the LED or display state logic.

Threat types are secondary information generated automatically by the service from public reports. The live endpoint is public but undocumented, and its events are not radar data. If this feed is unavailable or stale, the official red alert state is preserved and the display shows `THREAT: UNKNOWN`.
