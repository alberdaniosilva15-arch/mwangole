# PLANO MWANGOLÉ — de cidade de caixas a jogo GTA em Luanda

> Documento de direcção. Última actualização: 2026-11-17.
> Estado: M0–M2 concluídos (motor, condução, trânsito, peões, polícia, áudio).

---

## 0. Diagnóstico honesto (porque é que está tudo quadrado)

| O que parece | O que é na realidade |
|---|---|
| "Os GLB não aparecem" | **Não existe nenhum GLB no projecto.** `web/` só tem `index.html`, `hud.css`, `shell.js` e `build/` (164 KB). A pesquisa das bibliotecas ficou só na pesquisa. |
| Cidade quadrada | Os edifícios são gerados por código: `Builder::box` + fachadas pintadas numa textura 2D array (`src/world.h`, `src/gfx.h`). Não há modelos. |
| "Kz 7.500" | É uma variável (`g_cash`) que nunca muda — não há economia. |
| Sem armas, sem conversas, sem missões | Não implementado. Não há sistema de interacção, inventário, diálogo nem missões. |

**Conclusão:** o motor está sólido (60 fps a 720p, 127 KB de wasm, colisões, IA de trânsito, polícia, áudio). O que falta é o **jogo**: arte real e sistemas de gameplay. Este plano ataca isso por ordem de impacto.

---

## 1. Visão do produto

Um GTA-like **leve**, jogável em qualquer navegador (PC e telemóvel), ambientado em **Luanda**, com:

- Cidade reconhecível: Baixa, Cidade Alta, Ilha do Cabo, musseques, Kilamba.
- Carros com modelos reais (não caixas), casas, lojas, árvores, postes, bancas.
- **Dinheiro (Kz) que ganha e gasta**, casas à venda, garagem, lojas.
- **Pistola**, mira, tiros, reacção da polícia e dos peões.
- **Conversas** com NPCs e **missões** com marcadores, objectivos e recompensas.
- Polícia com 5 níveis de procurado, hospital, save no dispositivo.

Barra de qualidade: tem de **parecer um jogo**, não uma demo técnica. Orçamento de performance: **60 fps a 1280×720 no HD 4000** e jogável a 30+ fps em telemóvel médio.

---

## 2. Arquitectura técnica

### 2.1 O que já existe e é reaproveitado

```
src/math.h      vectores/matrizes
src/gfx.h       shaders, Mesh, instancing, textura 2D array
src/world.h     gerador de cidade por chunks, colisores AABB
src/car.h       física arcade do carro
src/player.h    peões procedurais (corpo, braços, pernas)
src/street.h    trânsito, peds de rua, polícia
src/main.cpp    loop, câmara, HUD, heat/wanted
web/shell.js    input, áudio WebAudio, HUD, bench
tools/*.mjs     testes CDP (check, street, police, audio, bench)
```

### 2.2 Pipeline de arte (a peça que falta)

Regra: **zero parsing de GLB em runtime.** Tudo é convertido em tempo de build para um formato binário compacto.

```
assets/models/*.glb   →  tools/glb2mw.mjs  →  web/assets/city.mwb + manifest.json
                                              ↓ (preload Emscripten)
                                        src/assets.h → Mesh (vertex color)
                                              ↓
                                        src/world.h  → merge por chunk
```

- **Formato de vértice** já existente (40 bytes): pos(3) + normal(3) + uv(2) + layer(1) + cor RGBA8(4).
- **Cor por vértice**: `layer = -1` (sentinela "flat"). O fragment shader usa `vColor` em vez da textura quando `layer < 0`. Uma alteração pequena em `gfx.h`.
- **Fusão estática por chunk**: os modelos de edifícios são transformados para o mundo e fundidos no mesh `bld` do chunk → mantém 1 draw call por chunk (crítico para o HD 4000). Props (árvores, postes, bancas) fundidos no mesh `props`; veículos ficam instanciados (são dinâmicos).
- **Escala**: kid usa 1 lote = 2×2 unidades; convertemos para metros (`LOT = 12 m`), com o pé assente em y = 0 e frente em −Z (convenção glTF, igual à nossa).

### 2.3 Sistemas de gameplay (novos)

