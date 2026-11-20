#!/usr/bin/env node
// Converte os GLB do kit CC0 para o formato binario do motor (MWB1).
// Uso: node tools/glb2mw.mjs [--only nome1,nome2]
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import jpeg from 'jpeg-js';
import { PNG } from 'pngjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const cfg = JSON.parse(fs.readFileSync(path.join(ROOT, 'tools', 'models.json'), 'utf8'));
const onlyArg = process.argv.find((a) => a.startsWith('--only='));
const only = onlyArg ? new Set(onlyArg.slice(7).split(',')) : null;

const CT = { 5120: 1, 5121: 1, 5122: 2, 5123: 2, 5125: 4, 5126: 4 };
const TS = { SCALAR: 1, VEC2: 2, VEC3: 3, VEC4: 4, MAT4: 16 };

function parseGlb(file) {
  const b = fs.readFileSync(file);
  if (b.readUInt32LE(0) !== 0x46546c67) throw new Error('nao e GLB: ' + file);
  const jsonLen = b.readUInt32LE(12);
  const jsonType = b.readUInt32LE(16);
  if (jsonType !== 0x4e4f534a) throw new Error('chunk JSON invalido: ' + file);
  const gltf = JSON.parse(b.slice(20, 20 + jsonLen).toString('utf8'));
  const binOff = 20 + jsonLen;
  if (binOff + 8 > b.length) throw new Error('sem chunk BIN: ' + file);
  const binLen = b.readUInt32LE(binOff);
  const binType = b.readUInt32LE(binOff + 4);
  if (binType !== 0x004e4942) throw new Error('chunk BIN invalido: ' + file);
  const bin = b.slice(binOff + 8, binOff + 8 + binLen);
  return { gltf, bin };
}

function readAccessor(gltf, bin, idx) {
  const a = gltf.accessors[idx];
  const bv = gltf.bufferViews[a.bufferView];
  const comp = CT[a.componentType];
  const n = TS[a.type];
  const stride = bv.byteStride || comp * n;
  const base = (bv.byteOffset || 0) + (a.byteOffset || 0);
  if (a.componentType === 5126) {
    const out = new Float32Array(a.count * n);
    for (let i = 0; i < a.count; ++i) {
      for (let c = 0; c < n; ++c) out[i * n + c] = bin.readFloatLE(base + i * stride + c * 4);
    }
    return out;
  }
  if (a.componentType === 5125 || a.componentType === 5123 || a.componentType === 5121 || a.componentType === 5122) {
    const out = new Uint32Array(a.count);
    for (let i = 0; i < a.count; ++i) {
      const o = base + i * stride;
      out[i] = comp === 4 ? bin.readUInt32LE(o) : comp === 2 ? bin.readUInt16LE(o) : bin.readUInt8(o);
    }
    return out;
  }
  throw new Error('componentType nao suportado ' + a.componentType);
}

function readAccessorFloat(gltf, bin, idx) {
  const a = gltf.accessors[idx];
  if (a.componentType === 5126) return readAccessor(gltf, bin, idx);
  const bv = gltf.bufferViews[a.bufferView];
  const comp = CT[a.componentType];
  const n = TS[a.type];
  const stride = bv.byteStride || comp * n;
  const base = (bv.byteOffset || 0) + (a.byteOffset || 0);
  const max = a.componentType === 5123 ? 65535 : 255;
  const out = new Float32Array(a.count * n);
  for (let i = 0; i < a.count; ++i) {
    for (let c = 0; c < n; ++c) {
      const o = base + i * stride + c * comp;
      out[i * n + c] = comp === 2 ? bin.readUInt16LE(o) / max : bin.readUInt8(o) / max;
    }
  }
  return out;
}

