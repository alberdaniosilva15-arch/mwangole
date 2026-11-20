# PLANO DE EXECUÇÃO — MWANGOLÉ (M5→M9)

Documento de trabalho para o modelo executor (DeepSeek). O Qoder actua como
**inspector**: não escreve código de fase, valida cada entrega contra os
critérios de aceitação abaixo. Ler isto inteiro antes de tocar em qualquer ficheiro.

## 0. Regras inegociáveis

1. `C:\Users\Admin\games\Gta-Clone` é **READ-ONLY**. Nunca copiar, servir ou
   referenciar assets de lá. Os GLBs antigos desse projecto não têm licença.
2. Nada de assets, mapas ou código decompilado de GTA San Andreas. Nada de
   marcas reais. Só CC0 / CC-BY (kit `assets-src/kit-cidade-lowpoly`) e os
   modelos do utilizador em `assets-src/meus-modelos` (já convertidos).
3. Toolchain: `D:\emsdk`. Build: `python tools/build.py`. Servidor:
   `python tools/serve.py` (porta **8090**, já envia `Cache-Control: no-store`).
4. Payload total gz ≤ **3 MB** (hoje ≈2.3 MB). FPS alvo ≥ **30** em Intel HD 4000
   (headless GPU de referência: ≥25; ver `tools/fps.mjs`).
