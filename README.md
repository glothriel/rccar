# LEGO Technic RC car

Single source of truth for the car's hardware, wiring, firmware, and controllers.

## Current revision

- Controller: Seeed Studio XIAO ESP32-S3 Sense product `113991115` with OV3660 camera.
- One protected 18650 cell feeds a TP4056 HW-373 charger/protection board.
- Switched protected battery power feeds the DRV8833 motor rail and Pololu U3V16F5 input in parallel.
- Regulated 5 V feeds the SG90 and XIAO; every ground is common.
- Control is available through the standalone web interface on the home Wi-Fi network.

Read [`docs/hardware.md`](docs/hardware.md) before connecting power. The editable wiring source is [`wiring/car.yml`](wiring/car.yml).

## Commands

```sh
uv sync
uv run wireviz wiring/car.yml
./local-dev build
```

WireViz also requires the system Graphviz `dot` executable. Go is not managed by `uv`.

## C++ editor support

Zed and OpenCode both use `clangd`. The repository contains shared `.clangd` settings, project-local Zed settings, and an OpenCode configuration that enables its built-in C++ language server.

Generate the compilation database used by `clangd`:

```sh
./local-dev clangd
```

Open the repository root in Zed. To refresh the database later, run the same command or use the `Firmware: refresh clangd database` project task. Restart the `clangd` language server after generating the database if files are already open.

Quit and restart OpenCode after the first checkout so it loads `opencode.json`; OpenCode downloads its built-in `clangd` when needed.

Refresh the database after changing the PlatformIO environment, framework dependencies, or build flags. `firmware/compile_commands.json` contains absolute paths for the local toolchain, so it is generated per checkout and ignored by Git.

## Web controller

Enter the home Wi-Fi SSID and password in `firmware/include/wifi_config.h`. The local file is ignored by Git; `firmware/include/wifi_config.example.h` is the tracked template for new checkouts.

Build, upload, and open the serial monitor:

```sh
./local-dev upload
./local-dev monitor
```

After connecting, the firmware prints `WEB READY` followed by its local IP address. Open that address on a device connected to the same Wi-Fi network. The page sends binary movement messages over `/ws/v1/control`; drag the joystick to control drive and steering together. Releasing it stops the drive and centers the steering. A 500 ms firmware timeout applies if the browser stops sending commands.

Drive commands preserve zero as stopped and linearly map every nonzero magnitude onto the motor's verified usable PWM range of 180–255.

The web controller is available to other devices on the same network and has no separate authentication. Steering uses the verified 10-degree minimum, 90-degree center, and 170-degree maximum from `firmware/include/pin_config.h`; verify the left/right direction on the raised car.
