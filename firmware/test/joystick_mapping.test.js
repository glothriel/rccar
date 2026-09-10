const assert = require('node:assert/strict');
const fs = require('node:fs');
const test = require('node:test');
const vm = require('node:vm');

const header = fs.readFileSync(new URL('../include/web_ui.h', `file://${__dirname}/`), 'utf8');
const source = header.match(/<script>([\s\S]*?)<\/script>/)[1];
const mappingSource = source.match(/const DEADZONE[\s\S]*?function mapJoystick\([\s\S]*?\n    }/)[0];
const context = {};
vm.runInNewContext(`${mappingSource}; result = mapJoystick`, context);
const mapJoystick = context.result;

const frameSource = source.match(/function movementFrame\([\s\S]*?\n    }/)[0];
vm.runInNewContext(`${frameSource}; result = movementFrame`, context);
const movementFrame = context.result;

test('maps cardinal directions to semantic command signs and full scale', () => {
  assert.deepEqual({ ...mapJoystick(0, 0, 100) }, { drive: 0, steering: 0, x: 0, y: 0 });
  assert.deepEqual({ ...mapJoystick(0, -100, 100) }, { drive: 255, steering: 0, x: 0, y: -100 });
  assert.deepEqual({ ...mapJoystick(0, 100, 100) }, { drive: -255, steering: 0, x: 0, y: 100 });
  assert.deepEqual({ ...mapJoystick(100, 0, 100) }, { drive: 0, steering: 80, x: 100, y: 0 });
  assert.deepEqual({ ...mapJoystick(-100, 0, 100) }, { drive: 0, steering: -80, x: -100, y: 0 });
});

test('maps proportional diagonal input to drive and steering together', () => {
  const mapped = mapJoystick(60, -80, 100);
  assert.equal(mapped.drive, 204);
  assert.equal(mapped.steering, 48);
});

test('clamps diagonally to the circular boundary proportionally', () => {
  const mapped = mapJoystick(100, -100, 100);
  assert.equal(mapped.drive, 180);
  assert.equal(mapped.steering, 57);
  assert.ok(Math.abs(Math.hypot(mapped.x, mapped.y) - 100) < 1e-9);
});

test('applies the center deadzone and handles unusable geometry', () => {
  assert.deepEqual({ ...mapJoystick(4, -4, 100) }, { drive: 0, steering: 0, x: 4, y: -4 });
  assert.deepEqual({ ...mapJoystick(10, 10, 0) }, { drive: 0, steering: 0, x: 0, y: 0 });
});

test('encodes movement as the v1 binary control frame', () => {
  const bytes = new Uint8Array(movementFrame(-120, 25));
  assert.deepEqual([...bytes], [0x01, 0xff, 0x88, 0x00, 0x19]);
});
