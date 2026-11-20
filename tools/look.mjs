// Mede aparência e fps nas 6 câmaras fixas x 3 horas do dia.
// Uso: node tools/look.mjs <tag>      (ex.: before, after, f1)
import { measureAll, ROOT } from './lib/harness.mjs';
import path from 'node:path';

const tag = process.argv[2] || 'run';
const MIN_FPS = Number(process.env.MIN_FPS || 25);

console.log(`look: tag=${tag} (6 cameras x 3 horas)`);
const summary = await measureAll({ tag });

const slow = summary.rows.filter((r) => r.fps < MIN_FPS);
const meanSd = summary.rows.reduce((a, r) => a + r.sd, 0) / summary.rows.length;
const meanEdges = summary.rows.reduce((a, r) => a + r.edges, 0) / summary.rows.length;
const meanLum = summary.rows.reduce((a, r) => a + r.mean, 0) / summary.rows.length;

console.log('');
console.log(`resumo: sd medio ${meanSd.toFixed(1)} | edges medio ${meanEdges.toFixed(2)}% | luminancia media ${meanLum.toFixed(1)}`);
console.log(`imagens: ${path.join(ROOT, 'shots', 'look', tag)}`);

if (slow.length) {
  console.error(`FALHA: ${slow.length} leitura(s) abaixo de ${MIN_FPS} fps:`);
  for (const r of slow) console.error(`  ${r.camera} tod=${r.tod} fps=${r.fps}`);
  process.exit(1);
}
console.log(`ok: todas as leituras >= ${MIN_FPS} fps`);