```
src/assets.h    modelos convertidos + colocação
src/economy.h   dinheiro, preços, compras, save/load
src/interact.h  prompts E, portas, lojas, conversas (estado)
src/weapons.h   pistola, mira, raycast, projécteis, munição
src/dialogue.h  guiões de conversa (dados), máquina de estados
src/missions.h  definição de missões, objectivos, marcadores, falha/retry
src/save.h      localStorage (via EM_JS), versionamento
```

HUD/UI em HTML/CSS (`web/index.html`, `hud.css`, `shell.js`): diálogo, loja, telefone (mapa/missões/save), mira, munição, aviso de missão. O C++ expõe estado por EM_JS (padrão já usado: `mw_hud`, `mw_street_js`).

---

## 3. Fases

Ordem pensada para que **cada fase seja jogável e visível**. Nenhuma fase depende de arte que ainda não exista.

### Fase M3 — Arte real (matar os quadrados)  ·  esforço L  ·  impacto máximo

**Objectivo:** a cidade deixa de ser caixas. Casas, lojas, prédios, árvores, postes, bancas e veículos com modelos CC0.

**Fonte:** `kayquer/kit-cidade-lowpoly` — CC0 1.0, 129 GLB (~5,6 MB), ≤1.500 tris/modelo, cores planas, sem texturas, 1 lote = 2×2. Categorias úteis: Residencial (5), Comércio (8), Serviços públicos (12), Parques (5), Ruas (12, não usamos), Terreno (9, não usamos), Veículos (10), Mobiliário urbano (20), Vegetação (7), Beira-rio (6).

**Tarefas**
1. Descarregar o kit e seleccionar ~45 ficheiros (todas as casas/lojas/prédios, 6 serviços, 7 árvores, ~10 mobiliário, 6 veículos). Vendorizar em `assets/models/` + `assets/CREDITS.md` (CC0, atribuição opcional).
2. `tools/glb2mw.mjs` (Node puro, sem dependências):
   - ler contentor GLB (header 12 B, chunk JSON, chunk BIN), acessores e hierarquia de nós;
   - aplicar transformações TRS; triângulos (mode 4), normais corrigidas pela inversa-transposta;
   - `material.pbrMetallicRoughness.baseColorFactor` → cor RGBA8 por vértice (converter linear→sRGB);
   - centrar em XZ, assentar em Y=0, calcular bounds;
   - emitir `city.mwb`: header `MWB1`, tabela de modelos (nome, offset, nº vértices/índices, bounds, escala) + `manifest.json` para o C++ e para os testes.
3. `src/assets.h`: carregar `city.mwb` (preload Emscripten), construir `Mesh`, expor `models_find(name)`, `model_stats()` via EM_JS.
4. `src/gfx.h`: caminho de cor plana no shader (`layer = -1`), fog/luz iguais.
5. `src/world.h`: substituir a geração de fachadas por colocação de modelos:
   - 4 distritos (ver M4) com paletas de modelos diferentes;
   - lotes de 12–18 m, frente virada para a rua, yaw em passos de 90°;
   - fusão no mesh `bld` do chunk + AABB de colisão a partir dos bounds;
   - props fundidos no mesh `props`; árvores existentes passam a usar os 7 modelos do kit.
6. Veículos: modelos do kit instanciados para trânsito e estacionados (10 modelos, 1 draw cada). O carro do jogador fica procedural por agora (tem rodas animadas); reavaliar no fim da fase com screenshot lado a lado.
7. Testes: `tools/assets.mjs` (contagens, tris, tamanho, screenshots de um quarteirão de perto, rua e topo).

**Aceitação**
- ≥8 silhuetas distintas por distrito; casas, lojas e prédios visíveis de perto e de longe.
- 60 fps p50 a 1280×720; aumento de draw calls ≤ 20%; payload total (wasm+assets) ≤ 3 MB gz.
- Colisões mantidas: carro trava nas casas, peões e polícia circulam, crash test passa.

**Risco:** escala/colisão dos modelos e cores demasiado escuras (linear vs sRGB) — validar com screenshots antes de espalhar por toda a cidade.

---

### Fase M4 — Cidade com lógica de jogo  ·  esforço M

**Objectivo:** a cidade passa a ter destinos e identidade.

**Tarefas**
- 4 distritos em quadrantes de 4×4 blocos:
  - **Baixa**: torres, bancos, escritórios, hotel, restaurante (trânsito denso).
  - **Cidade Alta / Kilamba**: prédios altos com fachada pintada, praças.
  - **Musseque (Sambizanga)**: casas simples, geminadas, sobrados, bancas, ruas mais estreitas.
  - **Ilha do Cabo**: marginal, palmeiras, quiosques, beira-rio.
