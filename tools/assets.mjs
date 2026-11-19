// Dev-only M3 check: models loaded + downtown screenshots. Usage: node tools/assets.mjs
import fs from 'node:fs';

const url = process.argv[2] || 'http://127.0.0.1:8090/?v=m3';
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
const key = async (code, down) => evalJs(
  `window.dispatchEvent(new KeyboardEvent('${down ? 'keydown' : 'keyup'}',{code:'${code}'}))`);

await send('Page.enable');
await send('Runtime.enable');
await send('Log.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false });
await send('Page.navigate', { url });

const t0 = Date.now();
let ready = false;
while (Date.now() - t0 < 30000) {
  try { if (await evalJs('!!window.__mwReady')) { ready = true; break; } } catch {}
  await sleep(200);
}
await sleep(1500);
const boot = await evalJs(`({ ready: !!window.__mwReady, boot: document.body.dataset.mw,
  assets: window.__mwAssets, fps: window.__mwFps, dbg: window.__dbg })`);
await shot('m3_spawn');

await key('KeyC', true); await sleep(80); await key('KeyC', false);
await sleep(900);
await shot('m3_farcam');

// drive forward through the downtown blocks
await key('KeyW', true);
await sleep(4500);
await key('KeyW', false);
await sleep(400);
const drive = await evalJs(`({ dbg: window.__dbg, fps: window.__mwFps, street: window.__mwStreet })`);
await shot('m3_drive');

const logs = events.filter((e) => e.method === 'Log.entryAdded');
console.log(JSON.stringify({ ready, boot, drive, errors: logs.map((l) => l.params.entry.text).slice(0, 10) }, null, 2));
ws.close();
process.exit(0);
