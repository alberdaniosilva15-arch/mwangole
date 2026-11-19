# Mwangolé

Jogo de mundo aberto ao estilo clássico, ambientado em **Luanda**. Motor próprio em
**C++20 → WebAssembly + WebGL2**, sem frameworks de jogo e sem conteúdo de terceiros
não licenciado. Corre no navegador.

![estado](https://img.shields.io/badge/estado-jogável-2ea043) ![stack](https://img.shields.io/badge/C%2B%2B20-WebAssembly%20%2B%20WebGL2-1e7fd8)

## Jogar

Requisitos: **Python 3** e um navegador com WebGL2.

```
JOGAR.cmd            # Windows: arranca o servidor e abre o jogo
```

ou manualmente:

```
python tools/serve.py
# http://127.0.0.1:8090/
```

A versão compilada (`web/build/mw.js` + `mw.wasm`) está incluída no repositório,
por isso não é preciso compilar para jogar.

## Estado do desenvolvimento

Tabela completa por fase em [`PLANO.md`](PLANO.md) (secção 0-A). Resumo:
M0–M4 feitos (cidade com distritos, POIs e semáforos) · M5 economia/save 20% ·
M6 combate 0% · M7 conversas 0% · M8 missões 15% (só loop de estafeta) · M9 polimento 50%.

Próximo por impacto: M5 → M8 → M6 → M7 → M9.

## Comandos

| Tecla | Ação |
|---|---|
| `W A S D` / setas | Conduzir / andar |
| `Shift` | Correr |
| `Espaço` | Travão de mão |
| `E` | Entrar / sair do carro, entrar num POI (a pé) |
| `F` | Iniciar trabalho de estafeta |
| `T` | Adiantar 3 horas (ciclo dia/noite) |
| `C` | Alternar câmara |
| `R` | Reiniciar na posição inicial |
| `H` / `M` | Buzina / som |
| Rato | Olhar em volta |

**Estafeta**: prime `F` para aceitar um trabalho (ou `E` na Garagem do Musseque),
segue o marcador azul no mínimo e no mundo (coluna luminosa), recolhe a encomenda e
entrega-a no marcador laranja antes do tempo acabar. A paga depende da distância.

**Cidade**: quatro zonas com arquitectura própria — Baixa (torres do centro),
Cidade Alta/Kilamba (a leste), Musseque (casas baixas a oeste e a norte) e Ilha do
Cabo (areal com quiosques, ligada por uma ponte na Marginal). O distrito actual
aparece no HUD. No minimapa há ícones com legenda para casas à venda, mercearia,
restaurante, garagem, loja de armas e contactos de missão; chega-te ao poste com a
placa luminosa e prime `E`.

## Compilar

Requisitos: [Emscripten SDK](https://emscripten.org/) em `D:\emsdk` (ou defina `EMSDK`).

```
python tools/build.py
```

Regenerar os modelos convertidos (só quando `assets-src/` muda):

```
node tools/glb2mw.mjs
python tools/build.py     # re-embebe web/assets/city.mwb no wasm
```

## Estrutura

```
src/            motor C++ (main, gfx, world, car, player, street, math, assets)
web/            shell HTML/JS (HUD, áudio, input) + build wasm + assets
tools/          build, serve, conversor GLB→MWB e utilitários de teste
assets-src/     modelos 3D de origem (CC BY 4.0, ver CREDITS.md)
```

## Créditos

- Modelos 3D externos: ver [`assets-src/meus-modelos/CREDITS.md`](assets-src/meus-modelos/CREDITS.md)
  (todos CC BY 4.0, com autores e ligações) e o kit
  [kayquer/kit-cidade-lowpoly](https://github.com/kayquer/kit-cidade-lowpoly) (CC0).
- Texturas do chão, fachadas, carros e personagens são geradas proceduralmente pelo motor.
- Nenhum ficheiro de GTA / San Andreas é usado neste projeto.
