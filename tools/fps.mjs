// Dev-only: sample fps over time at a spot. Usage: node tools/fps.mjs x z yaw
import fs from 'node:fs';

const [x, z, yaw] = process.argv.slice(2);
fs.mkdirSync('shots', { recursive: true });
const url = 'http://127.0.0.1:8090/?v=fps' + Date.now();
const target = await (await fetch('http://127.0.0.1:9222/json/new?' + encodeURIComponent(url), { method: 'PUT' })).json();
const ws = new WebSocket(target.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
let id = 0;
const pending = new Map();
ws.onmessage = (m) => {
  const msg = JSON.parse(m.data);
  if (msg.id && pending.has(msg.id)) {
    const { res, rej } = pending.get(msg.id);
    pending.delete(msg.id);
    msg.error ? rej(new Error(JSON.stringify(msg.error))) : res(msg.result);
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
  if (r.exceptionDetails) throw new Error(r.exceptionDetails.text);
  return r.result.value;
};

await send('Page.enable');
await send('Runtime.enable');
await send('Page.bringToFront');
await send('Emulation.setDeviceMetricsOverride', { width: Number(process.env.RESW || 1280), height: Number(process.env.RESH || 720), deviceScaleFactor: 1, mobile: false });
const t0 = Date.now();
while (Date.now() - t0 < 40000) {
  try { if (await evalJs('!!window.__mwReady')) break; } catch {}
  await sleep(250);
}
await sleep(1500);
await evalJs(`window.__mwModule._mw_debug_place(${x},${z},${yaw})`);
console.log('assets', JSON.stringify(await evalJs('window.__mwAssets')));
for (let i = 0; i < 4; ++i) {
  await sleep(3000);
  console.log('t+' + ((i + 1) * 3) + 's fps', await evalJs('window.__mwFps'));
}
ws.close();
await fetch('http://127.0.0.1:9222/json/close/' + target.id);
process.exit(0);