function matIdentity() { const m = new Float64Array(16); m[0] = m[5] = m[10] = m[15] = 1; return m; }
function matMul(a, b) {
  const o = new Float64Array(16);
  for (let c = 0; c < 4; ++c) for (let r = 0; r < 4; ++r) {
    let s = 0;
    for (let k = 0; k < 4; ++k) s += a[k * 4 + r] * b[c * 4 + k];
    o[c * 4 + r] = s;
  }
  return o;
}
function matTrs(t, q, s) {
  const [x, y, z, w] = q;
  const xx = x * x, yy = y * y, zz = z * z;
  const xy = x * y, xz = x * z, yz = y * z, wx = w * x, wy = w * y, wz = w * z;
  const m = new Float64Array(16);
  m[0] = (1 - 2 * (yy + zz)) * s[0];
  m[1] = 2 * (xy + wz) * s[0];
  m[2] = 2 * (xz - wy) * s[0];
  m[4] = 2 * (xy - wz) * s[1];
  m[5] = (1 - 2 * (xx + zz)) * s[1];
  m[6] = 2 * (yz + wx) * s[1];
  m[8] = 2 * (xz + wy) * s[2];
  m[9] = 2 * (yz - wx) * s[2];
  m[10] = (1 - 2 * (xx + yy)) * s[2];
  m[12] = t[0]; m[13] = t[1]; m[14] = t[2]; m[15] = 1;
  return m;
}
function nodeMatrix(n) {
  if (n.matrix) return Float64Array.from(n.matrix);
  return matTrs(n.translation || [0, 0, 0], n.rotation || [0, 0, 0, 1], n.scale || [1, 1, 1]);
}
function xformPoint(m, x, y, z) {
  return [
    m[0] * x + m[4] * y + m[8] * z + m[12],
    m[1] * x + m[5] * y + m[9] * z + m[13],
    m[2] * x + m[6] * y + m[10] * z + m[14],
  ];
}
function normalMatrix(m) {
  const a = m[0], b = m[1], c = m[2];
  const d = m[4], e = m[5], f = m[6];
  const g = m[8], h = m[9], i = m[10];
  const det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g) || 1;
  const inv = [
    (e * i - f * h) / det, (f * g - d * i) / det, (d * h - e * g) / det,
    (c * h - b * i) / det, (a * i - c * g) / det, (b * g - a * h) / det,
    (b * f - c * e) / det, (c * d - a * f) / det, (a * e - b * d) / det,
  ];
  return inv; // ja e a inversa; transposta aplicada em xformNormal
}
function xformNormal(nm, x, y, z) {
  // normal = transpose(inverse(M)) * n  ==  inverse(M) aplicada a n por colunas
  return [nm[0] * x + nm[3] * y + nm[6] * z, nm[1] * x + nm[4] * y + nm[7] * z, nm[2] * x + nm[5] * y + nm[8] * z];
}
function lin2srgb(c) {
  c = Math.max(0, Math.min(1, c));
  return c <= 0.0031308 ? c * 12.92 : 1.055 * Math.pow(c, 1 / 2.4) - 0.055;
}

// ---- texturas reais: camadas 15+ no atlas 2D array do motor ----
const TEX_BASE = 15;
const TEX_SIZE = 256;
const texLayers = [];          // buffers JPEG prontos para citytex.bin
const texKeyMap = new Map();   // file#imgIdx -> layer

function downscaleRGBA(t, S) {
  if (t.w === S && t.h === S) return t.d;
  const out = new Uint8Array(S * S * 4);
  const sx = t.w / S, sy = t.h / S;
  for (let y = 0; y < S; ++y) {
    const y0 = Math.floor(y * sy), y1 = Math.max(y0 + 1, Math.floor((y + 1) * sy));
    for (let x = 0; x < S; ++x) {
      const x0 = Math.floor(x * sx), x1 = Math.max(x0 + 1, Math.floor((x + 1) * sx));
      let r = 0, g = 0, b = 0, a = 0, n = 0;
      for (let yy = y0; yy < y1 && yy < t.h; ++yy) {
        for (let xx = x0; xx < x1 && xx < t.w; ++xx) {
          const i = (yy * t.w + xx) * 4;
          r += t.d[i]; g += t.d[i + 1]; b += t.d[i + 2]; a += t.d[i + 3]; n++;
        }
      }
      const o = (y * S + x) * 4, inv = 1 / (n || 1);
      out[o] = r * inv; out[o + 1] = g * inv; out[o + 2] = b * inv; out[o + 3] = a * inv;
    }
  }
  return out;
}