- **Pontos de interesse (POI)** fixos com marcador no minimapa: 3 casas à venda, garagem, mercearia, restaurante (cura), loja de armas, 3 missões, praça de candongueiros.
- Portas interactivas: volume + prompt "E · Entrar" (prioridade sobre entrar no carro quando mais perto).
- Minimapa: ícones HTML sobre o canvas (casa, loja, missão, polícia), legenda.
- Semáforos simples nos cruzamentos principais (ciclo de 12 s) para o trânsito não colidir em cruzamentos.

**Aceitação:** percorrer a cidade e identificar os bairros pela arquitectura; todos os POIs aparecem no mapa; E funciona com prioridade correcta (porta vs carro vs NPC).

---

### Fase M5 — Economia e propriedade  ·  esforço M

**Objectivo:** o dinheiro tem fonte e destino. Casas à venda, como pediste.

**Tarefas**
- `g_cash` passa a ser saldo real; HUD anima mudanças (+/− a verde/vermelho).
- Fontes: missões (2.000–8.000 Kz), corridas de candongueiro (300–900 Kz), entregas.
- Gastos: casas 25.000 / 45.000 / 90.000 Kz (3 níveis, cada uma com garagem e ponto de spawn), pistola 6.000, munições 400, kit de saúde 800.
- Menu de compra (HTML) ao premir E no letreiro "À VENDA"; casas compradas mostram "TUA" no mapa e passam a ser safehouse: cura + guardar.
- Garagem: guardar/retirar o carro actual.
- Save: `localStorage` com `{versão, cash, casas, armas, missão, posição}`; botão "Guardar" no telemóvel e auto-save ao entrar em casa.
- Preços e recompensas num único sítio (`src/economy.h`) para ajuste rápido.

**Aceitação:** começar com 7.500 Kz, fazer uma missão, comprar a primeira casa e guardar; recarregar a página mantém tudo.

---

### Fase M6 — Armas e combate  ·  esforço L

**Objectivo:** pistola na mão, mira, tiros e consequências.

**Tarefas**
- Pistola low-poly (modelo CC0 validado ou construído em código, estilo do carro) na mão do peão; animação de mira e recuo.
- Modo de mira (RMB no PC, botão no telemóvel): câmara aproxima, mira na tela, movimento mais lento.
- Tiro: raycast contra esferas (peões/polícia) e AABB (carros/paredes); tracer + flash de boca + som + faísca no impacto.
- Dano: peões caem (animação de queda simples), polícia também; matar sobe o procurado (+2) e chama reforços.
- Polícia passa a **responder a tiro** a partir de 3 estrelas (rajadas com intervalo, precisão baixa).
- Jogador pode morrer → ecrã "Hospital" → spawn na casa mais próxima (ou inicial), perde 10% do dinheiro.
- Munição e arma no HUD; compra na loja; teclas 1/2 para alternar (punhos/pistola).
- Botões móveis: mirar, disparar, trocar arma.

**Aceitação:** disparar contra um peão e contra um carro produz reacções distintas; 3 estrelas geram troca de tiros; morrer tem custo e respawn; 60 fps mantidos.

---

### Fase M7 — Conversas e interacção  ·  esforço M

**Objectivo:** "não se conversa" deixa de ser verdade.

**Tarefas**
- Prompt contextual: "E · Falar", "E · Comprar", "E · Entrar" (prioridade por distância e tipo).
- Caixa de diálogo (HTML): nome do NPC, texto com efeito de máquina de escrever, 2–3 opções, Enter/clique para avançar; sem pausa do mundo (GTA-like).
- NPCs com guiões em dados (`src/dialogue.h`): 3 personagens de missão, vendedor da mercearia, vendedor de armas, candongueiro, transeuntes com falas soltas ("Bué de calor, mano.", "Cuidado com a polícia.").
- Reacções: peões fogem ao ouvir tiros; polícia grita ordem a <6 m; vendedores recusam servir com 3+ estrelas.
- Telemóvel (J / botão): mapa, lista de missões, guardar, estatísticas.

**Aceitação:** falar com os 3 personagens de missão, comprar numa loja via diálogo, e ver peões a fugir depois de um tiro.

---

