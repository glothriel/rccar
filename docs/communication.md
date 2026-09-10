# Communication Design

All connections use plain HTTP or WebSocket on a trusted network. Examples use `car.local:80` for the ESP and `go.local:8080` for Go.

## Topology

The ESP accepts local control but does not accept connections from Go:

```text
Local browser --WS--> ESP /ws/v1/control

ESP --WS--> Go /ws/v1/car/control
ESP --WS--> Go /ws/v1/car/video

Remote browser --WS--> Go /ws/v1/browser/control
Remote browser <--WS-- Go /ws/v1/browser/video
```

Relay mode is configured on the ESP settings page. When enabled, the ESP maintains both outbound Go sockets with independent exponential reconnect backoff and jitter capped at 60 seconds. Losing either socket does not affect local control.

There are no car IDs, protocol sessions, command sequence numbers, or controller ownership rules on the ESP. This version supports one car per Go process. A later multi-car service can add routing outside the ESP protocol.

## ESP HTTP

| Endpoint | Purpose |
|---|---|
| `GET /` | Standalone controller and relay settings page |
| `GET /api/v1/status` | Current relay, camera, and fault status |
| `GET /api/v1/relay` | Current relay configuration |
| `PUT /api/v1/relay` | Replace relay configuration and restart relay tasks |

Example relay update:

```http
PUT /api/v1/relay HTTP/1.1
Content-Type: application/json

{"enabled":true,"server_url":"ws://go.local:8080"}
```

```json
{"enabled":true,"server_url":"ws://go.local:8080","control":"connected","video":"connected","last_error":null}
```

Invalid settings return `400` without replacing working NVS values. Disabling relay closes both outbound sockets, stops the camera, and leaves the local server running.

## Binary Encoding

WebSocket messages are binary. The first byte is the message type; the remaining bytes have a fixed meaning for that type. Integers use network byte order. Signed fields use two's complement. The `/v1/` endpoint path versions the format.

An unexpected type, length, or out-of-range value closes that WebSocket. A malformed message never changes actuator output.

## Control Protocol

The ESP local endpoint and Go relay endpoint carry exactly the same protocol:

```text
Local browser <-> ESP /ws/v1/control
ESP <-> Go /ws/v1/car/control
```

Any connected controller may send movement. The ESP stores one synchronized latest-command value; whichever valid message arrived most recently wins. There is no source priority, reservation, or command queue on the ESP.

### Set movement: `0x01`

Controller to ESP, exactly 5 bytes:

| Bytes | Field | Example |
|---|---|---:|
| `0` | type | `0x01` |
| `1..2` | drive, signed 16-bit, `-255..255` | `-120` |
| `3..4` | steering, signed 16-bit, `-100..100` | `25` |

Example `drive=-120, steering=25`:

```text
01 FF 88 00 19
```

Stop uses the same message:

```text
01 00 00 00 00
```

Only receipt of a valid `0x01` refreshes the 500 ms failsafe. Opening a socket, telemetry, video, ping, and pong do not.

### Movement state: `0x02`

ESP to every connected control socket, exactly 9 bytes:

| Bytes | Field | Example |
|---|---|---:|
| `0` | type | `0x02` |
| `1..2` | commanded drive | `-120` |
| `3..4` | commanded steering | `25` |
| `5..6` | applied drive PWM | `-215` |
| `7..8` | applied steering | `25` |

Example:

```text
02 FF 88 00 19 FF 29 00 19
```

The ESP sends current state immediately after a control socket connects. It broadcasts another `0x02` to all healthy local and relay control sockets whenever output changes, including a failsafe stop. Applied values describe firmware output, not measured wheel or steering position.

### Telemetry: `0x03`

ESP to every connected control socket, exactly 33 bytes:

| Bytes | Field | Example |
|---|---|---:|
| `0` | type | `0x03` |
| `1..8` | uptime, unsigned 64-bit milliseconds | `932840` |
| `9..10` | Wi-Fi RSSI, signed 16-bit dBm | `-61` |
| `11..14` | free internal heap, unsigned 32-bit bytes | `183240` |
| `15..18` | largest internal block, unsigned 32-bit bytes | `90112` |
| `19..22` | free PSRAM, signed 32-bit bytes | `7213040` |
| `23..26` | largest PSRAM block, signed 32-bit bytes | `4194304` |
| `27` | link flags | `0x07` |
| `28` | camera state | `0x02` |
| `29..32` | fault flags, unsigned 32-bit bitset | `0` |

