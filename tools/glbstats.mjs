// Dev-only: raw tri/texture stats of user GLBs (read-only, no output files).
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const dir = path.join(ROOT, 'assets-src/meus-modelos');
for (const f of fs.readdirSync(dir).filter((x) => x.endsWith('.glb'))) {
  const b = fs.readFileSync(path.join(dir, f));
  const jl = b.readUInt32LE(12);
  const g = JSON.parse(b.slice(20, 20 + jl).toString('utf8'));
  let tris = 0, verts = 0;
  for (const m of g.meshes || []) for (const p of m.primitives) {
    if ((p.mode ?? 4) !== 4) continue;
    const n = p.indices !== undefined ? g.accessors[p.indices].count : g.accessors[p.attributes.POSITION].count;
    tris += n / 3; verts += g.accessors[p.attributes.POSITION].count;
  }
  const texs = (g.images || []).map((im) => {
    const bv = g.bufferViews[im.bufferView];
    return `${im.mimeType || '?'}:${((bv.byteLength || 0) / 1024).toFixed(0)}KB`;
  });
  const mats = (g.materials || []).length;
  const withTex = (g.materials || []).filter((m) => m.pbrMetallicRoughness?.baseColorTexture).length;
  console.log(f.padEnd(30), String(Math.round(tris)).padStart(7), 'tris', String(verts).padStart(7), 'v',
    mats + ' mats (' + withTex + ' c/ tex)', texs.join(' ') || 'sem texturas');
}