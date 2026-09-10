#pragma once

constexpr char kIndexHtml[] = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
  <title>RC car</title>
  <style>
    :root { color-scheme: dark; font-family: system-ui, sans-serif; }
    * { box-sizing: border-box; }
    body { margin: 0; height: 100dvh; overflow: hidden; background: radial-gradient(circle at 50% 35%, #263441, #0c1116 70%); color: #eef5f8; }
    main { height: 100%; display: grid; place-items: center; padding: max(20px, env(safe-area-inset-top)) max(20px, env(safe-area-inset-right)) max(20px, env(safe-area-inset-bottom)) max(20px, env(safe-area-inset-left)); }
    #joystick { position: relative; width: min(88vmin, 720px); aspect-ratio: 1; border: 2px solid #718493; border-radius: 50%; background: radial-gradient(circle, #25333d 0 12%, #1a252d 13% 49%, #131c23 50%); box-shadow: inset 0 0 45px #0009, 0 20px 60px #0008; touch-action: none; user-select: none; }
    #joystick::before, #joystick::after { content: ''; position: absolute; background: #91a3ae30; pointer-events: none; }
    #joystick::before { width: 1px; height: 86%; left: 50%; top: 7%; }
    #joystick::after { height: 1px; width: 86%; top: 50%; left: 7%; }
    #knob { position: absolute; width: 31%; aspect-ratio: 1; left: 50%; top: 50%; translate: -50% -50%; border: 2px solid #d7e5eb; border-radius: 50%; background: radial-gradient(circle at 35% 28%, #6ce2ff, #1688aa 55%, #07516c); box-shadow: 0 10px 24px #0009, inset 0 2px 8px #ffffff66; pointer-events: none; will-change: transform; }
    #readout { position: fixed; top: max(10px, env(safe-area-inset-top)); left: max(10px, env(safe-area-inset-left)); padding: .45em .6em; border: 1px solid #91a3ae55; border-radius: 6px; background: #071016bb; color: #c8d8df; font: clamp(.7rem, 2vmin, .9rem)/1.35 ui-monospace, SFMono-Regular, Consolas, monospace; font-variant-numeric: tabular-nums; white-space: pre; pointer-events: none; }
  </style>
</head>
<body>
  <main>
    <div id="joystick" role="application" aria-label="Drive and steering joystick"><div id="knob"></div></div>
  </main>
  <output id="readout" aria-live="polite">connecting
  <script>
    const DEADZONE = 0.08;
    const MAX_STEERING = 80;

    function mapJoystick(dx, dy, radius) {
      if (radius <= 0) return { drive: 0, steering: 0, x: 0, y: 0 };
      const distance = Math.hypot(dx, dy);
      const clampedDistance = Math.min(distance, radius);
      const factor = distance ? clampedDistance / distance : 0;
      const x = dx * factor;
      const y = dy * factor;
      const magnitude = clampedDistance / radius;
      if (magnitude <= DEADZONE) return { drive: 0, steering: 0, x, y };
      const output = (magnitude - DEADZONE) / (1 - DEADZONE);
      return {
        drive: Math.round(-y / clampedDistance * output * 255) || 0,
        steering: Math.round(x / clampedDistance * output * MAX_STEERING) || 0,
        x,
        y
      };
    }

    function movementFrame(drive, steering) {
      const frame = new ArrayBuffer(5);
      const view = new DataView(frame);
      view.setUint8(0, 0x01);
      view.setInt16(1, drive, false);
      view.setInt16(3, steering, false);
      return frame;
    }

    const joystick = document.querySelector('#joystick');
    const knob = document.querySelector('#knob');
    const readout = document.querySelector('#readout');
    let activePointer = null;
    let command = { drive: 0, steering: 0 };
    let applied = { drive: 0, steering: 0 };
    let socket;
    let reconnectDelay = 250;

    function render(connection = socket?.readyState === WebSocket.OPEN ? 'connected' : 'connecting') {
      readout.value = `${connection}\ndrive    ${String(applied.drive).padStart(4)}\nsteering ${String(applied.steering).padStart(4)}`;
    }

    function connect() {
      socket = new WebSocket(`ws://${location.host}/ws/v1/control`);
      socket.binaryType = 'arraybuffer';
      socket.addEventListener('open', () => {
        reconnectDelay = 250;
        render('connected');
        if (activePointer !== null) send();
      });
      socket.addEventListener('message', event => {
        if (!(event.data instanceof ArrayBuffer) || event.data.byteLength !== 9) return;
        const view = new DataView(event.data);
        if (view.getUint8(0) !== 0x02) return;
        applied = { drive: view.getInt16(5, false), steering: view.getInt16(7, false) };
        render();
      });
      socket.addEventListener('close', () => {
        render('disconnected');
        setTimeout(connect, reconnectDelay);
        reconnectDelay = Math.min(reconnectDelay * 2, 5000);
      });
      socket.addEventListener('error', () => socket.close());
    }

    function send() {
      if (socket?.readyState === WebSocket.OPEN) {
        socket.send(movementFrame(command.drive, command.steering));
      }
    }

    function update(event) {
      const box = joystick.getBoundingClientRect();
      const knobRadius = knob.offsetWidth / 2;
      const radius = Math.max(0, Math.min(box.width, box.height) / 2 - knobRadius);
      const mapped = mapJoystick(event.clientX - (box.left + box.width / 2), event.clientY - (box.top + box.height / 2), radius);
      knob.style.transform = `translate(${mapped.x}px, ${mapped.y}px)`;
      command = { drive: mapped.drive, steering: mapped.steering };
      send();
    }

    function stop() {
      const wasActive = activePointer !== null;
      activePointer = null;
      command = { drive: 0, steering: 0 };
      knob.style.transform = '';
      if (wasActive) send();
    }

    joystick.addEventListener('pointerdown', event => {
      event.preventDefault();
      if (activePointer !== null) return;
      activePointer = event.pointerId;
      joystick.setPointerCapture(event.pointerId);
      update(event);
    });
    joystick.addEventListener('pointermove', event => {
      if (event.pointerId === activePointer) update(event);
    });
    ['pointerup', 'pointercancel', 'lostpointercapture'].forEach(name =>
      joystick.addEventListener(name, event => {
        if (activePointer === null || event.pointerId === activePointer) stop();
      })
    );

    setInterval(() => {
      if (activePointer !== null) send();
    }, 200);
    window.addEventListener('blur', stop);
    document.addEventListener('visibilitychange', () => {
      if (document.hidden) stop();
    });
    connect();
  </script>
</body>
</html>
)HTML";