### Fase M8 — Missões  ·  esforço L

**Objectivo:** o jogo passa a ter objectivo.

**Estrutura:** `src/missions.h` com máquina de estados — `idle → briefing → active(objectivo N) → success/fail → reward`. Marcadores cilíndricos no mundo + blip no minimapa + texto de objectivo no HUD. Falha por tempo, morte, ou carro destruído. Retry imediato.

**Missões iniciais (5)**
1. **Candongueiro** (tutorial): levar um passageiro da Baixa ao musseque; ensina conduzir, mapa e dinheiro. Recompensa 1.500 Kz.
2. **Kixikila**: recolher 3 dívidas em pontos diferentes; uma delas foge de carro → perseguição. Recompensa 3.000 Kz.
3. **A encomenda**: buscar caixa no Zunga, entregar na Ilha, perder um carro que segue atrás. Recompensa 4.500 Kz.
4. **Corrida na Ilha**: 3 checkpoints, adversário com IA de trânsito agressiva, sem polícia. Recompensa 5.000 Kz.
5. **O negócio** (finale): escoltar um candongueiro com 2 estrelas de polícia activas, chegar à Cidade Alta. Recompensa 8.000 Kz + casa.

**Extras:** corrida de candongueiro repetível (rendimento), entregas aleatórias, estatísticas (missões, mortes, Kz ganho).

**Aceitação:** jogar a cadeia 1→5 sem falhas de estado, com falha/retry funcionais e recompensas creditadas; save a meio da cadeia.

---

### Fase M9 — Polimento, áudio e publicação  ·  esforço M

- **Rádio Luanda**: 3 faixas geradas por WebAudio (sequenciador simples: baixo, bateria, arpejo) com intro de locutor; tecla para mudar estação. Assinatura GTA.
- LOD: silhueta/impostor a >150 m para edifícios; culling já existente por distância.
- Ciclo dia/noite opcional (sol animado + faróis) — só se mantiver os 60 fps.
- Definições: escala de render, som, qualidade (auto por GPU).
- Mobile: revisão do HUD, botões de mira/tiro, performance em 390×844.
- **Publicação**: página estática, `LICENSE` do jogo, `CREDITS.md` dos assets CC0, verificação de que nenhum GLB antigo/não licenciado é servido.

---

## 4. Orçamento de performance (inviolável)

| Métrica | Limite |
|---|---|
| fps p50 (1280×720, HD 4000) | ≥ 55 |
| Draw calls | ≤ 300 (fusão por chunk obrigatória) |
| Payload total gz | ≤ 3 MB |
| Tris visíveis por frame | ≤ 250 k |
| Memória JS+WASM | ≤ 300 MB |

Cada fase termina com `node tools/bench.mjs`; se falhar, corta-se densidade de props ou modelos antes de avançar.

## 5. Riscos e decisões

| Risco | Mitigação |
|---|---|
| Modelos CC0 não cobrem armas/personagens | Pistola construída em código (como o carro); personagens mantêm peões procedurais até haver pack CC0 validado |
| Cores erradas (linear vs sRGB) | Conversão no conversor + screenshot de controlo |
| Escala/colisão | AABB por modelo calculado dos bounds reais, não estimado |
| Estilo "brasileiro" vs Luanda | Usar residencial/musseques, cores quentes, palmeiras e beira-rio; evitar bandeiras/logos |
| Draw calls ao usar 20 modelos de veículos | Instancing por modelo + limite de 10 modelos activos |
| Save corrompido após updates | Campo `versão` e migração/descarte |

## 6. Ordem imediata

1. M3.1 — descarregar kit, escrever `tools/glb2mw.mjs`, converter 3 modelos de teste (casa simples, loja, palmeira).
2. M3.2 — shader flat color + loader C++ + colocar os 3 modelos num bloco de teste; screenshot.
3. M3.3 — escolher os ~45 modelos e substituir a geração de edifícios por distrito.
4. M3.4 — veículos do kit + props + testes de performance e colisão.

---

# 7. ULTRA PROMPT (colar numa sessão nova)

