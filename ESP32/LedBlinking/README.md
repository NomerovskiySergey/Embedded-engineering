# ESP32-C6-LCD-1.47 RGB Web Control

Control the onboard RGB LED from a browser over Wi-Fi. The page switches the red and blue channels independently: red + blue produces purple, and both channels off turns the RGB LED off. No external LEDs or wiring are required. The onboard LCD is not initialized by this project.

## Hardware

- Board: Waveshare ESP32-C6-LCD-1.47.
- Connected and identified chip: ESP32-C6FH8, 8 MB flash.
- RGB LED: GPIO8, WS2812-compatible protocol, RGB byte order for this board revision.
- Brightness: 32/255 per enabled channel; green remains zero.
- Transport: RMT at 10 MHz, 64 symbols, DMA disabled because ESP32-C6 RMT does not support it.
- Console: native USB Serial/JTAG, monitor configured for 115200 baud.

Some revisions have 4 MB flash. Verify your actual hardware before reusing the 8 MB configuration. If selecting red produces green, check the LED's RGB/GRB channel order; this project uses RGB following that observation on the connected board.

## Development environment

The configured workflow is **VS Code → PlatformIO → ESP-IDF → ESP32-C6**. VS Code is the editor, PlatformIO manages tools and build/upload commands, and ESP-IDF provides the embedded framework. This is a C/ESP-IDF project, not an Arduino sketch.

The setup was verified on macOS with:

| Component                          | Configuration                   |
| ---------------------------------- | ------------------------------- |
| VS Code extensions                 | PlatformIO IDE, Microsoft C/C++ |
| PlatformIO platform                | `espressif32@7.0.1`             |
| Framework resolved by the platform | ESP-IDF 6.0.1                   |
| Board profile                      | `esp32-c6-devkitc-1`            |
| Flash                              | 8 MB                            |
| LED component                      | `espressif/led_strip` 3.0.3     |

The generic DevKitC profile supplies the ESP32-C6 toolchain and chip settings. The Waveshare-specific RGB pin and color order are configured in the application.

### Setup from scratch

1. Install [Visual Studio Code](https://code.visualstudio.com/).
2. In Extensions, install **PlatformIO IDE** (`platformio.platformio-ide`) and ensure **C/C++** (`ms-vscode.cpptools`) is installed. Restart VS Code if prompted. PlatformIO IDE includes PlatformIO Core; a separate system-wide installation is unnecessary.
3. Clone or download this repository. Use **File → Open Folder** to open `ESP32/LedBlinking`, the folder containing `platformio.ini`.
4. Let PlatformIO initialize the project. Internet access is needed for the first build to download the platform, RISC-V compiler, ESP-IDF, build tools, and LED component.
5. Review `platformio.ini`: `src_dir = main` selects the source directory, `framework = espidf` selects ESP-IDF, and the environment defines the chip profile, flash size, upload speed, and monitor speed.
6. Review `sdkconfig.defaults`: it selects the ESP32-C6 target, 8 MB flash, and USB Serial/JTAG console.
7. In `main/main.c`, set `WIFI_SSID` and `WIFI_PASSWORD` to your **2.4 GHz** Wi-Fi network. Save the file. Keep real credentials out of commits and public repository content; they are currently stored directly in this source file.
8. Open the Command Palette and run **PlatformIO: Build**. Wait for `SUCCESS`. This verifies compilation without requiring a connected board.
9. Connect the board with a USB data cable. Use PlatformIO's device list to identify its serial port. The port on the configured Mac was `/dev/cu.usbmodem11101`; it can change after reconnecting.

The Espressif ESP-IDF VS Code extension is not required for this PlatformIO workflow. Use the PlatformIO commands below with this project.

### Build, upload, and monitor

Command Palette shortcut: **Cmd+Shift+P** on macOS, **Ctrl+Shift+P** on Windows/Linux.

1. Save all edited files.
2. Close an active Serial Monitor with **Ctrl+C** in its terminal so it releases the USB port.
3. Run **PlatformIO: Build** to compile. Wait for `SUCCESS`.
4. Run **PlatformIO: Upload** to build any changed sources and write the firmware. Wait for hash verification and `SUCCESS`. An alternative is **PlatformIO sidebar → Project Tasks → waveshare_esp32_c6_lcd → General → Upload**.
5. Run **PlatformIO: Serial Monitor**. If startup messages have already passed, press the board's RESET button.
6. Find `sta ip:` in the log. Open `http://<that-IP>` in a browser on the same network. The address is assigned by the router and may change.
7. Reload the page and confirm the heading reads **ESP32 onboard RGB**.

Equivalent commands in a PlatformIO terminal, from this project folder:

```sh
pio device list
pio run
pio run --target upload
pio device monitor
```

To select a port explicitly, append `--upload-port <port>` to the upload command or `--port <port>` to the monitor command.

## Project structure

```text
LedBlinking/
├── platformio.ini
├── CMakeLists.txt
├── sdkconfig.defaults
├── dependencies.lock
├── main/
│   ├── main.c
│   ├── CMakeLists.txt
│   └── idf_component.yml
├── .vscode/
│   └── extensions.json
├── .gitignore
└── README.md
```

### Build and upload

1. PlatformIO reads `platformio.ini` and selects the ESP32-C6 compiler and ESP-IDF.
2. The root CMake file loads ESP-IDF; `main/CMakeLists.txt` registers the application and its component dependencies.
3. ESP-IDF Component Manager resolves `main/idf_component.yml` using `dependencies.lock`, downloading the LED driver when needed.
4. ESP-IDF reads the SDK configuration. The compiler builds the application and framework; the linker produces `firmware.elf`, and tooling creates `firmware.bin`.
5. Upload uses esptool over USB to write the bootloader, partition table, and application, verify the written data, and reset the chip.

## Troubleshooting

- **No USB port:** check the data cable. Hold BOOT, press and release RESET, release BOOT, then retry Upload.
- **Port busy:** close Serial Monitor or other applications using the board's serial port.
- **Repeated Wi-Fi reconnection messages:** check the saved network name/password and availability of a 2.4 GHz network. Rebuild and upload after editing credentials.
- **Browser cannot connect:** use the IP printed in the current boot log and ensure the browser device can reach the board on the local network.
- **Old behavior after editing:** save the file and run Upload; Build alone does not change the firmware on the board.
- **Red appears green:** the LED revision may need a different channel order in `color_component_format`; this board's configuration is RGB.
- **LCD remains blank:** this application controls the RGB LED only; it does not include an LCD driver or display UI.

## References

- [Waveshare board documentation](https://docs.waveshare.com/ESP32-C6-LCD-1.47)
- [PlatformIO IDE for VS Code](https://docs.platformio.org/en/latest/integration/ide/vscode.html)
- [PlatformIO ESP32-C6 board profile](https://docs.platformio.org/en/latest/boards/espressif32/esp32-c6-devkitc-1.html)
- [Espressif LED strip component](https://components.espressif.com/components/espressif/led_strip/versions/3.0.3)
