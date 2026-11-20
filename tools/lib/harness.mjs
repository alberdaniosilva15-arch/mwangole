// Base partilhada dos harnesses de aparência e performance (CDP + métricas).
// Sem dependências novas: usa o WebSocket global do Node 24 e o pngjs já instalado.
import fs from 'node:fs';
import path from 'node:path';
import { PNG } from 'pngjs';

export const CDP_PORT = process.env.CDP_PORT || '9222';
export const GAME_URL = process.env.GAME_URL || 'http://127.0.0.1:8090/?v=look';
export const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, '$1')), '..', '..');

export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// Câmaras fixas do plano. Nao mudar sem invalidar o historico.
export const CAMERAS = [
  { name: 'spawn', x: 97, z: 248, yaw: 0 },
  { name: 'muro', x: 250, z: 210, yaw: 0 },
  { name: 'baixa', x: 104, z: 250, yaw: 0 },
  { name: 'condo', x: 104, z: 150, yaw: 3.14159 },
  { name: 'avenida', x: 0, z: 104, yaw: -1.5708 },
  { name: 'noite', x: 104, z: 260, yaw: 0 },
];

export const HOURS = [0.30, 0.50, 0.79];

export async function openPage(url = GAME_URL) {
  const res = await fetch(`http://127.0.0.1:${CDP_PORT}/json/new?${encodeURIComponent(url)}`, { method: 'PUT' });
  if (!res.ok) throw new Error(`CDP: nao consegui abrir pagina (${res.status}). O Chrome esta em --remote-debugging-port ${CDP_PORT}?`);
  const target = await res.json();
  const ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((ok, bad) => { ws.onopen = ok; ws.onerror = bad; });

  let id = 0;
  const pending = new Map();
  ws.onmessage = (m) => {
    const msg = JSON.parse(m.data);
    if (msg.id && pending.has(msg.id)) {
      const cb = pending.get(msg.id);
      pending.delete(msg.id);
      cb(msg);
    }
  };
  const send = (method, params = {}) => new Promise((ok, bad) => {
    const n = ++id;
    pending.set(n, (msg) => (msg.error ? bad(new Error(JSON.stringify(msg.error))) : ok(msg.result)));
    ws.send(JSON.stringify({ id: n, method, params }));
  });
  const ev = async (expr) => {
    const r = await send('Runtime.evaluate', { expression: expr, returnByValue: true, awaitPromise: true });
    if (r.exceptionDetails) throw new Error(r.exceptionDetails.exception?.description || r.exceptionDetails.text);
    return r.result.value;
  };
  await send('Page.enable');
  await send('Runtime.enable');
  const close = async () => {
    try { ws.close(); } catch {}
    try { await fetch(`http://127.0.0.1:${CDP_PORT}/json/close/${target.id}`); } catch {}
  };
  return { id: target.id, send, ev, close };
}

export async function waitReady(ev, ms = 60000) {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) {
    try { if (await ev('!!window.__mwReady')) return true; } catch {}
    await sleep(200);
  }
  return false;
}

export async function sampleFps(ev, ms = 2200) {
  // Mediana dos intervalos: uma travada isolada (GC, decode) nao pode decidir o
  // veredicto de uma camara inteira, como acontecia com a media.
  const expr = `new Promise((res)=>{const t=[];let last=performance.now();const t0=last;
const tick=()=>{const now=performance.now();t.push(now-last);last=now;
if(now-t0<${ms})requestAnimationFrame(tick);
else{const s=t.slice(6).sort((a,b)=>a-b);const med=s[Math.floor(s.length/2)]||16.7;
res(+(1000/med).toFixed(1));}};
requestAnimationFrame(tick);})`;
  return ev(expr);
}

export async function screenshot(send) {
  const r = await send('Page.captureScreenshot', { format: 'png' });
  return Buffer.from(r.data, 'base64');
}