```text
És o engenheiro principal do projecto MWANGOLÉ, um jogo estilo GTA ambientado em
Luanda, feito de raiz: C++20 compilado para WebAssembly (Emscripten 7.0.0 em
D:\emsdk) + WebGL2, com shell HTML/JS vanilla. Repositório:
C:\Users\Admin\Documents\Qoder\2026-11-17\59f0b988 (src/*.h, src/main.cpp,
web/*, tools/*.mjs). Servidor de desenvolvimento: python tools/serve.py na porta
8090. Build: python tools/build.py. Testes via Chrome headless CDP em
http://127.0.0.1:9222 (node tools/check.mjs, bench.mjs, street.mjs, police.mjs,
audio.mjs).

OBJECTIVO FINAL
Um jogo leve e fluido (60 fps a 720p num Intel HD 4000, 30+ fps em telemóvel
médio), em pt-AO, com cidade reconhecível de Luanda, carros e casas com modelos
3D reais (não caixas), dinheiro (Kz) que se ganha e gasta, casas à venda e
garagem, pistola com mira e combate, conversas com NPCs, missões com
marcadores e recompensas, polícia com 5 níveis de procurado, save local.

REGRAS DURAS (não negociáveis)
1. Código e assets originais. Proibido usar assets, código ou mapas de GTA San
   Andreas ou de qualquer GTA. Proibido tocar em C:\Users\Admin\games\Gta-Clone
   (é só leitura). Os GLB antigos desse projecto são não licenciados: nunca
   copiar, nunca servir.
2. Assets 3D apenas de fontes CC0 validadas. Fonte principal:
   kayquer/kit-cidade-lowpoly (CC0 1.0, 129 GLB, ≤1500 tris, cores planas).
   Manter assets/CREDITS.md. Validar licença ficheiro a ficheiro.
3. Zero parsing GLB em runtime. Conversão em tempo de build para binário próprio
   (.mwb) e preload Emscripten.
4. Orçamento: 60 fps p50 a 1280×720; ≤300 draw calls; ≤3 MB gz total; fusão de
   geometria estática por chunk; instancing só para dinâmicos.
5. Nada de SDL/GLFW/pthreads. Single-thread. Servidor estático simples.
6. Cada fase termina com build + testes automáticos + screenshots. Se os testes
   ou o bench falharem, corrigir antes de avançar.
7. Não pedir confirmação a cada passo dentro da fase; reportar no fim com
   números (fps, draw calls, KB, resultados dos testes) e screenshots.

ARQUITECTURA EXISTENTE (reaproveitar, não reescrever)
- math.h vectores/matrizes; gfx.h shaders, Mesh (Vertex: pos3+normal3+uv2+
  layer1+RGBA8), instancing, textura 2D array; world.h cidade por chunks com
  colisores AABB; car.h física arcade; player.h peões procedurais animados;
  street.h trânsito em faixas, peões de passeio, polícia; main.cpp loop, câmara,
  HUD, heat/wanted; shell.js input, WebAudio (motor, buzina, sirene, impacto),
  HUD, bench.
- Comunicação C++→JS por EM_JS (padrão mw_hud, mw_street_js, mw_traffic_js).
- UI em HTML/CSS sobre o canvas; input teclado+pointer; mobile com stick virtual.

FASES (executar por esta ordem; cada uma jogável)

M3 — ARTE REAL
- Descarregar o kit CC0 e seleccionar ~45 modelos: todas as casas/lojas/prédios,
  6 serviços públicos, 7 árvores, ~10 mobiliário urbano, 6 veículos.
- Criar tools/glb2mw.mjs (Node sem dependências): ler GLB (header, chunk JSON,
  chunk BIN), aplicar TRS dos nós, extrair triângulos e normais, converter
  baseColorFactor por material em cor por vértice (linear→sRGB), centrar XZ,
  assentar Y=0, emitir city.mwb (header MWB1 + tabela de modelos) e manifest.
- src/assets.h: carregar city.mwb via preload, criar Mesh, API models_find().
- gfx.h: layer sentinela -1 => cor por vértice; manter luz e fog.
- world.h: substituir fachadas procedimentais por modelos colocados em lotes de
  12–18 m, yaw 90°, fundidos no mesh do chunk; colisores a partir dos bounds.
  Props fundidos; árvores passam a modelos do kit.
- Veículos do kit para trânsito e estacionados (instanciados, ≤10 modelos).
  Carro do jogador mantém-se procedural nesta fase.
- Testes: tools/assets.mjs com contagens, tris, tamanho e screenshots de rua,
  quarteirão de perto e topo.
- ACEITAÇÃO: ≥8 silhuetas por distrito, nenhuma fachada só de caixa, 60 fps p50,
  draw calls +≤20%, payload ≤3 MB gz, colisões e testes check/police/street OK.

M4 — CIDADE COM LÓGICA
- 4 distritos de 4×4 blocos: Baixa (torres/negócios), Kilamba/Cidade Alta
  (prédios altos), Musseque (casas, ruas estreitas, bancas), Ilha (marginal,
  palmeiras, quiosques).
- POIs com ícones no minimapa: 3 casas à venda, garagem, mercearia,
  restaurante, loja de armas, 3 missões, praça de candongueiros.
- Portas: prompt E com prioridade por distância (porta vs carro vs NPC).
- Semáforos com ciclo de 12 s nos cruzamentos principais.
- ACEITAÇÃO: bairros reconhecíveis, POIs no mapa, E com prioridade correcta.

M5 — ECONOMIA E PROPRIEDADE
- g_cash real; HUD anima +/−.
- Receitas: missões 2.000–8.000 Kz; candongueiro 300–900; entregas.
- Despesas: casas 25.000/45.000/90.000 (com garagem e spawn), pistola 6.000,
  munições 400, saúde 800.
- Menu de compra no letreiro À VENDA; casa comprada = safehouse (cura + guardar).
- Garagem guardar/retirar carro. Save localStorage com versão.
- ACEITAÇÃO: ganhar, comprar casa, guardar, recarregar e manter estado.

M6 — ARMAS E COMBATE
- Pistola low-poly na mão (CC0 validado ou construída em código).
- Mira (RMB / botão), raycast (esferas peões/polícia, AABB carros/paredes),
  tracer, flash, som, faísca. Dano e queda de NPCs.
- Matar sobe procurado (+2); polícia responde a tiro a partir de 3 estrelas.
- Morte do jogador: hospital, respawn em casa, perde 10% do dinheiro.
- HUD munição; compra na loja; teclas 1/2; botões móveis.
- ACEITAÇÃO: reacções distintas peão/carro, troca de tiros a 3 estrelas, custo
  de morte, 60 fps.

M7 — CONVERSAS E INTERACÇÃO
- Prompt contextual E (Falar/Comprar/Entrar).
- Caixa de diálogo HTML com máquina de escrever, 2–3 opções, sem pausar o mundo.
- Guiões em dados: 3 personagens de missão, vendedores, candongueiro, falas
  soltas de transeuntes (pt-AO).
- Peões fogem de tiros; polícia grita ordem perto; vendedores recusam a 3+.
- Telemóvel (J): mapa, missões, guardar, estatísticas.
- ACEITAÇÃO: diálogos completos com os 3 personagens, compra por diálogo,
  fuga de peões após tiro.

M8 — MISSÕES
- missions.h com estados idle/briefing/active/success/fail, marcador no mundo,
  blip no minimapa, objectivo no HUD, retry.
- 5 missões: Candongueiro (tutorial, 1.500), Kixikila (cobranças + perseguição,
  3.000), A encomenda (entrega + escapar, 4.500), Corrida na Ilha (3
  checkpoints, 5.000), O negócio (escolta final, 8.000 + casa).
- Rendimento repetível de candongueiro; estatísticas.
- ACEITAÇÃO: cadeia 1→5 completa, falha/retry, recompensas, save a meio.

M9 — POLIMENTO
- Rádio Luanda (3 faixas WebAudio), LOD a >150 m, definições de qualidade,
  revisão mobile, créditos/licenças, publicação estática sem assets ilegais.

MÉTODO DE TRABALHO
- Antes de codificar: ler os ficheiros relevantes e confirmar o estado actual.
- Implementar em passos pequenos; compilar (python tools/build.py) após cada
  passo significativo.
- Validar no browser real via CDP; tirar screenshots em shots/ e inspeccioná-las.
- Manter os testes existentes verdes (check, street, police, audio, bench) e
  acrescentar um teste por sistema novo.
- No fim de cada fase: relatório curto com fps, draw calls, KB, testes e
  screenshots; corrigir regressões antes da fase seguinte.
```

---

## 8. Resumo de uma linha

O motor já corre a 60 fps; o que falta é **arte real CC0 (M3)**, **cidade com destinos (M4)**, **dinheiro e casas à venda (M5)**, **pistola (M6)**, **conversas (M7)** e **5 missões (M8)** — este documento define o como, o quando e os limites de performance.
