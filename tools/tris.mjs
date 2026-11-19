// Dev-only: dump scene triangle counts.
const url = 'http://127.0.0.1:8090/?v=tris' + Date.now();
const target = await (await fetch('http://127.0.0.1:9222/json/new?' + encodeURIComponent(url), { method: 'PUT' })).json();
const ws = new WebSocket(target.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
let id = 0; const pending = new Map();
ws.onmessage = (m) => { const msg = JSON.parse(m.data); if (msg.id && pending.has(msg.id)) { const p = pending.get(msg.id); pending.delete(msg.id); msg.error ? p.rej(new Error(JSON.stringify(msg.error))) : p.res(msg.result); } };
const send = (method, params = {}) => new Promise((res, rej) => { const n = ++id; pending.set(n, { res, rej }); ws.send(JSON.stringify({ id: n, method, params })); });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const evalJs = async (expr) => (await send('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true })).result.value;
await send('Page.enable'); await send('Runtime.enable');
const t0 = Date.now();
while (Date.now() - t0 < 40000) { try { if (await evalJs('!!window.__mwReady')) break; } catch {} await sleep(250); }
console.log('tris', await evalJs('window.__mwModule._mw_debug_tris()'));
ws.close(); await fetch('http://127.0.0.1:9222/json/close/' + target.id); process.exit(0);