function registerTexture(key, t) {
  if (texKeyMap.has(key)) return texKeyMap.get(key);
  const src = downscaleRGBA(t, TEX_SIZE);
  // mild contrast + slight darkening so facade/window detail reads under bright sun
  const rgba = new Uint8Array(src.length);
  for (let i = 0; i < src.length; i += 4) {
    for (let c = 0; c < 3; ++c) {
      const v = (src[i + c] / 255 - 0.5) * 1.18 + 0.46;
      rgba[i + c] = Math.max(0, Math.min(255, Math.round(v * 255)));
    }
    rgba[i + 3] = src[i + 3];
  }
  const enc = jpeg.encode({ data: Buffer.from(rgba), width: TEX_SIZE, height: TEX_SIZE }, 82);
  const layer = TEX_BASE + texLayers.length;
  texLayers.push(enc.data);
  texKeyMap.set(key, layer);
  return layer;
}

// bitmaps sem detalhe (betao/pintura lisa) viram fachada procedural com janelas
const flatTexCache = new Map();
function texIsFlat(key, t) {
  if (flatTexCache.has(key)) return flatTexCache.get(key);
  const d = downscaleRGBA(t, TEX_SIZE);
  let s = 0, s2 = 0;
  const N = TEX_SIZE * TEX_SIZE;
  for (let p = 0; p < N; ++p) {
    const l = 0.3 * d[p * 4] + 0.5 * d[p * 4 + 1] + 0.2 * d[p * 4 + 2];
    s += l; s2 += l * l;
  }
  const m = s / N;
  const flat = Math.sqrt(Math.max(0, s2 / N - m * m)) < 18;
  flatTexCache.set(key, flat);
  return flat;
}

// indice da imagem de cor base, incluindo KHR_materials_pbrSpecularGlossiness
function baseColorImageOf(gltf, mat) {
  const pbr = mat.pbrMetallicRoughness || {};
  let bt = pbr.baseColorTexture;
  if (!bt && mat.extensions && mat.extensions.KHR_materials_pbrSpecularGlossiness) {
    bt = mat.extensions.KHR_materials_pbrSpecularGlossiness.diffuseTexture;
  }
  if (!bt || !gltf.textures) return null;
  const tex = gltf.textures[bt.index];
  return tex && tex.source !== undefined ? tex.source : null;
}

function factorOf(mat) {
  const pbr = mat.pbrMetallicRoughness || {};
  let f = pbr.baseColorFactor;
  if (!f && mat.extensions && mat.extensions.KHR_materials_pbrSpecularGlossiness) {
    f = mat.extensions.KHR_materials_pbrSpecularGlossiness.diffuseFactor;
  }
  return f || [1, 1, 1, 1];
}

// camadas do atlas do motor (ver enum Layer em src/gfx.h)
const SKIN_LAYER = {
  white: 0, asphalt: 1, sidewalk: 2, roof: 3, facadeA: 4, facadeB: 5, facadeC: 6,
  glass: 7, sand: 8, grass: 9, concrete: 10, shop: 11, blob: 12, rubber: 13,
};
// UV por metro (celula do padrao no atlas de 256 px)
const SKIN_UV = {
  3: [0.025, 0.025],
  4: [0.0781, 0.0848],
  5: [0.0770, 0.0940],
  6: [0.0390, 0.0848],
  7: [0.0500, 0.0600],
  10: [0.0200, 0.0200],
  11: [0.0600, 0.1400],
};

// kit CC0 buildings ship as flat vertex-colour boxes; skin them with windowed facades
function skinAuto(name) {
  return /^(residencial\/predio_|industrial\/|comercial\/(banco|escritorio|hotel|loja_esquina|mercado|restaurante|shopping)|publicos\/(escola|hospital|policia|bombeiros|museu|biblioteca|prefeitura))/.test(name);
}

