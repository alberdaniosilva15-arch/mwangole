// Dev-only: screenshot with a given time of day and optional mission test.
// Usage: node tools/timeview.mjs x z yaw name tod [job]
import fs from 'node:fs';

const [x, z, yaw, name = 'timeview', tod = '0.36', job] = process.argv.slice(2);
const out = 'shots';
fs.mkdirSync(out, { recursive: true });
const url = 'http://127.0.0.1:8090/?v=timeview' + Date.now();
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
  if (r.exceptionDetails) throw new Error(r.exceptionDetails.exception?.description || r.exceptionDetails.text);
  return r.result.value;
};
const shot = async (n) => {
  const r = await send('Page.captureScreenshot', { format: 'png' });
  fs.writeFileSync(`${out}/${n}.png`, Buffer.from(r.data, 'base64'));
};
const jobText = () => evalJs("document.querySelector('#job .job-t').textContent + ' | ' + document.querySelector('#job .job-s').textContent");

await send('Page.enable');
await send('Runtime.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false });
const t0 = Date.now();
while (Date.now() - t0 < 40000) {
  try { if (await evalJs('!!window.__mwReady')) break; } catch {}
  await sleep(250);
}
await sleep(1200);
await evalJs(`window.__mwModule._mw_debug_place(${x},${z},${yaw})`);
await evalJs(`window.__mwModule._mw_debug_time(${tod})`);
await sleep(1200);

if (job === 'shot') {
  await evalJs('window.__mwModule._mw_action(15,1); window.__mwModule._mw_action(15,0);');
  await sleep(700);
  const j = await evalJs('window.__mwJob');
  if (j) {
    await evalJs(`window.__mwModule._mw_debug_place(${j.x},${j.z + 48},3.14159)`);
    await sleep(1500);
    await shot(name);
  }
  console.log('beacon', JSON.stringify(j));
  ws.close();
  await fetch('http://127.0.0.1:9222/json/close/' + target.id);
  process.exit(0);
}

if (job) {
  await evalJs('window.__mwModule._mw_action(15,1); window.__mwModule._mw_action(15,0);');
  await sleep(700);
  console.log('job start:', await jobText());
  let j = await evalJs('window.__mwJob');
  console.log('target1:', JSON.stringify(j));
  if (j) {
    await evalJs(`window.__mwModule._mw_debug_place(${j.x},${j.z},${yaw})`);
    await sleep(900);
    console.log('job 2:', await jobText());
    j = await evalJs('window.__mwJob');
    console.log('target2:', JSON.stringify(j));
    await shot(name + '-pick');
    if (j) {
      await evalJs(`window.__mwModule._mw_debug_place(${j.x},${j.z},${yaw})`);
      await sleep(900);
      console.log('job 3:', await jobText());
      console.log('cash:', await evalJs("document.getElementById('cash').textContent"));
    }
  }
}

await sleep(1400);
await shot(name);
console.log(name, await evalJs('window.__mwFps'));
ws.close();
await fetch('http://127.0.0.1:9222/json/close/' + target.id);
process.exit(0);
