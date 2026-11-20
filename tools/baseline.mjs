// Congela o estado actual em bench/baseline.json. É a referência de todas as regressões.
// Uso: node tools/baseline.mjs
import fs from 'node:fs';
import path from 'node:path';
import zlib from 'node:zlib';
import { execFileSync } from 'node:child_process';
import { measureAll, ROOT, ensureDir } from './lib/harness.mjs';

function gz(file) {
  return zlib.gzipSync(fs.readFileSync(file), { level: 9 }).length;
}

function payload() {
  // Só o que o browser descarrega: web/build/*. Os assets já vêm preloadados
  // dentro de mw.data, por isso contar web/assets/* seria duplicar.
  const files = [];
  const p = path.join(ROOT, 'web', 'build');
  if (fs.existsSync(p)) for (const f of fs.readdirSync(p)) files.push(path.join(p, f));
  let raw = 0, gzipped = 0;
  for (const f of files) { raw += fs.statSync(f).size; gzipped += gz(f); }
  return { files: files.length, rawMB: +(raw / 1048576).toFixed(2), gzMB: +(gzipped / 1048576).toFixed(2) };
}

console.log('baseline: a medir aparência (6 câmaras x 3 horas)...');
const look = await measureAll({ tag: 'baseline' });

console.log('baseline: a medir condução...');
execFileSync(process.execPath, [path.join(ROOT, 'tools', 'perf.mjs'), 'baseline'], { stdio: 'inherit', cwd: ROOT });
const perf = JSON.parse(fs.readFileSync(path.join(ROOT, 'bench', 'perf-baseline.json'), 'utf8'));

const data = {
  created: new Date().toISOString(),
  payload: payload(),
  look,
  perf,
};

const dir = ensureDir(path.join(ROOT, 'bench'));
fs.writeFileSync(path.join(dir, 'baseline.json'), JSON.stringify(data, null, 1));
console.log('');
console.log(`baseline congelado: bench/baseline.json`);
console.log(`  payload gz ${data.payload.gzMB} MB (${data.payload.files} ficheiros)`);
console.log(`  p95 ${perf.p95} ms · tris/frame ${(perf.tris / 1000).toFixed(0)}k · tris/cidade ${(perf.trisCity / 1e6).toFixed(2)}M`);
