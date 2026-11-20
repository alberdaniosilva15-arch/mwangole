# ESTADO DO PROJECTO — onde paramos e o que falta

Actualizado: 20/11/2026 · branch `main`

Documento de acompanhamento. O plano completo está em `.opencode/plan/Mwangole-PLANO-UNICO.md`
e o histórico técnico nos commits `F0` → `F3c`.

---

## 1. Números actuais (medidos, não estimados)

| métrica | baseline (F0) | agora | alvo |
|---|---|---|---|
| fps mínimo (6 câmaras) | 18,9 | **48–60** | ≥30 |
| fps no spawn | 23,8 | **54,3** | ≥30 |
| triângulos/frame | 2 861 000 | **487 000** | ≤750 000 |
| draw calls | 15 | 21 | ≤600 |
| modelos | 71 | **83** | — |
| triângulos únicos | 165 102 | 224 269 | — |
| payload gz | 2 424 KB | **3 050 KB** | ≤3 000 KB (a rever) |
| arranque | 3,1 s | 3,1 s | ≤6 s |
| `check.mjs` | verde | verde | verde |

Nota: o payload passou 50 KB acima do alvo de 3 MB por causa dos carros em
detalhe. Decisão pendente: subir o alvo para 4 MB ou decimar mais os carros.

---

## 2. O que está feito

### F0 — instrumentação (commit `2f3a293`)
- `tools/look.mjs` — 6 câmaras fixas × 3 horas, mede fps, contraste (`sd`) e
  densidade de detalhe (`edges`)
- `tools/perf.mjs` — p50/p95/max frame time, draw calls, triângulos
- `tools/gate.mjs` — veredicto por fase com exit code
- `tools/baseline.mjs` — congela `bench/baseline.json`
- exports de debug: `mw_debug_lot`, `mw_debug_lot_nearest`, `mw_debug_stats`,
  `mw_debug_chunk_stats`, `mw_debug_cars`

### F1 — base visual (commit `93e6333`, **parcial**)
- fog coerente com o horizonte (antes: 220→1150 m, nunca acontecia)
- sol limitado a ~55° de elevação e azimute variável com a hora
  (ao meio-dia todas as fachadas ficavam com `ndl=0`)
- oclusão falsa por vértice + sombra de contacto no passeio
- `L_GLASS` com caixilharia de 5 px (antes 1 px, desaparecia nos mipmaps)
- paleta de lotes com cor real (cal, terracota, areia, azul)
- mobiliário de rua: bancos, lixeiras, paragens, hidrantes
- **três defeitos estruturais corrigidos**:
  - `place_landmark` sem `model_scale_max` → POIs ampliavam miniaturas 33×
    (um "restaurante" de 45 m)
  - `place_lot` sem fallback para modelos sem fachada
  - `model_dominant_layer` contava telhados, escondendo o material das paredes

> Gate F1 **reprovado** na altura (fps 18,9, contraste 34,1 vs 43,8). O motivo
> era o gargalo de geometria, resolvido no F2.

### F2 — LOD e corte por distância (commit `06a0511`)
- chunk 832 m → **416 m** (48 chunks): dá granularidade ao LOD
- `cluster_simplify` no motor: malha distante a 2 m nas fachadas
- near/far pela distância à **borda** do chunk (120 m)
- corte por distância alinhado com o fog
- resultado: **2,86 M → 0,49 M tris/frame**, 18,9 → **51,8 fps mínimo**
- gate F2 **PASSA**

### F3 — cidade e carros do utilizador (commits `810f160`, `6dd556c`)
- `glb2mw.mjs` descodifica **KHR_draco_mesh_compression** com `draco3d`
  (`GetTrianglesUInt32Array` devolvia zeros → passou a `GetFaceFromMesh`)
- 6 prédios convertidos e colocados em coordenadas fixas à volta do spawn:
  `oc_hotel`, `oc_apartments`, `oc_banco`, `oc_showroom`, `oc_garagem`, `oc_lojas`
