// Mede frame time a conduzir e a carga de desenho (draw calls, triângulos visíveis).
// Uso: node tools/perf.mjs [tag]
import fs from 'node:fs';
import path from 'node:path';
import { openPage, waitReady, sleep, ROOT, ensureDir, GAME_URL } from './lib/harness.mjs';

const tag = process.argv[2] || 'run';
const DRIVE_MS = Number(process.env.DRIVE_MS || 12000);

const sampler = (ms) => `new Promise((res)=>{const t=[];let last=performance.now();const t0=last;
function tick(){const now=performance.now();t.push(now-last);last=now;
if(now-t0<${ms})requestAnimationFrame(tick);
else{const s=t.slice(8).sort((a,b)=>a-b);const q=(p)=>s[Math.min(s.length-1,Math.floor(s.length*p))];
const sum=s.reduce((a,b)=>a+b,0);
res({frames:s.length,p50:+q(.5).toFixed(2),p95:+q(.95).toFixed(2),max:+s[s.length-1].toFixed(2),
fps:+(1000/(sum/s.length)).toFixed(1)});}}
requestAnimationFrame(tick);})`;

const page = await openPage(`${GAME_URL}${Date.now()}`);
try {
  if (!(await waitReady(page.ev))) throw new Error('o jogo nao ficou pronto');
  await page.send('Emulation.setDeviceMetricsOverride', { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false });
  await sleep(1200);

  await page.ev("window.__mwModule._mw_debug_place(97,248,0)");
  await page.ev("window.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyW'}))");
  const drive = await page.ev(sampler(DRIVE_MS));
  const stats = await page.ev("window.__mwModule.UTF8ToString(window.__mwModule._mw_debug_stats())");
  await page.ev("window.dispatchEvent(new KeyboardEvent('keyup',{code:'KeyW'}))");

  const [calls, tris] = String(stats).split('|').map(Number);
  const result = {
    tag,
    created: new Date().toISOString(),
    driveMs: DRIVE_MS,
    ...drive,
    calls,
    tris,
    trisCity: await page.ev('window.__mwModule._mw_debug_tris()'),
  };

  const dir = ensureDir(path.join(ROOT, 'bench'));
  fs.writeFileSync(path.join(dir, `perf-${tag}.json`), JSON.stringify(result, null, 1));
  console.log(`perf: p50=${result.p50}ms p95=${result.p95}ms max=${result.max}ms fps=${result.fps}`);
  console.log(`      draw calls=${calls} tris/frame=${(tris / 1000).toFixed(0)}k tris/cidade=${(result.trisCity / 1e6).toFixed(2)}M`);
  if (process.env.GATE === '1' && result.p95 > 40) {
    console.error('FALHA: p95 acima de 40 ms (menos de 25 fps sustentados)');
    process.exitCode = 1;
  }
} finally {
  await page.close();
}