5. Texto de UI em **pt-AO**. Sem emojis. Sem dependências novas de npm.
6. Cada fase = 1 commit + push para `main`
   (https://github.com/alberdaniosilva15-arch/mwangole). Mensagem no estilo
   `M5: economia e save local (kwanzas, garagem, progresso)`.
7. Não avançar para a fase seguinte sem o inspector dar OK na anterior.
8. Não redesenhar o que já funciona: HUD, condução, tráfego, polícia, ciclo
   dia/noite, atlas de texturas. Só estender.

## 1. Como construir e testar (comandos exactos)

```bash
python tools/build.py                  # emcc + preload city.mwb/citytex.bin
node tools/glb2mw.mjs                  # só se mexer em models.json/conversor
python tools/serve.py                  # porto 8090 (deixar a correr)
node tools/check.mjs                   # boot+condução+tráfego+polícia+fps
node tools/police.mjs                  # perseguição: heat sobe, cops spawn
node tools/fps.mjs                     # fps sustentado 12 s
node tools/timeview.mjs X Z YAW NOME TOD   # screenshot (TOD 0..1, 0.5=meio-dia)
node tools/view.mjs X Z YAW NOME top     # vista aérea
```

Chrome headless de teste (GPU real, NÃO swiftshader):

```
--headless=new --remote-debugging-port=9222 --user-data-dir=C:/Users/Admin/AppData/Local/Temp/mwchrome
--no-first-run --enable-gpu --ignore-gpu-blocklist --disable-background-timer-throttling
--disable-renderer-backgrounding --disable-backgrounding-occluded-windows --window-size=1280,720
```

`check.mjs` deve devolver `ready:true`, carro a andar (`speed>10`),
`street.cars≥15`, `street.peds≥20`, e `errors` apenas com avisos de
AudioContext autoplay (artefacto headless conhecido, não é regressão).

## 2. Estado actual (percentagens)

| Fase | Conteúdo | % |
|---|---|---|
| M1 | motor WASM/WebGL2, mundo em chunks, condução | 100 |
| M2 | tráfego, peões, semáforos, colisões | 100 |
| M3 | polícia/heat, ciclo dia-noite, HUD GSAP | 100 |
| M4 | bairros (Baixa/Alta/Musseque), POIs, modelos do utilizador com texturas reais | 100 |
| M5 | economia + save | 20 |
| M6 | combate | 0 |
| M7 | diálogo | 0 |
| M8 | cadeia de missões | 15 (estafeta existe) |
| M9 | áudio, polish, performance final | 50 |

## 3. Dívida visual herdada (fazer ANTES de M5, commit `V1`)

Sintoma: algumas faces de torres na Baixa ainda lêem como lajes lisas
(ex.: muro em ≈(250,190), visto de (250,210) yaw 0). Já corrigido nesta sessão:
texturas reais (camadas ≥15 do atlas), bitmaps lisos (std<18) substituídos por
fachada procedural com janelas, kits CC0 com skin de fachada e UV pré-multiplicada
pela escala de colocação (`kitUvScale`), tint ignorado em modelos texturados,
exposição do meio-dia baixada.

O muro restante **não mudou entre rebuilds do conversor** → não vem do
`city.mwb`. Diagnóstico obrigatório, por ordem:

1. Confirmar que `serve.py` envia `no-store` também para `.data`/`.wasm`
   (cache do disco do Chrome pode estar a servir `mw.data` velho).
2. Exportar `mw_debug_lot(x,z)` em `src/main.cpp` que devolve o nome do modelo
   colocado no lote que contém o ponto (reproduzir `lotSeed`/`pick_lot_model`
   de `src/world.h:281` e `place_lot` `src/world.h:744`). Chamá-lo em (250,190).
3. Se for procedural: `pick_style` (`src/world.h:559`) dá `L_GLASS` a ~28% dos
   edifícios da Baixa — vidro sem reflexo lê-se liso. Trocar para fachada com
   janelas ou dar ao `L_GLASS` um padrão de caixilharia no atlas (`src/gfx.h`).
4. Se for modelo: ver histograma de camadas no `city.mwb` (float no offset +32
   de cada vértice de 40 bytes; header de 56 bytes/modelo a partir do offset 8).

Aceitação V1: screenshot `timeview 250 210 0 v1fix 0.5` mostra janelas/caixilharia
nesse muro; `check.mjs` verde; fps ≥25 headless; payload gz ≤3 MB.

## 4. M5 — Economia e save (commit único)

- `src/econ.h` novo: carteira em kwanzas (já existe HUD `Kz`), preços de
  combustível, renda de garagem, multas da polícia debitadas automaticamente.
- Postos (`comercial/posto_recarga`, POI kind existente): parar junto + `F`
  abastece e debita; sem dinheiro → aviso pt-AO no HUD, sem bloquear jogo.
- Save/localStorage em `web/index.html` + ponte C++ (`mw_save`/`mw_load`
  exportados): dinheiro, hora do dia, posição, carro actual, progresso de
  missões, heat nunca save (sempre 0 ao carregar).
- Garagem: comprar 1 de 3 carros com `Kz`; troca de carro reflecte no spawn.
- Aceitação: `check.mjs` verde; teste manual via CDP: ganhar Kz numa estafeta,
  reload da página → dinheiro/posição persistem; comprar carro → HUD e modelo
  mudam; sem dinheiro → mensagem e nada crasha. fps ≥25.

## 5. M6 — Combate leve (commit único)

- Só corpo-a-corpo e arma curta opcional; **sem** sangue explícito, sem mira
  livre tipo FPS. Alvo: peões hostis só quando heat ≥3, cops já existentes.
- `src/combat.h`: golpe (tecla `G`), cooldown, knockback, dano em cops;
  cops revidem com dano em health (HUD já tem barra).
- Morte → respawn no hospital mais próximo (`publicos/hospital`), perde 10% Kz.
- Aceitação: `police.mjs` continua verde; novo `tools/combat.mjs`: heat 3 →
  golpe derruba cop (posição/estado muda), health desce quando cop bate,
  morte → respawn hospital + débito. fps ≥25.

## 6. M7 — Diálogo (commit único)

- `src/dialog.h` + overlay DOM em `web/index.html` (caixa pt-AO, nome do NPC,
  2–3 opções). NPCs: chefe das estafetas, mecânico da garagem, polícia (suborno).
- Tecla `E` perto de NPC marcado no minimapa abre diálogo; opções disparam
  acções reais (iniciar trabalho, comprar carro, baixar heat por Kz).
- Sem voice-over. Texto curto, pt-AO, sem emojis.
- Aceitação: `tools/dialog.mjs`: abrir, escolher opção 2 → efeito observável
  (Kz ou heat ou job activo); fechar com Esc; andar com diálogo aberto bloqueado.

## 7. M8 — Cadeia de missões (commit único)

- 6 missões encadeadas além da estafeta: entrega urgente (tempo), perseguição
  a carro ladrão (IA de fuga simples), rota de táxi (3 paragens), carga no
  trapiche (`beira_rio/trapiche`), volta à cidade (checkpoint), final: entrega
  na Alta com heat 2 activo.
- `src/missions.h`: tabela data-driven (id, tipo, alvo, recompensa, próxima).
  Beacon no minimapa + seta no HUD (já existe base da estafeta).
- Recompensas crescentes 500→5000 Kz; falhar (tempo/destroy) → retry sem softlock.
- Aceitação: `tools/mission.mjs` percorre as 6 por debug-place até aos alvos;
  cada uma paga e desbloqueia a seguinte; falhar uma → estado retry, não crash.

## 8. M9 — Áudio e polish final (commit único)

- `web/audio.js` já tem base: motor (pitch por speed), buzina, sirene por heat,
  ambiente por bairro (3 loops curtos CC0 gerados/procedurais, ≤200 KB total).
- Botão de mute visível no topo (tecla `M` já existe) com estado claro.
- Polimento: sombras baked já existem; adicionar only fog por distância e
  vignette ligeira no shader se fps ≥30 headless; caso contrário cortar.
- Aceitação: payload gz ≤3 MB; fps ≥30 headless; `check.mjs`+`police.mjs`+
  `mission.mjs` verdes; mute corta tudo num clique; reload sem leaks
  (AudioContext único, sem loops duplicados).

## 9. Protocolo do inspector (Qoder)

Para cada entrega o inspector corre, sem excepção:
1. `git log -1` + `git diff HEAD~1 --stat` (âmbito bate com a fase? nada fora dela?)
2. `python tools/build.py` limpo, sem warnings novos.
3. `check.mjs`, `police.mjs`, `fps.mjs` + o harness novo da fase.
4. Screenshots spawn/aérea/noite (`timeview … 0.5 / 0.05`) comparados com `shots/v9_*`.
5. Payload: `gzip -c web/build/mw.data | wc -c` + wasm/js ≤3 MB.
6. Procura por violações: `grep -ri "gta-clone\|san andreas\|rockstar" src web tools`.
Falhou um → devolve com lista exacta de defeitos; não avança de fase.