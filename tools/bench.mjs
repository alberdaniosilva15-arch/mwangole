// Dev-only benchmark run over CDP. Usage: node tools/bench.mjs
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
  return r.result.value;
};
await send('Page.enable'); await send('Runtime.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false });
await send('Page.navigate', { url: 'http://127.0.0.1:8090/?bench=1&v=' + Date.now() });
for (let i = 0; i < 80; ++i) {
  const t = await ev('window.__mwFps');
  await sleep(500);
  if (i > 20 && t) break;
}
await sleep(6000);
console.log(await ev('JSON.stringify({fps: window.__mwFps, size: [document.getElementById("game").width, document.getElementById("game").height]})'));
ws.close(); process.exit(0);
