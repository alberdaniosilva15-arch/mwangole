import fs from 'node:fs';

const targets = await (await fetch('http://127.0.0.1:9222/json')).json();
const page = targets.find((t) => t.type === 'page');
const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
let id = 0;
const pend = new Map();
ws.onmessage = (m) => {
  const x = JSON.parse(m.data);
  if (x.id && pend.has(x.id)) {
    const p = pend.get(x.id); pend.delete(x.id);
    x.error ? p.rej(new Error(JSON.stringify(x.error))) : p.res(x.result);
  }
};
const send = (method, params = {}) => new Promise((res, rej) => {
  const n = ++id; pend.set(n, { res, rej }); ws.send(JSON.stringify({ id: n, method, params }));
});
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const ev = async (e) => {
  const r = await send('Runtime.evaluate', { expression: e, awaitPromise: true, returnByValue: true });
  if (r.exceptionDetails) throw new Error(r.exceptionDetails.text);
  return r.result.value;
};
const shot = async (name) => {
  const r = await send('Page.captureScreenshot', { format: 'png' });
  fs.writeFileSync(`shots/${name}.png`, Buffer.from(r.data, 'base64'));
};

await send('Page.enable'); await send('Runtime.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false });
await send('Page.navigate', { url: 'http://127.0.0.1:8090/?v=car2' });
for (let i = 0; i < 100; ++i) { if (await ev('!!window.__mwReady')) break; await sleep(200); }
await sleep(800);

const look = async (dx, dy) => ev(`__mwModule._mw_look(${dx}, ${dy})`);
await look(520, 60); await sleep(350); await shot('photo_rear');
await look(650, 0); await sleep(300); await shot('photo_side');
await look(650, 0); await sleep(300); await shot('photo_front');
await look(650, -80); await sleep(300); await shot('photo_front2');
await look(-1400, 150); await sleep(300); await shot('photo_top');
console.log('ok');
ws.close(); process.exit(0);
