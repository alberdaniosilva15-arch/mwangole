// Dev-only zoomed clip over CDP. Usage: node tools/zoom.mjs
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
  const r = await send('Page.captureScreenshot', { format: 'png', clip });
  fs.writeFileSync(`shots/${name}.png`, Buffer.from(r.data, 'base64'));
};

await send('Page.enable'); await send('Runtime.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false });
await send('Page.navigate', { url: 'http://127.0.0.1:8090/?v=zoom1' });
for (let i = 0; i < 100; ++i) { if (await ev('!!window.__mwReady')) break; await sleep(200); }
await sleep(900);
await ev('__mwModule._mw_look(520, 60)'); await sleep(350);
await shot('zoom_rear', { x: 430, y: 260, width: 430, height: 300, scale: 3 });
console.log(JSON.stringify(await ev('({fps: window.__mwFps, size: [document.getElementById("game").width, document.getElementById("game").height]})')));
ws.close(); process.exit(0);
