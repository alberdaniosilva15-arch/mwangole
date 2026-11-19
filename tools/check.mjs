// Dev-only runtime check over CDP. Usage: node tools/check.mjs [url]
import fs from 'node:fs';

const url = process.argv[2] || 'http://127.0.0.1:8090/?v=dev';
const out = 'shots';

const targets = await (await fetch('http://127.0.0.1:9222/json')).json();
const page = targets.find((t) => t.type === 'page');
if (!page) throw new Error('no page target');

const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
let id = 0;
const pending = new Map();
const events = [];
ws.onmessage = (m) => {
  const msg = JSON.parse(m.data);
  if (msg.id && pending.has(msg.id)) {
    const { res, rej } = pending.get(msg.id);
    pending.delete(msg.id);
    msg.error ? rej(new Error(JSON.stringify(msg.error))) : res(msg.result);
  } else if (msg.method) {
    events.push(msg);
  }
};
const send = (method, params = {}) => new Promise((res, rej) => {
  const n = ++id;
  pending.set(n, { res, rej });
  ws.send(JSON.stringify({ id: n, method, params }));
});
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const evalJs = async (expr) => {
  const r = await send('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true });
  if (r.exceptionDetails) throw new Error(r.exceptionDetails.text + ' ' + (r.exceptionDetails.exception?.description || ''));
  return r.result.value;
};
const shot = async (name) => {
  const r = await send('Page.captureScreenshot', { format: 'png' });
  fs.writeFileSync(`${out}/${name}.png`, Buffer.from(r.data, 'base64'));
};

await send('Page.enable');
await send('Runtime.enable');
await send('Log.enable');

const waitReady = async (ms = 25000) => {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) {
    try { if (await evalJs('!!window.__mwReady')) return true; } catch {}
    await sleep(200);
  }
  return false;
};

await send('Emulation.setDeviceMetricsOverride', { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false });
await send('Page.navigate', { url });
const ready = await waitReady();
await sleep(1200);
const boot = await evalJs(`({
  ready: !!window.__mwReady, boot: document.body.dataset.mw,
  dbg: window.__dbg, fps: window.__mwFps,
  size: [document.getElementById('game')?.width, document.getElementById('game')?.height],
  touch: getComputedStyle(document.getElementById('touch')).display,
})`);
await shot('cdp_desktop');

await evalJs(`window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyW'}))`);
await sleep(2200);
const drive = await evalJs(`({dbg: window.__dbg, fps: window.__mwFps, street: window.__mwStreet})`);
await shot('cdp_drive');

// steer right while driving: yaw must decrease and x increase (facing -z)
await evalJs(`window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyD'}))`);
await sleep(600);
const steer = await evalJs(`({dbg: window.__dbg})`);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyD'}))`);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyW'}))`);

// exit car
await evalJs(`window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyE'}))`);
await sleep(300);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyE'}))`);
await sleep(600);
const onfoot = await evalJs(`({inCar: window.__mwInCar, dbg: window.__dbg})`);
await shot('cdp_foot');

// walk on foot, then enter the car again
await evalJs(`window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyW'}))`);
await sleep(700);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyW'}))`);
await sleep(300);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyE'}))`);
await sleep(60);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyE'}))`);
await sleep(600);
const reenter = await evalJs(`({inCar: window.__mwInCar})`);

// far camera + reset
await evalJs(`window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyC'}))`);
await sleep(80);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyC'}))`);
await sleep(900);
await shot('cdp_farcam');
await evalJs(`window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyR'}))`);
await sleep(80);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyR'}))`);
await sleep(400);
const reset = await evalJs(`({inCar: window.__mwInCar, dbg: window.__dbg})`);

// steer left while driving: yaw must increase and x decrease
await evalJs(`window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyW'}))`);
await sleep(900);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyA'}))`);
await sleep(500);
const steerLeft = await evalJs(`({dbg: window.__dbg})`);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyA'}))`);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyW'}))`);

// drive at the building block ahead: hold W until beside the block, turn in, and
// confirm the car stays outside the wall with speed stalled
await evalJs(`window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyR'}))`);
await sleep(80);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyR'}))`);
await sleep(300);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyW'}))`);
{
  const t0 = Date.now();
  while (Date.now() - t0 < 9000) {
    const z = await evalJs('window.__dbg.z');
    if (z <= 176) break;
    await sleep(100);
  }
}
await evalJs(`window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyD'}))`);
await sleep(1000);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyD'}))`);
await sleep(2500);
const crash1 = await evalJs(`({dbg: window.__dbg})`);
await sleep(900);
const crash2 = await evalJs(`({dbg: window.__dbg})`);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyW'}))`);
await shot('cdp_crash');

// mobile portrait with touch
await send('Emulation.setDeviceMetricsOverride', { width: 390, height: 844, deviceScaleFactor: 2, mobile: true });
await send('Emulation.setTouchEmulationEnabled', { enabled: true, maxTouchPoints: 5 });
await send('Page.navigate', { url: url + '&m=1' });
const ready2 = await waitReady();
await sleep(1200);
const mobile = await evalJs(`({
  ready: !!window.__mwReady, size: [document.getElementById('game')?.width, document.getElementById('game')?.height],
  css: [document.getElementById('game')?.clientWidth, document.getElementById('game')?.clientHeight],
  touch: getComputedStyle(document.getElementById('touch')).display,
  stick: getComputedStyle(document.getElementById('stick')).display,
  fps: window.__mwFps,
})`);
await shot('cdp_mobile');

// touch: drag the virtual stick up-right (throttle + steer right)
const sr = await evalJs(`(() => { const r = document.getElementById('stick').getBoundingClientRect();
  return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; })()`);
const tp = (type, x, y) => send('Input.dispatchTouchEvent', {
  type, touchPoints: type === 'touchEnd' ? [] : [{ x, y, id: 1 }] });
await tp('touchStart', sr.x, sr.y);
await sleep(80);
await tp('touchMove', sr.x + 42, sr.y - 55);
await sleep(1400);
const touchDrive = await evalJs(`({dbg: window.__dbg})`);
await tp('touchEnd', 0, 0);
await shot('cdp_touchdrive');

const logs = events.filter((e) => e.method === 'Log.entryAdded');
console.log(JSON.stringify({ ready, boot, drive, steer, onfoot, reenter, reset, steerLeft, crash1, crash2,
  ready2, mobile, touchDrive, errors: logs.map((l) => l.params.entry.text).slice(0, 10) }, null, 2));
ws.close();
process.exit(0);
