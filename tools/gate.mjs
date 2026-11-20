// Veredicto de fase: compara shots/look/<tag>/metrics.json contra bench/baseline.json.
// Uso: node tools/gate.mjs F1 [tag]     (tag por omissão: after)
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { ROOT } from './lib/harness.mjs';

const phase = (process.argv[2] || 'F0').toUpperCase();
const tag = process.argv[3] || 'after';

// Cada fase declara os seus limites. Um limite ausente = não é verificado nessa fase.
const PHASES = {
  F0: { minFps: 0, sdGain: -1, meanTol: 1, edges: null, trisMax: null, gzMax: null },
  F1: { minFps: 25, sdGain: 0.25, meanTol: 0.08, edges: { camera: 'baixa', factor: 2 }, trisMax: null, gzMax: 3 },
  F2: { minFps: 30, sdGain: 0.20, meanTol: 0.10, edges: null, trisMax: 250000, gzMax: 3 },
  F3: { minFps: 30, sdGain: 0.25, meanTol: 0.12, edges: { camera: 'baixa', factor: 3 }, trisMax: 250000, gzMax: 24 },
  F4: { minFps: 30, sdGain: 0.20, meanTol: 0.12, edges: null, trisMax: 250000, gzMax: 24 },
  F5: { minFps: 30, sdGain: 0.25, meanTol: 0.12, edges: null, trisMax: 250000, gzMax: 26 },
  F6: { minFps: 30, sdGain: 0.20, meanTol: 0.12, edges: null, trisMax: 250000, gzMax: 26 },
  F7: { minFps: 30, sdGain: 0.10, meanTol: 0.12, edges: null, trisMax: 250000, gzMax: 26 },
  F8: { minFps: 30, sdGain: 0.10, meanTol: 0.12, edges: null, trisMax: 260000, gzMax: 28 },
  F9: { minFps: 30, sdGain: 0.10, meanTol: 0.15, edges: { camera: 'spawn', factor: 1.5 }, trisMax: 260000, gzMax: 30 },
};

const rules = PHASES[phase];
if (!rules) {
  console.error(`fase desconhecida: ${phase}. Opções: ${Object.keys(PHASES).join(', ')}`);
  process.exit(2);
}

const baselineFile = path.join(ROOT, 'bench', 'baseline.json');
const currentFile = path.join(ROOT, 'shots', 'look', tag, 'metrics.json');
if (!fs.existsSync(baselineFile)) { console.error('falta bench/baseline.json (corre node tools/baseline.mjs)'); process.exit(2); }
if (!fs.existsSync(currentFile)) { console.error(`falta ${currentFile} (corre node tools/look.mjs ${tag})`); process.exit(2); }

const base = JSON.parse(fs.readFileSync(baselineFile, 'utf8'));
const curRaw = JSON.parse(fs.readFileSync(currentFile, 'utf8'));
// baseline.json guarda { look: { rows } }; metrics.json do look guarda { rows }.
const cur = curRaw.rows ? { look: curRaw } : curRaw;

const avg = (rows, key) => rows.reduce((a, r) => a + r[key], 0) / Math.max(1, rows.length);
const byCamera = (rows, cam, key) => avg(rows.filter((r) => r.camera === cam), key);

const checks = [];
const add = (name, ok, detail) => checks.push({ name, ok, detail });

// fps mínimo em qualquer leitura
const minFps = Math.min(...cur.look.rows.map((r) => r.fps));
const worst = cur.look.rows.find((r) => r.fps === minFps);
add('fps minimo', minFps >= rules.minFps, `${minFps} fps (${worst.camera} tod=${worst.tod}) >= ${rules.minFps}`);

// contraste e luminância
const sdBase = avg(base.look.rows, 'sd');
const sdCur = avg(cur.look.rows, 'sd');
const sdGoal = sdBase * (1 + Math.max(0, rules.sdGain));
add('contraste (sd)', sdCur >= sdGoal, `${sdCur.toFixed(1)} >= ${sdGoal.toFixed(1)} (baseline ${sdBase.toFixed(1)})`);

const lumBase = avg(base.look.rows, 'mean');
const lumCur = avg(cur.look.rows, 'mean');
const lumTol = Math.abs(lumCur - lumBase) / lumBase;
add('luminancia media', lumTol <= rules.meanTol, `variacao ${(lumTol * 100).toFixed(1)}% <= ${(rules.meanTol * 100).toFixed(0)}%`);

// densidade de detalhe numa câmara específica
if (rules.edges) {
  const cam = rules.edges.camera;
  const eBase = byCamera(base.look.rows, cam, 'edges');
  const eCur = byCamera(cur.look.rows, cam, 'edges');
  const goal = eBase * rules.edges.factor;
  add(`detalhe (${cam})`, eCur >= goal, `${eCur.toFixed(2)}% >= ${goal.toFixed(2)}% (${rules.edges.factor}x baseline ${eBase.toFixed(2)}%)`);
}

// carga de desenho
if (rules.trisMax != null) {
  const perfFile = path.join(ROOT, 'bench', `perf-${tag}.json`);
  if (fs.existsSync(perfFile)) {
    const perf = JSON.parse(fs.readFileSync(perfFile, 'utf8'));
    add('triangulos/frame', perf.tris <= rules.trisMax, `${(perf.tris / 1000).toFixed(0)}k <= ${(rules.trisMax / 1000).toFixed(0)}k`);
    add('draw calls', perf.calls <= 900, `${perf.calls} <= 900`);
  } else {
    add('triangulos/frame', false, `falta bench/perf-${tag}.json (corre node tools/perf.mjs ${tag})`);
  }
}

// payload
if (rules.gzMax != null) {
  let gz = 0;
  const buildDir = path.join(ROOT, 'web', 'build');
  if (fs.existsSync(buildDir)) {
    for (const f of fs.readdirSync(buildDir)) gz += zlib.gzipSync(fs.readFileSync(path.join(buildDir, f)), { level: 9 }).length;
  }
  const mb = gz / 1048576;
  add('payload gz', mb <= rules.gzMax, `${mb.toFixed(2)} MB <= ${rules.gzMax} MB`);
}

console.log(`\ngate ${phase} (tag=${tag})\n`);
let failed = 0;
for (const c of checks) {
  console.log(`  ${c.ok ? 'PASSA' : 'FALHA'}  ${c.name.padEnd(20)} ${c.detail}`);
  if (!c.ok) failed++;
}
console.log('');
if (failed) {
  console.log(`${failed} verificação(ões) falharam — a fase ${phase} não fecha.`);
  process.exit(1);
}
console.log(`fase ${phase} cumpre todos os gates.`);