- 5 carros convertidos e em uso:
  - jogador → `oc_sedan`
  - trânsito → `oc_hatch` / `oc_exotic` (alternados por índice, estável)
  - polícia → `oc_police`
  - `oc_scooter` e `oc_police_heli` convertidos mas ainda **não colocados**
- peças de dano (`^Damage`) e LODs duplicados (`^LOD[23]`) excluídos —
  apareciam como calhaus em cima do carro

---

## 3. O que está em falta

### Bloqueantes de qualidade (próximo trabalho)

1. **Sombras** — não existe shadow map. É a maior causa do ar "chapado".
   Plano: 2 cascatas 1024², PCF 3×3, só chunks próximos projectam.
   Orçamento: ~3–5 ms (temos 20 ms livres).
2. **PBR** — tudo usa atlas de 256 px com cor por vértice. Falta
   normal map, roughness/metalness por textura e KTX2.
3. **Upscaler (FSR1)** — render a 67% com EASU+RCAS. Devolve 35–45% de frame
   time e permite subir a densidade.

### Conteúdo em falta

4. **Rodas dos carros não giram** — o modelo GLB é desenhado inteiro e estático.
   Solução: separar as rodas por nome de nó no conversor e animá-las.
5. **Helicóptero da polícia** convertido mas não usado.
6. **Scooter** convertida mas não usada no trânsito.
7. **Personagens** — os peões e o jogador são figuras blocadas de 6 peças.
   Plano: MakeHuman (CC0) + Quaternius UAL (CC0) + `ozz-animation`.
8. **Pontes** — só existe o aterro para a Ilha; falta ponte suspensa.
9. **Estádio** — não existe.
10. **Rodovia** — a grelha é toda igual; falta via rápida.
11. **Biomas** — Baixa/Alta/Musseque têm paleta diferente, mas falta vegetação
    e solo próprios por zona.
12. **Clima** — sem chuva, sem asfalto molhado, sem variação de tempo.
13. **Água** — o mar é uma malha plana; falta reflexo e ondas.
14. **Idiomas** — só pt-AO; falta tabela de strings e en/fr.

### Sistemas de jogo (M5–M9)

15. **M5 economia e save** — 20% / 0%. Save versionado, garagem, posto.
16. **M6 combate** — 0%.
17. **M7 diálogo** — 0%.
18. **M8 missões** — só a estafeta.
19. **M9 áudio, clima e PWA** — base em `web/audio.js`.

### Dívida técnica conhecida

- payload 50 KB acima do alvo
- **alguns modelos do utilizador têm faces sem textura por desenho**
  (`loja_downtown`, `condominio`): precisam de unwrap no Blender ou substituição
  pelos kits CC0 (Zero Energy / Quaternius)
- `predio_sanzio`: 91% dos meshes sem UV
- sem LOD dentro do chunk que contém o jogador (só near/far por chunk)
- `mw_debug_stats` mede só o último frame; não há contador de VRAM

---

## 4. Ordem sugerida para continuar

```
1. Sombras (F6)            ← maior ganho visual por dia de trabalho
2. Rodas a girar           ← o carro parece morto sem isto
3. PBR + KTX2 (F5)
4. FSR1 (F4)
5. Personagens (F8)
6. Clima + água
7. M5 → M9
```

---

## 5. Como correr

```bash
python tools/build.py          # emcc (D:\emsdk)
python tools/serve.py          # http://127.0.0.1:8090
node tools/check.mjs           # jogo: condução, trânsito, peões
node tools/look.mjs f4         # 6 câmaras × 3 horas
node tools/perf.mjs f4         # frame time a conduzir
node tools/gate.mjs F2 f4      # veredicto
node tools/glb2mw.mjs          # só se mexer em modelos
```

Chrome de teste (headless, CDP em 9222):

```
chrome --headless=new --remote-debugging-port=9222
       --user-data-dir=<tmp> --enable-gpu --ignore-gpu-blocklist
       --disable-background-timer-throttling --window-size=1280,720
```