PSRAM fields use `-1` when unavailable. Link flag `0x01` means Wi-Fi connected, `0x02` control relay connected, and `0x04` video relay connected. Camera state is `0` off, `1` starting, `2` streaming, or `3` fault. Fault bit assignments are added only with the code that can raise them.

Telemetry is sent every 500 ms while movement is fresh or video is enabled, every 30 seconds otherwise, and immediately when link, camera, or fault state changes. A slow control peer keeps only the latest unsent movement state and telemetry; persistent write failure closes only that peer.

All control peers use WebSocket ping/pong every 10 seconds and close after two unanswered pings. No application handshake is required. Reconnection starts stopped: Go discards its pending movement when its socket closes and does not replay it after reconnecting.

## Video Protocol

The ESP connects to `ws://go.local:8080/ws/v1/car/video`. The socket may remain connected while the camera is off. Go controls capture over this same socket.

### Set video: `0x10`

Go to ESP, exactly 2 bytes:

| Bytes | Field | Start | Stop |
|---|---|---:|---:|
| `0` | type | `0x10` | `0x10` |
| `1` | enabled | `0x01` | `0x00` |

```text
10 01  # first remote viewer arrived
10 00  # last remote viewer left
```

`START` initializes capture. `STOP` stops and deinitializes it. Video-socket loss has the same effect as `STOP`. After reconnecting, the camera remains off until Go sends the current desired state again.

### JPEG frame: `0x11`

ESP to Go, one frame per WebSocket message. A 21-byte header is followed by exactly one sensor-produced JPEG:

| Bytes | Field | Example |
|---|---|---:|
| `0` | type | `0x11` |
| `1..4` | frame sequence, unsigned 32-bit | `841` |
| `5..12` | capture timestamp, unsigned 64-bit microseconds since boot | `932800123` |
| `13..14` | width, unsigned 16-bit | `640` |
| `15..16` | height, unsigned 16-bit | `480` |
| `17..20` | JPEG length, unsigned 32-bit | `42817` |
| `21..42837` | JPEG payload | `FF D8 ... FF D9` |

The ESP retains at most one unsent frame. Go validates lengths but does not decode JPEG. Resolution, quality, and frame rate remain runtime configuration chosen after hardware measurement.

## Remote Browser Protocol

Go serves the remote UI and owns visitor names and the FIFO controller queue. None of this state reaches the ESP.

The browser opens `/ws/v1/browser/control` and sends JSON because queue state is variable-length UI data:

```json
{"type":"join","name":"Alex"}
```

Go identifies the visitor by that WebSocket, not by a client-supplied ID:

```json
{"type":"queue","you":1,"controller":"Marta","queue":["Marta","Alex"]}
```

Only the queue head's movement is accepted:

```json
{"type":"command","drive":80,"steering":-10}
```

Go converts it to control message `0x01` and keeps only the latest unsent movement. On release or browser disconnect, Go writes a zero movement before allowing the next controller to move:

```json
{"type":"release"}
```

Go converts ESP `0x02` and `0x03` messages to state broadcasts for all remote browsers:

```json
{"type":"state","commanded":{"drive":-120,"steering":25},"applied":{"drive_pwm":-215,"steering":25},"rssi_dbm":-61,"camera":"streaming","faults":[]}
```

The browser opens `/ws/v1/browser/video` to view video. Go counts these sockets:

```text
first browser video socket opens -> Go sends 10 01 to ESP
last browser video socket closes -> Go sends 10 00 to ESP
```

Go forwards each `0x11` message unchanged to every browser video socket. Each browser writer has one latest-frame slot; slow viewers drop old frames independently.

## End-to-End Behavior

Local and remote movement deliberately compete:

```text
local sends drive=80       -> ESP applies 80 and broadcasts state
Go sends drive=-120 next   -> ESP applies -120 and broadcasts state
local sends steering=20    -> full local command wins; ESP applies its drive and steering
no valid command for 500ms -> ESP applies zero and broadcasts state
```

Both movement fields are always present. A controller that wants to change steering while preserving drive must send both desired values.

Remote video lifecycle:

```text
ESP connects outbound control and video sockets
Go has no video viewers; camera remains off
first viewer opens; Go sends 10 01
ESP starts camera and sends latest 0x11 frames
last viewer closes; Go sends 10 00
ESP stops camera but keeps both relay sockets connected
```
