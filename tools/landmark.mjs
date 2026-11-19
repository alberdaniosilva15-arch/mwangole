// Dev-only: opens an isolated tab, screenshots spawn + aerial, closes it.
// Usage: node tools/landmark.mjs [url]
import fs from 'node:fs';

const url = process.argv[2] || 'http://127.0.0.1:8090/?v=landmark';
const out = 'shots';
fs.mkdirSync(out, { recursive: true });

const target = await (await fetch('http://127.0.0.1:9222/json/new?' + encodeURIComponent(url), { method: 'PUT' })).json();
const ws = new WebSocket(target.webSocketDebuggerUrl);
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
  } else if (msg.method) events.push(msg);
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
await send('Emulation.setDeviceMetricsOverride', { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false });

const t0 = Date.now();
let ready = false;
while (Date.now() - t0 < 40000) {
  try { if (await evalJs('!!window.__mwReady')) { ready = true; break; } } catch {}
  await sleep(250);
}
await sleep(1500);
const boot = await evalJs(`({ ready: !!window.__mwReady, assets: window.__mwAssets, dbg: window.__dbg, fps: window.__mwFps })`);
await shot('landmark_spawn');

await evalJs(`window.__mwModule && window.__mwModule._mw_debug_top(1)`);
await sleep(400);
await shot('landmark_top');
await evalJs(`window.__mwModule && window.__mwModule._mw_debug_top(0)`);
await sleep(200);

await evalJs(`window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyW'}))`);
await sleep(5000);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyW'}))`);
await sleep(600);
await shot('landmark_drive');

await evalJs(`window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyW'}))`);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyA'}))`);
await sleep(1600);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyA'}))`);
await sleep(2600);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyW'}))`);
await sleep(500);
await shot('landmark_turn');

await evalJs(`window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyC'}))`);
await sleep(80);
await evalJs(`window.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyC'}))`);
await sleep(1200);
await shot('landmark_farcam');

const logs = events.filter((e) => e.method === 'Log.entryAdded');
console.log(JSON.stringify({ ready, boot, errors: logs.map((l) => l.params.entry.text).slice(0, 8) }, null, 2));
ws.close();
await fetch('http://127.0.0.1:9222/json/close/' + target.id);
process.exit(0);