// Métricas objectivas de uma imagem: luminância média, contraste (sd) e densidade de arestas.
export function imageMetrics(buf) {
  const png = PNG.sync.read(buf);
  const { width: w, height: h, data } = png;
  const lum = new Float32Array(w * h);
  let sum = 0;
  for (let i = 0; i < w * h; i++) {
    const l = 0.299 * data[i * 4] + 0.587 * data[i * 4 + 1] + 0.114 * data[i * 4 + 2];
    lum[i] = l;
    sum += l;
  }
  const n = w * h;
  const mean = sum / n;
  let varSum = 0;
  for (let i = 0; i < n; i++) { const d = lum[i] - mean; varSum += d * d; }
  const sd = Math.sqrt(varSum / n);
  let edges = 0, tot = 0;
  for (let y = 1; y < h - 1; y++) {
    for (let x = 1; x < w - 1; x++) {
      const i = y * w + x;
      const gx = -lum[i - w - 1] - 2 * lum[i - 1] - lum[i + w - 1] + lum[i - w + 1] + 2 * lum[i + 1] + lum[i + w + 1];
      const gy = -lum[i - w - 1] - 2 * lum[i - w] - lum[i - w + 1] + lum[i + w - 1] + 2 * lum[i + w] + lum[i + w + 1];
      if (Math.abs(gx) + Math.abs(gy) > 60) edges++;
      tot++;
    }
  }
  return {
    w, h,
    mean: +mean.toFixed(1),
    sd: +sd.toFixed(1),
    edges: +((100 * edges) / Math.max(1, tot)).toFixed(2),
  };
}

export function ensureDir(dir) {
  fs.mkdirSync(dir, { recursive: true });
  return dir;
}

// Percorre cameras x horas, mede fps, guarda PNG e devolve as linhas.
export async function measureAll({ tag = 'run', cameras = CAMERAS, hours = HOURS, outDir } = {}) {
  const dir = ensureDir(outDir || path.join(ROOT, 'shots', 'look', tag));
  const page = await openPage(`${GAME_URL}${Date.now()}`);
  const rows = [];
  try {
    const ready = await waitReady(page.ev);
    if (!ready) throw new Error('o jogo nao ficou pronto (window.__mwReady)');
    await page.send('Emulation.setDeviceMetricsOverride', { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false });
    await sleep(1200);
    for (const cam of cameras) {
      for (const tod of hours) {
        await page.ev(`window.__mwModule._mw_debug_place(${cam.x},${cam.z},${cam.yaw})`);
        await page.ev(`window.__mwModule._mw_debug_time(${tod})`);
        await sleep(700);
        const fps = await sampleFps(page.ev, 1400);
        const buf = await screenshot(page.send);
        const file = path.join(dir, `${cam.name}_${String(tod).replace('.', '')}.png`);
        fs.writeFileSync(file, buf);
        const m = imageMetrics(buf);
        const stats = await page.ev("window.__mwModule.UTF8ToString(window.__mwModule._mw_debug_stats())");
        const [calls, tris] = String(stats).split('|');
        const lot = await page.ev(`window.__mwModule.UTF8ToString(window.__mwModule._mw_debug_lot_nearest(${cam.x},${cam.z}))`);
        rows.push({ camera: cam.name, tod, fps, ...m, calls: +calls, tris: +tris, nearest: lot.split('|')[0], file });
        process.stdout.write(`  ${cam.name.padEnd(8)} tod=${tod} fps=${String(fps).padStart(5)} sd=${String(m.sd).padStart(5)} edges=${String(m.edges).padStart(5)} tris=${(tris / 1000).toFixed(0)}k calls=${calls}\n`);
      }
    }
  } finally {
    await page.close();
  }
  const summary = {
    tag,
    created: new Date().toISOString(),
    resolution: '1280x720',
    cameras: cameras.map((c) => c.name),
    hours,
    rows,
  };
  fs.writeFileSync(path.join(dir, 'metrics.json'), JSON.stringify(summary, null, 1));
  return summary;
}

export function readJson(file) {
  return JSON.parse(fs.readFileSync(file, 'utf8'));
}
