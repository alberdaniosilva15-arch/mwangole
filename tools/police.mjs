// Dev-only: cause a pile-up, verify heat/wanted and police pursuit. Usage: node tools/police.mjs
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
const key = (code, down) => ev(`window.dispatchEvent(new KeyboardEvent('${down ? 'keydown' : 'keyup'}',{code:'${code}'}))`);

await send('Page.enable'); await send('Runtime.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false });
await send('Page.navigate', { url: 'http://127.0.0.1:8090/?v=police' });
for (let i = 0; i < 120; ++i) { if (await ev('!!window.__mwReady')) break; await sleep(200); }
await sleep(900);

const log = [];
await key('KeyW', true);
for (let i = 0; i < 14; ++i) {
  await sleep(700);
  log.push(await ev(`({s: window.__mwStreet, dbg: window.__dbg})`));
  if (log[log.length - 1].s.heat > 0 && log[log.length - 1].s.cops > 0) break;
}
await key('KeyW', false);
await shot('police_chase');
let arrived = false;
for (let i = 0; i < 16; ++i) {
  await sleep(800);
  const s = await ev('window.__mwStreet');
  if (s.nearCop < 26) { arrived = true; break; }
}
const after = await ev(`({s: window.__mwStreet, dbg: window.__dbg})`);
await shot('police_chase2');
await ev(`__mwModule._mw_look(900, 110)`); await sleep(400);
await shot('police_side');
console.log(JSON.stringify({
  heat: log.map((l) => l.s.heat.toFixed(2)),
  cops: log.map((l) => l.s.cops),
  speeds: log.map((l) => +l.dbg.speed.toFixed(1)),
  arrived,
  after: { heat: after.s.heat.toFixed(2), cops: after.s.cops, nearCop: +after.s.nearCop.toFixed(1),
           health: +after.s.health.toFixed(1) },
}, null, 1));
ws.close(); process.exit(0);
