// Dev-only: watch the street life around the spawn. Usage: node tools/street.mjs
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
const look = (dx, dy) => ev(`__mwModule._mw_look(${dx}, ${dy})`);
const key = (code, down) => ev(`window.dispatchEvent(new KeyboardEvent('${down ? 'keydown' : 'keyup'}',{code:'${code}'}))`);

await send('Page.enable'); await send('Runtime.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false });
await send('Page.navigate', { url: 'http://127.0.0.1:8090/?v=street' });
for (let i = 0; i < 120; ++i) { if (await ev('!!window.__mwReady')) break; await sleep(200); }
await sleep(1200);

const log = [];
for (let i = 0; i < 6; ++i) {
  log.push(await ev(`({s: window.__mwStreet, t: window.__mwTraffic})`));
  await sleep(1000);
}
await shot('street_a');

await look(700, 120); await sleep(400); await shot('street_left');
await look(1400, 0); await sleep(400); await shot('street_back');
await look(1400, -140); await sleep(400); await shot('street_top');
await look(-2100, 0); await sleep(300);

await key('KeyW', true); await sleep(2600); await key('KeyW', false);
await sleep(900);
await shot('street_drive');
const moving = log.map((l) => (l.t || []).filter((c) => c && c.speed > 1).length);
console.log(JSON.stringify({ moving, dens: log.map((l) => l.s), cars0: log.map((l) => l.t?.[0]) }));
ws.close(); process.exit(0);
