#pragma once

#include <Arduino.h>

constexpr char kIndexHtml[] PROGMEM = R"HTML(
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
  <output id="readout" aria-live="polite">drive      0\nsteering   0</output>
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

    const joystick = document.querySelector('#joystick');
    const knob = document.querySelector('#knob');
    const readout = document.querySelector('#readout');
    let activePointer = null;
    let command = { drive: 0, steering: 0 };
    let sendPending = false;
    let sending = false;

    async function send() {
      sendPending = true;
      if (sending) return;
      sending = true;
      while (sendPending) {
        sendPending = false;
        const next = command;
        const controller = new AbortController();
        const timeout = setTimeout(() => controller.abort(), 150);
        try {
          await fetch(`/command?drive=${next.drive}&steering=${next.steering}`, {
            method: 'POST', cache: 'no-store', signal: controller.signal
          });
        } catch (_) {
        } finally {
          clearTimeout(timeout);
        }
      }
      sending = false;
    }

    function update(event) {
      const box = joystick.getBoundingClientRect();
      const knobRadius = knob.offsetWidth / 2;
      const radius = Math.max(0, Math.min(box.width, box.height) / 2 - knobRadius);
      const mapped = mapJoystick(event.clientX - (box.left + box.width / 2), event.clientY - (box.top + box.height / 2), radius);
      knob.style.transform = `translate(${mapped.x}px, ${mapped.y}px)`;
      command = { drive: mapped.drive, steering: mapped.steering };
      readout.value = `drive    ${String(command.drive).padStart(4)}\nsteering ${String(command.steering).padStart(4)}`;
      send();
    }

    function stop() {
      const wasActive = activePointer !== null;
      activePointer = null;
      command = { drive: 0, steering: 0 };
      knob.style.transform = '';
      readout.value = 'drive      0\nsteering   0';
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
  </script>
</body>
</html>
)HTML";
