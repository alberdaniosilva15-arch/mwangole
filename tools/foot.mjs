// Dev-only pedestrian close-up over CDP. Usage: node tools/foot.mjs
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
const shot = async (name, clip) => {
  const r = await send('Page.captureScreenshot', clip ? { format: 'png', clip } : { format: 'png' });
  fs.writeFileSync(`shots/${name}.png`, Buffer.from(r.data, 'base64'));
};
const key = (code, down) => ev(`window.dispatchEvent(new KeyboardEvent('${down ? 'keydown' : 'keyup'}',{code:'${code}'}))`);

await send('Page.enable'); await send('Runtime.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false });
await send('Page.navigate', { url: 'http://127.0.0.1:8090/?v=foot3' });
for (let i = 0; i < 100; ++i) { if (await ev('!!window.__mwReady')) break; await sleep(200); }
await sleep(900);

// exit the car
await key('KeyE', true); await sleep(60); await key('KeyE', false);
await sleep(800);
await shot('foot_back');

// walking away
await key('KeyW', true); await sleep(500); await shot('foot_walk'); await key('KeyW', false);
await sleep(500);

// orbit to the front, keep the hold alive and shoot
for (let i = 0; i < 18; ++i) await ev('__mwModule._mw_look(30, 0)');
await sleep(60);
await shot('foot_front');
await shot('foot_front_zoom', { x: 460, y: 120, width: 360, height: 480, scale: 2 });

// walking toward camera (S) while orbit holds
await key('KeyS', true);
for (let i = 0; i < 6; ++i) { await ev('__mwModule._mw_look(20, 0)'); await sleep(70); }
await shot('foot_front_walk');
await key('KeyS', false);

console.log(JSON.stringify(await ev('({inCar: window.__mwInCar, dbg: window.__dbg})')));
ws.close(); process.exit(0);