// kit GLBs are miniatures placed at 6-9.5x; pre-multiply skin UVs so windows
// end up at real-world metre scale instead of one window per facade
function kitUvScale(name) {
  if (/^meus\//.test(name)) return 1;
  if (/predio_alto/.test(name)) return 9.5;
  if (/predio_apartamentos/.test(name)) return 9.0;
  if (/escritorio|hotel/.test(name)) return 9.0;
  if (/^publicos\//.test(name)) return 6.2;
  if (/shopping/.test(name)) return 6.0;
  return 7.3;
}

function skinLayerOf(matName, map) {
  const n = matName || '';
  if (map) for (const k of Object.keys(map)) if (new RegExp(k, 'i').test(n)) return map[k];
  if (/Maskedstuff/i.test(n)) return 'concrete';
  if (/Firstfloortrim/i.test(n)) return 'concrete';
  if (/Concrete/i.test(n)) return 'concrete';
  if (/Brownstone/i.test(n)) return 'facadeB';
  if (/Modern_Firstfloor/i.test(n)) return 'facadeC';
  if (/International_Side/i.test(n)) return 'facadeA';
  if (/Facade_Flr_2/i.test(n)) return 'facadeB';
  if (/Facade_FLR_(4|6|8)/i.test(n)) return 'facadeC';
  if (/Facade_FLR_(1|2|3|7)/i.test(n)) return 'facadeA';
  if (/Firstfloor/i.test(n)) return 'facadeA';
  let h = 0;
  for (let i = 0; i < n.length; ++i) h = (h * 31 + n.charCodeAt(i)) | 0;
  return ['facadeA', 'facadeB', 'facadeC', 'glass'][Math.abs(h) % 4];
}

function skinUv(layer, p, n) {
  const s = SKIN_UV[layer];
  if (!s) return [0, 0];
  const ax = Math.abs(n[0]), ay = Math.abs(n[1]), az = Math.abs(n[2]);
  if (ay > ax && ay > az) return [p[0] * s[0], p[2] * s[1]];
  const u = ax >= az ? p[2] : p[0];
  return [u * s[0], p[1] * s[1]];
}

function convertModel(file, bright = 1, pick = null, skin = false, skinMap = null, skip = null,
                      skinTint = 1, tintMul = null, texMode = false) {
  const { gltf, bin } = parseGlb(file);
  const verts = [];
  const idx = [];
  const colCache = new Map();
  const texCache = new Map();
  let minX = Infinity, minY = Infinity, minZ = Infinity, maxX = -Infinity, maxY = -Infinity, maxZ = -Infinity;

  function textureOf(imgIdx) {
    if (texCache.has(imgIdx)) return texCache.get(imgIdx);
    const img = gltf.images && gltf.images[imgIdx];
    let tex = null;
    if (img && img.bufferView !== undefined) {
      const bv = gltf.bufferViews[img.bufferView];
      const off = bv.byteOffset || 0;
      const data = bin.slice(off, off + bv.byteLength);
      try {
        if ((img.mimeType || '').includes('png')) {
          const png = PNG.sync.read(data);
          tex = { w: png.width, h: png.height, d: png.data };
        } else if ((img.mimeType || '').includes('jpeg') || (img.mimeType || '').includes('jpg')) {
          const j = jpeg.decode(data, { useTArray: true, formatAsRGBA: true });
          tex = { w: j.width, h: j.height, d: j.data };
        }
      } catch (e) {
        tex = null;
      }
    }
    texCache.set(imgIdx, tex);
    return tex;
  }

  function sampleHit(mat, u, v) {
    const pbr = mat.pbrMetallicRoughness || {};
    const bt = pbr.baseColorTexture;
    if (!bt || !gltf.textures) return null;
    const tex = gltf.textures[bt.index];
    const imgIdx = tex && tex.source !== undefined ? tex.source : undefined;
    if (imgIdx === undefined) return null;
    const t = textureOf(imgIdx);
    if (!t) return null;
    let x = Math.floor((u - Math.floor(u)) * t.w);
    let y = Math.floor((v - Math.floor(v)) * t.h);
    if (x < 0) x = 0; if (x >= t.w) x = t.w - 1;
    if (y < 0) y = 0; if (y >= t.h) y = t.h - 1;
    const i = (y * t.w + x) * 4;
    return [t.d[i], t.d[i + 1], t.d[i + 2], t.d[i + 3]];
  }

  function packColor(r, g, b, a) {
    return ((r & 255) | ((g & 255) << 8) | ((b & 255) << 16) | ((a & 255) << 24)) >>> 0;
  }

  function colorOf(matIdx) {
    if (colCache.has(matIdx)) return colCache.get(matIdx);
    const mat = (gltf.materials && gltf.materials[matIdx]) || {};
    const f = (mat.pbrMetallicRoughness && mat.pbrMetallicRoughness.baseColorFactor) || [1, 1, 1, 1];
    const r = Math.round(lin2srgb(f[0] * bright) * 255);
    const g = Math.round(lin2srgb(f[1] * bright) * 255);
    const b = Math.round(lin2srgb(f[2] * bright) * 255);
    const a = Math.round(Math.max(0, Math.min(1, f[3] === undefined ? 1 : f[3])) * 255);
    const c = (r | (g << 8) | (b << 16) | (a << 24)) >>> 0;
    colCache.set(matIdx, c);
    return c;
  }

  const tintCache = new Map();
  function tintOf(matIdx) {
    if (tintCache.has(matIdx)) return tintCache.get(matIdx);
    const mat = (gltf.materials && gltf.materials[matIdx]) || {};
    const pbr = mat.pbrMetallicRoughness || {};
    let tint = [1, 1, 1];
    const bt = pbr.baseColorTexture;
    if (bt && gltf.textures) {
      const tex = gltf.textures[bt.index];
      const t = tex && tex.source !== undefined ? textureOf(tex.source) : null;
      if (t) {
        let r = 0, g = 0, b = 0, n = 0;
        for (let sy = 0; sy < 12; ++sy) {
          for (let sx = 0; sx < 12; ++sx) {
            const i = (((sy * 16 + 8) % t.h) * t.w + ((sx * 21 + 5) % t.w)) * 4;
            r += t.d[i]; g += t.d[i + 1]; b += t.d[i + 2]; n++;
          }
        }
        tint = [r / n, g / n, b / n].map((c) => Math.min(0.88, Math.max(0.62, 0.62 + 0.26 * (c / 255))));
      }
    }
    if (skinTint !== 1) tint = tint.map((c) => c * skinTint);
    if (tintMul) tint = tint.map((c, i) => Math.min(1, c * tintMul[i]));
    const packed = packColor(Math.round(tint[0] * 255), Math.round(tint[1] * 255), Math.round(tint[2] * 255), 255);
    tintCache.set(matIdx, packed);
    return packed;
  }

  function addMesh(meshIdx, world) {
    const mesh = gltf.meshes[meshIdx];
    const nm = normalMatrix(world);
    for (const prim of mesh.primitives) {
      if ((prim.mode === undefined ? 4 : prim.mode) !== 4) continue;
      const pos = readAccessor(gltf, bin, prim.attributes.POSITION);
      const nrm = prim.attributes.NORMAL !== undefined ? readAccessor(gltf, bin, prim.attributes.NORMAL) : null;
      const ind = prim.indices !== undefined ? readAccessor(gltf, bin, prim.indices) : null;
      const cnt = pos.length / 3;
      const base = verts.length;
      const matIdx = prim.material !== undefined ? prim.material : 0;
      const mat = (gltf.materials && gltf.materials[matIdx]) || {};
      const skinLayer = skin ? SKIN_LAYER[skinLayerOf(mat.name, skinMap)] : -1;
      const col = skin ? tintOf(matIdx) : colorOf(matIdx);
      const uv = prim.attributes.TEXCOORD_0 !== undefined ? readAccessorFloat(gltf, bin, prim.attributes.TEXCOORD_0) : null;
      const hasTex = !!(mat.pbrMetallicRoughness && mat.pbrMetallicRoughness.baseColorTexture);
      let texLayer = -1;
      if (texMode && uv) {
        const img = baseColorImageOf(gltf, mat);
        if (img !== null) {
          const t = textureOf(img);
          if (t && !texIsFlat(file + '#' + img, t)) texLayer = registerTexture(file + '#' + img, t);
        }
      }
      const ff = factorOf(mat);
      const texCol = packColor(Math.round(lin2srgb(ff[0] * bright) * 255),
        Math.round(lin2srgb(ff[1] * bright) * 255), Math.round(lin2srgb(ff[2] * bright) * 255),
        Math.round(Math.max(0, Math.min(1, ff[3] === undefined ? 1 : ff[3])) * 255));
      const fr = ((col & 255) / 255), fg = (((col >> 8) & 255) / 255), fb = (((col >> 16) & 255) / 255), fa = ((col >>> 24) & 255);
      for (let i = 0; i < cnt; ++i) {
        const p = xformPoint(world, pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2]);
        let nx = 0, ny = 1, nz = 0;
        if (nrm) {
          const t = xformNormal(nm, nrm[i * 3], nrm[i * 3 + 1], nrm[i * 3 + 2]);
          const l = Math.hypot(t[0], t[1], t[2]) || 1;
          nx = t[0] / l; ny = t[1] / l; nz = t[2] / l;
        }
        let vc = col;
        let layer = skinLayer;
        if (skin && Math.abs(ny) > 0.7 && layer !== 10 && layer !== 11) layer = 3;
        let vuv = null;
        if (texLayer >= TEX_BASE) {
          layer = texLayer; vc = texCol; vuv = [uv[i * 2], uv[i * 2 + 1]];
        } else if (texMode) {
          // source material has no bitmap: give concrete/glass bodies a windowed
          // facade instead of a flat slab so the model still reads as a building
          let pl;
          if (Math.abs(ny) > 0.7) pl = 3;
          else if (ff[2] > ff[0] * 1.15 && ff[2] > ff[1] * 1.05) pl = 7;
          else if (ff[0] > ff[1] * 1.12 && ff[0] > ff[2] * 1.2) pl = 6;
          else pl = matIdx % 2 ? 5 : 4;
          const m = Math.max(ff[0], ff[1], ff[2], 0.001);
          layer = pl;
          vc = packColor(Math.round(lin2srgb(ff[0] / m) * 255), Math.round(lin2srgb(ff[1] / m) * 255),
            Math.round(lin2srgb(ff[2] / m) * 255), Math.round(Math.max(0, Math.min(1, ff[3] === undefined ? 1 : ff[3])) * 255));
          vuv = skinUv(pl, p, [nx, ny, nz]);
        } else if (!skin && uv && hasTex) {
          const s = sampleHit(mat, uv[i * 2], uv[i * 2 + 1]);
          if (s) vc = packColor(Math.round(s[0] * fr), Math.round(s[1] * fg), Math.round(s[2] * fb), Math.round(s[3] * (fa / 255)));
        }
        if (tintMul) {
          vc = packColor(
            Math.round((vc & 255) * tintMul[0]),
            Math.round(((vc >> 8) & 255) * tintMul[1]),
            Math.round(((vc >> 16) & 255) * tintMul[2]),
            (vc >>> 24) & 255);
        }
        verts.push({ p, n: [nx, ny, nz], col: vc, layer, uv: vuv });
        if (p[0] < minX) minX = p[0];
        if (p[1] < minY) minY = p[1];
        if (p[2] < minZ) minZ = p[2];
        if (p[0] > maxX) maxX = p[0];
        if (p[1] > maxY) maxY = p[1];
        if (p[2] > maxZ) maxZ = p[2];
      }
      if (ind) {
        for (let i = 0; i < ind.length; ++i) idx.push(base + ind[i]);
      } else {
        for (let i = 0; i < cnt; ++i) idx.push(base + i);
      }
    }
  }

  function walk(ni, parent, inside) {
    const n = gltf.nodes[ni];
    const nname = n.name || '';
    if (skip && skip.some((r) => r.test(nname))) return;
    const world = matMul(parent, nodeMatrix(n));
    const sel = inside || !pick || pick.some((p) => nname === p);
    if (n.mesh !== undefined && sel) addMesh(n.mesh, world);
    if (n.children) for (const c of n.children) walk(c, world, sel);
  }

  const scene = gltf.scenes[gltf.scene || 0];
  for (const ni of scene.nodes) walk(ni, matIdentity(), false);

  const cx = (minX + maxX) * 0.5;
  const cz = (minZ + maxZ) * 0.5;
  const by = minY;
  for (const v of verts) {
    v.p[0] -= cx;
    v.p[1] -= by;
    v.p[2] -= cz;
  }
  return {
    verts,
    idx,
    min: [minX - cx, 0, minZ - cz],
    max: [maxX - cx, maxY - by, maxZ - cz],
    tris: idx.length / 3,
  };
}

function decimate(model, cell) {
  if (!cell || cell <= 0) return model;
  const map = new Map();
  const nv = model.verts.length;
  const remap = new Int32Array(nv);
  const acc = [];
  for (let i = 0; i < nv; ++i) {
    const v = model.verts[i];
    const kx = Math.round(v.p[0] / cell), ky = Math.round(v.p[1] / cell), kz = Math.round(v.p[2] / cell);
    let key = kx + ',' + ky + ',' + kz + ',' + v.layer;
    const keepUv = !!v.uv && v.layer >= TEX_BASE;
    if (keepUv) key += ',' + Math.round(v.uv[0] * 96) + ',' + Math.round(v.uv[1] * 96);
    let c = map.get(key);
    if (c === undefined) {
      c = acc.length;
      map.set(key, c);
      acc.push({ p: [kx * cell, ky * cell, kz * cell], n: [0, 0, 0], rgb: [0, 0, 0, 0], uv: [0, 0], hasUv: keepUv, count: 0, layer: v.layer });
    }
    const a = acc[c];
    a.n[0] += v.n[0]; a.n[1] += v.n[1]; a.n[2] += v.n[2];
    a.rgb[0] += v.col & 255;
    a.rgb[1] += (v.col >> 8) & 255;
    a.rgb[2] += (v.col >> 16) & 255;
    a.rgb[3] += (v.col >>> 24) & 255;
    if (keepUv) { a.uv[0] += v.uv[0]; a.uv[1] += v.uv[1]; }
    a.count += 1;
    remap[i] = c;
  }
  const verts = acc.map((a) => {
    const inv = 1 / a.count;
    const l = Math.hypot(a.n[0], a.n[1], a.n[2]) || 1;
    const r = Math.round(a.rgb[0] * inv), g = Math.round(a.rgb[1] * inv);
    const b = Math.round(a.rgb[2] * inv), al = Math.round(a.rgb[3] * inv);
    return { p: a.p, n: [a.n[0] / l, a.n[1] / l, a.n[2] / l],
             col: (r | (g << 8) | (b << 16) | (al << 24)) >>> 0, layer: a.layer,
             uv: a.hasUv ? [a.uv[0] * inv, a.uv[1] * inv] : null };
  });
  const idx = [];
  for (let i = 0; i < model.idx.length; i += 3) {
    const a = remap[model.idx[i]], b = remap[model.idx[i + 1]], c = remap[model.idx[i + 2]];
    if (a === b || b === c || a === c) continue;
    idx.push(a, b, c);
  }
  return { verts, idx, min: model.min, max: model.max, tris: idx.length / 3 };
}

function computeHeaderOffsets(models) {
  const H = 8 + models.length * 56;
  let vOff = H;
  for (const m of models) { m.vertOff = vOff; vOff += m.verts.length * 40; }
  let iOff = vOff;
  for (const m of models) { m.idxOff = iOff; iOff += m.idx.length * 4; }
  let nOff = iOff;
  for (const m of models) { m.nameOff = nOff; nOff += Buffer.byteLength(m.name); }
  return { total: nOff, modelCount: models.length };
}

function writeMwb(models, file) {
  const h = computeHeaderOffsets(models);
  const buf = Buffer.alloc(h.total);
  buf.write('MWB1', 0, 'ascii');
  buf.writeUInt32LE(h.modelCount, 4);
  let o = 8;
  for (const m of models) {
    buf.writeUInt32LE(m.nameOff, o); buf.writeUInt32LE(Buffer.byteLength(m.name), o + 4);
    buf.writeUInt32LE(m.vertOff, o + 8); buf.writeUInt32LE(m.idxOff, o + 12);
    buf.writeUInt32LE(m.verts.length, o + 16); buf.writeUInt32LE(m.idx.length, o + 20);
    buf.writeFloatLE(m.min[0], o + 24); buf.writeFloatLE(m.min[1], o + 28); buf.writeFloatLE(m.min[2], o + 32);
    buf.writeFloatLE(m.max[0], o + 36); buf.writeFloatLE(m.max[1], o + 40); buf.writeFloatLE(m.max[2], o + 44);
    buf.writeUInt32LE(m.tris, o + 48);
    buf.writeUInt32LE(m.verts.reduce((a, v) => a, 0), o + 52);
    o += 56;
  }
  for (const m of models) {
    let p = m.vertOff;
    for (const v of m.verts) {
      buf.writeFloatLE(v.p[0], p); buf.writeFloatLE(v.p[1], p + 4); buf.writeFloatLE(v.p[2], p + 8);
      buf.writeFloatLE(v.n[0], p + 12); buf.writeFloatLE(v.n[1], p + 16); buf.writeFloatLE(v.n[2], p + 20);
      const layer = v.layer === undefined ? -1 : v.layer;
      const mul = kitUvScale(m.name);
      const base = v.uv ? v.uv : (layer >= 0 && layer < TEX_BASE ? skinUv(layer, v.p, v.n) : [0, 0]);
      const uv = [base[0] * mul, base[1] * mul];
      buf.writeFloatLE(uv[0], p + 24); buf.writeFloatLE(uv[1], p + 28);
      buf.writeFloatLE(layer, p + 32);
      buf.writeUInt32LE(v.col >>> 0, p + 36);
      p += 40;
    }
    let q = m.idxOff;
    for (const i of m.idx) { buf.writeUInt32LE(i, q); q += 4; }
    buf.write(m.name, m.nameOff, 'utf8');
  }
  fs.writeFileSync(file, buf);
  return buf.length;
}

const kitDir = path.join(ROOT, cfg.kit);
const userDir = path.join(ROOT, cfg.user_dir || 'assets-src/meus-modelos');
const outDir = path.join(ROOT, cfg.out);
fs.mkdirSync(outDir, { recursive: true });

const models = [];
let totalTris = 0;
for (const entry of cfg.models) {
  const e = typeof entry === 'string' ? { name: entry } : entry;
  const name = e.name;
  if (only && !only.has(name)) continue;
  const file = [e.file ? path.join(userDir, e.file) : null,
    path.join(userDir, name.replace(/^meus\//, '') + '.glb'), path.join(kitDir, name + '.glb')].find((f) => f && fs.existsSync(f));
  if (!file) throw new Error('modelo em falta: ' + name);
  const cat = name.split('/')[0];
  const cell = e.cell !== undefined ? e.cell : ({ mobiliario: 0.022, vegetacao: 0.025, parques: 0.05, beira_rio: 0.04, veiculos: 0 })[cat] ?? 0.05;
  const pick = e.pick ? (Array.isArray(e.pick) ? e.pick : [e.pick]) : null;
  const skip = e.skip ? e.skip.map((r) => new RegExp(r)) : null;
  const raw = convertModel(file, e.brighten || 1, pick, !!e.skin || skinAuto(name), e.skin_map || null, skip,
    e.skin_tint || 1, e.tint || null, !!e.tex);
  if (e.scale && e.scale !== 1) {
    for (const v of raw.verts) {
      v.p[0] *= e.scale; v.p[1] *= e.scale; v.p[2] *= e.scale;
    }
    raw.min = raw.min.map((x) => x * e.scale);
    raw.max = raw.max.map((x) => x * e.scale);
  }
  const m = cell > 0 ? decimate(raw, cell) : raw;
  m.name = name;
  models.push(m);
  totalTris += m.tris;
  console.log(
    name.padEnd(34),
    String(m.verts.length).padStart(6), 'v',
    String(m.tris).padStart(5), 'tris',
    '(' + raw.tris + ')',
    'foot', (m.max[0] - m.min[0]).toFixed(2), 'x', (m.max[2] - m.min[2]).toFixed(2),
    'h', m.max[1].toFixed(2)
  );
}
models.sort((a, b) => a.name.localeCompare(b.name));
const bytes = writeMwb(models, path.join(outDir, 'city.mwb'));
// citytex.bin: MTX1 + count + (len + JPEG) por camada de textura (15+)
{
  let total = 8;
  for (const t of texLayers) total += 4 + t.length;
  const tb = Buffer.alloc(total);
  tb.write('MTX1', 0, 'ascii');
  tb.writeUInt32LE(texLayers.length, 4);
  let o = 8;
  for (const t of texLayers) {
    tb.writeUInt32LE(t.length, o); o += 4;
    Buffer.from(t).copy(tb, o); o += t.length;
  }
  fs.writeFileSync(path.join(outDir, 'citytex.bin'), tb);
}
fs.writeFileSync(
  path.join(outDir, 'manifest.json'),
  JSON.stringify({
    count: models.length,
    tris: totalTris,
    bytes,
    texCount: texLayers.length,
    models: models.map((m) => ({
      name: m.name,
      verts: m.verts.length,
      tris: m.tris,
      size: [m.max[0] - m.min[0], m.max[1], m.max[2] - m.min[2]],
    })),
  }, null, 1)
);
console.log(`\n${models.length} modelos, ${totalTris} tris, ${(bytes / 1024).toFixed(1)} KB -> web/assets/city.mwb`);
console.log(`${texLayers.length} texturas reais -> web/assets/citytex.bin`);
