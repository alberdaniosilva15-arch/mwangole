// Dev-only: verify the WebAudio chain starts after a real gesture. Usage: node tools/audio.mjs
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
const click = async (x, y) => {
  await send('Input.dispatchMouseEvent', { type: 'mousePressed', x, y, button: 'left', clickCount: 1 });
  await sleep(60);
  await send('Input.dispatchMouseEvent', { type: 'mouseReleased', x, y, button: 'left', clickCount: 1 });
};
const key = (code, down) => ev(`window.dispatchEvent(new KeyboardEvent('${down ? 'keydown' : 'keyup'}',{code:'${code}'}))`);

await send('Page.enable'); await send('Runtime.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false });
await send('Page.navigate', { url: 'http://127.0.0.1:8090/?v=audio' });
for (let i = 0; i < 120; ++i) { if (await ev('!!window.__mwReady')) break; await sleep(200); }
await sleep(600);
const before = await ev('window.__mwAudioDebug()');
await click(640, 300);
await sleep(400);
const afterClick = await ev('window.__mwAudioDebug()');
await key('KeyW', true);
await sleep(1600);
const driving = await ev('window.__mwAudioDebug()');
await key('KeyH', true); await sleep(60); await key('KeyH', false);
await sleep(250);
const horn = await ev('window.__mwAudioDebug()');
let hot = null;
for (let i = 0; i < 14; ++i) {
  await sleep(700);
  const s = await ev('window.__mwStreet');
  if (s && s.heat > 0) { hot = s; break; }
}
await key('KeyW', false);
await sleep(900);
const chase = await ev('window.__mwAudioDebug()');
await key('KeyM', true); await sleep(80); await key('KeyM', false);
await sleep(500);
const mutedState = await ev('window.__mwAudioDebug()');
console.log(JSON.stringify({
  before, afterClick, driving,
  horn: { hornCount: horn.hornCount, muted: horn.muted },
  hot: hot && hot.heat,
  impact: { count: chase.impactCount, last: chase.lastImpact },
  chase, mutedState,
}, null, 1));
ws.close(); process.exit(0);
