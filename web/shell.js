const canvas = document.getElementById('game');
const bootMsg = document.getElementById('boot-msg');

let Module = null;
let activePointer = null;
let lastX = 0, lastY = 0;

function fail(text) {
  document.body.dataset.mw = 'error';
  if (bootMsg) {
    bootMsg.textContent = text;
    bootMsg.classList.add('err');
  }
  const sub = document.getElementById('boot-sub');
  if (sub) sub.textContent = 'O navegador não conseguiu iniciar o WebGL2.';
}

window.addEventListener('error', (e) => fail('Erro: ' + (e.message || 'desconhecido')));
window.addEventListener('unhandledrejection', (e) => fail('Erro: ' + (e.reason && e.reason.message ? e.reason.message : e.reason)));

function sizeCanvas() {
  const dpr = Math.min(2, window.devicePixelRatio || 1);
  const w = Math.max(320, Math.round((canvas.clientWidth || window.innerWidth) * dpr));
  const h = Math.max(180, Math.round((canvas.clientHeight || window.innerHeight) * dpr));
  if (canvas.width !== w || canvas.height !== h) {
    canvas.width = w;
    canvas.height = h;
  }
  if (Module && Module._mw_resize) Module._mw_resize(canvas.width, canvas.height);
}
window.addEventListener('resize', sizeCanvas);
window.addEventListener('orientationchange', () => setTimeout(sizeCanvas, 250));

// ------------------------------------------------------------------ audio

let actx = null;
let engGain = null, engOsc1 = null, engOsc2 = null, engFilter = null, windGain = null;
let sirenOsc = null, sirenGain = null, sirenOn = false, sirenTone = 0;
let masterGain = null, impactBuf = null;
let muted = false;
let hornCount = 0, impactCount = 0, lastImpact = 0;

function audioEnsure() {
  if (actx) {
    if (actx.state === 'suspended') actx.resume();
    return;
  }
  const AC = window.AudioContext || window.webkitAudioContext;
  if (!AC) return;
  try {
    actx = new AC();
    masterGain = actx.createGain();
    masterGain.gain.value = 0.66;
    masterGain.connect(actx.destination);
    const master = masterGain;

    engFilter = actx.createBiquadFilter();
    engFilter.type = 'lowpass';
    engFilter.frequency.value = 340;
    engFilter.Q.value = 0.6;

    engGain = actx.createGain();
    engGain.gain.value = 0.0;
    engFilter.connect(engGain);
    engGain.connect(master);

    engOsc1 = actx.createOscillator();
    engOsc1.type = 'sawtooth';
    engOsc1.frequency.value = 46;
    const g1 = actx.createGain();
    g1.gain.value = 0.35;
    engOsc1.connect(g1);
    g1.connect(engFilter);
    engOsc1.start();

    engOsc2 = actx.createOscillator();
    engOsc2.type = 'sine';
    engOsc2.frequency.value = 23;
    const g2 = actx.createGain();
    g2.gain.value = 0.9;
    engOsc2.connect(g2);
    g2.connect(engFilter);
    engOsc2.start();

    const len = Math.floor(actx.sampleRate * 2);
    const buf = actx.createBuffer(1, len, actx.sampleRate);
    const data = buf.getChannelData(0);
    let last = 0;
    for (let i = 0; i < len; ++i) {
      const w = Math.random() * 2 - 1;
      last = last * 0.965 + w * 0.035;
      data[i] = last * 5;
    }
    const src = actx.createBufferSource();
    src.buffer = buf;
    src.loop = true;
    const bp = actx.createBiquadFilter();
    bp.type = 'bandpass';
    bp.frequency.value = 420;
    bp.Q.value = 0.5;
    windGain = actx.createGain();
    windGain.gain.value = 0.004;
    src.connect(bp);
    bp.connect(windGain);
    windGain.connect(master);
    src.start();

    sirenOsc = actx.createOscillator();
    sirenOsc.type = 'square';
    sirenOsc.frequency.value = 700;
    const sbp = actx.createBiquadFilter();
    sbp.type = 'bandpass';
    sbp.frequency.value = 780;
    sbp.Q.value = 1.4;
    sirenGain = actx.createGain();
    sirenGain.gain.value = 0.0;
    sirenOsc.connect(sbp);
    sbp.connect(sirenGain);
    sirenGain.connect(master);
    sirenOsc.start();
    setInterval(() => {
      if (!sirenOsc || !sirenOn) return;
      sirenTone ^= 1;
      sirenOsc.frequency.setTargetAtTime(sirenTone ? 900 : 640, actx.currentTime, 0.04);
    }, 520);
  } catch (e) {
    console.warn('audio', e);
  }
}

function toggleMute() {
  muted = !muted;
  if (engGain) engGain.gain.value = 0;
  if (windGain) windGain.gain.value = 0;
  if (sirenGain) sirenGain.gain.value = 0;
  if (!muted) audioEnsure();
}

window.__mwSiren = (on) => {
  sirenOn = !!on;
  if (on && !actx) audioEnsure();
  if (sirenGain && actx) {
    sirenGain.gain.setTargetAtTime(on && !muted ? 0.045 : 0, actx.currentTime, 0.15);
  }
};

function playHorn() {
  if (!actx) audioEnsure();
  if (!actx || muted || !masterGain) return;
  ++hornCount;
  const t = actx.currentTime;
  const g = actx.createGain();
  g.gain.setValueAtTime(0.0001, t);
  g.gain.exponentialRampToValueAtTime(0.085, t + 0.025);
  g.gain.setValueAtTime(0.085, t + 0.3);
  g.gain.exponentialRampToValueAtTime(0.0001, t + 0.5);
  const lp = actx.createBiquadFilter();
  lp.type = 'lowpass';
  lp.frequency.value = 1700;
  for (const hz of [370, 466]) {
    const o = actx.createOscillator();
    o.type = 'sawtooth';
    o.frequency.value = hz;
    o.connect(lp);
    o.start(t);
    o.stop(t + 0.55);
  }
  lp.connect(g);
  g.connect(masterGain);
}

window.__mwImpact = (s) => {
  if (!actx || muted || !masterGain) return;
  ++impactCount; lastImpact = s;
  const t = actx.currentTime;
  if (!impactBuf) {
    const len = Math.floor(actx.sampleRate * 0.4);
    impactBuf = actx.createBuffer(1, len, actx.sampleRate);
    const d = impactBuf.getChannelData(0);
    for (let i = 0; i < len; ++i) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, 2.5);
  }
  const src = actx.createBufferSource();
  src.buffer = impactBuf;
  const lp = actx.createBiquadFilter();
  lp.type = 'lowpass';
  lp.frequency.value = 280;
  const g = actx.createGain();
  g.gain.value = 0.24 * Math.min(1, Math.max(0.15, s));
  src.connect(lp);
  lp.connect(g);
  g.connect(masterGain);
  src.start(t);
};

window.__mwAudioDebug = () => ({
  state: actx ? actx.state : 'none',
  engine: engGain ? +engGain.gain.value.toFixed(4) : null,
  engineHz: engOsc1 ? +engOsc1.frequency.value.toFixed(1) : null,
  wind: windGain ? +windGain.gain.value.toFixed(4) : null,
  siren: sirenGain ? +sirenGain.gain.value.toFixed(4) : null,
  sirenOn,
  muted,
  hornCount,
  impactCount,
  lastImpact: +lastImpact.toFixed(2),
});

window.__mwAudio = (kmh, inCar) => {
  if (!actx || !engGain) return;
  const v = Math.min(1, Math.max(0, kmh) / 140);
  const f = 42 + v * 122;
  const t = actx.currentTime;
  engOsc1.frequency.setTargetAtTime(f, t, 0.07);
  engOsc2.frequency.setTargetAtTime(f * 0.5, t, 0.07);
  engFilter.frequency.setTargetAtTime(310 + v * 980, t, 0.12);
  const engineOn = inCar && !muted;
  engGain.gain.setTargetAtTime(engineOn ? 0.018 + v * 0.030 : 0, t, 0.16);
  if (windGain) windGain.gain.setTargetAtTime(muted ? 0 : 0.0025 + v * 0.014, t, 0.2);
};

// ------------------------------------------------------------------ input

const KEYMAP = {
  KeyW: 0, ArrowUp: 0,
  KeyS: 1, ArrowDown: 1,
  KeyA: 2, ArrowLeft: 2,
  KeyD: 3, ArrowRight: 3,
  Space: 4,
};

function action(code, value) {
  if (Module && Module._mw_action) Module._mw_action(code, value);
}
function pulse(code) {
  action(code, 1);
  setTimeout(() => action(code, 0), 30);
}

window.addEventListener('keydown', (e) => {
  if (e.repeat) return;
  audioEnsure();
  if (e.code === 'KeyE') { pulse(10); return; }
  if (e.code === 'KeyC') { pulse(11); return; }
  if (e.code === 'KeyR') { pulse(12); return; }
  if (e.code === 'KeyF') { pulse(15); return; }
  if (e.code === 'KeyT') { pulse(14); return; }
  if (e.code === 'KeyM') { toggleMute(); return; }
  if (e.code === 'KeyH') { playHorn(); return; }
  if (e.code === 'ShiftLeft' || e.code === 'ShiftRight') { action(13, 1); return; }
  const code = KEYMAP[e.code];
  if (code !== undefined) {
    action(code, 1);
    e.preventDefault();
  }
});

window.addEventListener('keyup', (e) => {
  if (e.code === 'ShiftLeft' || e.code === 'ShiftRight') { action(13, 0); return; }
  const code = KEYMAP[e.code];
  if (code !== undefined) {
    action(code, 0);
    e.preventDefault();
  }
});

window.addEventListener('blur', () => {
  for (let c = 0; c <= 4; ++c) action(c, 0);
  action(13, 0);
  if (Module && Module._mw_axis) Module._mw_axis(0, 0);
});

window.addEventListener('contextmenu', (e) => e.preventDefault());

// camera drag
canvas.addEventListener('pointerdown', (e) => {
  audioEnsure();
  if (activePointer !== null) return;
  activePointer = e.pointerId;
  lastX = e.clientX;
  lastY = e.clientY;
  canvas.setPointerCapture(e.pointerId);
});

canvas.addEventListener('pointermove', (e) => {
  if (e.pointerId !== activePointer) return;
  const dx = e.clientX - lastX;
  const dy = e.clientY - lastY;
  lastX = e.clientX;
  lastY = e.clientY;
  if (Module && Module._mw_look) Module._mw_look(dx, dy);
});

function endDrag(e) {
  if (e.pointerId !== activePointer) return;
  activePointer = null;
}
canvas.addEventListener('pointerup', endDrag);
canvas.addEventListener('pointercancel', endDrag);

// virtual stick
const stick = document.getElementById('stick');
const knob = document.getElementById('knob');
let stickId = null;

function updateStick(e) {
  const r = stick.getBoundingClientRect();
  let dx = (e.clientX - (r.left + r.width / 2)) / (r.width / 2);
  let dy = (e.clientY - (r.top + r.height / 2)) / (r.height / 2);
  const len = Math.hypot(dx, dy);
  if (len > 1) { dx /= len; dy /= len; }
  if (len < 0.14) { dx = 0; dy = 0; }
  if (Module && Module._mw_axis) Module._mw_axis(dx, -dy);
  knob.style.transform = `translate(${dx * r.width * 0.27}px, ${dy * r.height * 0.27}px)`;
}

if (stick) {
  stick.addEventListener('pointerdown', (e) => {
    audioEnsure();
    stickId = e.pointerId;
    stick.setPointerCapture(e.pointerId);
    updateStick(e);
    e.preventDefault();
  });
  stick.addEventListener('pointermove', (e) => {
    if (e.pointerId === stickId) updateStick(e);
  });
  const release = (e) => {
    if (e.pointerId !== stickId) return;
    stickId = null;
    if (Module && Module._mw_axis) Module._mw_axis(0, 0);
    knob.style.transform = 'translate(0, 0)';
  };
  stick.addEventListener('pointerup', release);
  stick.addEventListener('pointercancel', release);
}

function bindButton(id, code, isPulse) {
  const el = document.getElementById(id);
  if (!el) return;
  el.addEventListener('pointerdown', (e) => {
    audioEnsure();
    el.classList.add('on');
    if (isPulse) pulse(code);
    else action(code, 1);
    el.setPointerCapture(e.pointerId);
    e.preventDefault();
  });
  const off = (e) => {
    el.classList.remove('on');
    if (!isPulse) action(code, 0);
  };
  el.addEventListener('pointerup', off);
  el.addEventListener('pointercancel', off);
  el.addEventListener('pointerleave', off);
}

bindButton('btnGas', 0, false);
bindButton('btnBrake', 1, false);
bindButton('btnHand', 4, false);
bindButton('btnAct', 10, true);

setInterval(() => {
  const el = document.getElementById('btnAct');
  if (el) el.textContent = window.__mwInCar ? 'SAIR' : 'ENTRAR';
}, 400);

// ------------------------------------------------------------------ hud (gsap)

const G = window.gsap || null;
const hud = { cash: -1, health: 100, wanted: -1, band: -1, prompt: null, intro: false, pulse: null, flash: 0 };
const cashObj = { v: 0 };

function fmtKz(n) {
  const str = String(Math.round(n));
  let out = '';
  for (let i = 0; i < str.length; ++i) {
    if (i > 0 && (str.length - i) % 3 === 0) out += '.';
    out += str[i];
  }
  return out;
}

function hudIntro() {
  if (!G || hud.intro) return;
  hud.intro = true;
  G.timeline({ defaults: { ease: 'power3.out' } })
    .fromTo('#topleft', { x: -28, autoAlpha: 0 }, { x: 0, autoAlpha: 1, duration: 0.55 }, 0)
    .fromTo('#topright', { x: 28, autoAlpha: 0 }, { x: 0, autoAlpha: 1, duration: 0.55 }, 0.07)
    .fromTo('#mapwrap', { x: -18, scale: 0.9, autoAlpha: 0 }, { x: 0, scale: 1, autoAlpha: 1, duration: 0.6 }, 0.14)
    .fromTo('#health', { x: 24, autoAlpha: 0 }, { x: 0, autoAlpha: 1, duration: 0.5 }, 0.2)
    .fromTo('#speedo', { y: 22, scale: 0.9, autoAlpha: 0 }, { y: 0, scale: 1, autoAlpha: 1, duration: 0.55 }, 0.22)
    .fromTo('#hint', { autoAlpha: 0 }, { autoAlpha: 1, duration: 0.7 }, 0.5)
    .fromTo('#touch', { autoAlpha: 0 }, { autoAlpha: 1, duration: 0.55 }, 0.4);
}

function hudCash(target) {
  target = Math.round(target);
  if (target === hud.cash) return;
  const delta = hud.cash < 0 ? 0 : target - hud.cash;
  hud.cash = target;
  const el = document.getElementById('cash');
  if (!G || !el) {
    if (el) el.textContent = 'Kz ' + fmtKz(target);
    cashObj.v = target;
    return;
  }
  G.killTweensOf(cashObj);
  G.to(cashObj, {
    v: target,
    duration: Math.max(0.35, Math.min(1.3, Math.abs(cashObj.v - target) / 6000 + 0.35)),
    ease: 'power2.out',
    onUpdate: () => { el.textContent = 'Kz ' + fmtKz(cashObj.v); },
  });
  if (delta > 0) {
    G.killTweensOf(el);
    G.fromTo(el, { scale: 1 }, { scale: 1.13, duration: 0.16, yoyo: true, repeat: 1, ease: 'power2.out' });
    const pop = document.getElementById('cash-pop');
    if (pop) {
      pop.textContent = '+' + fmtKz(delta);
      G.killTweensOf(pop);
      G.fromTo(pop, { y: 8, autoAlpha: 0, scale: 0.9 },
        { y: -16, autoAlpha: 1, scale: 1, duration: 0.28, ease: 'power2.out',
          onComplete: () => G.to(pop, { autoAlpha: 0, duration: 0.7, delay: 0.7 }) });
    }
  }
  if (delta < 0) {
    G.fromTo(el, { color: '#ff8a7a' }, { color: '#a8e6a1', duration: 0.7, ease: 'power2.out' });
  }
}

function hudHealth(h) {
  h = Math.max(0, Math.min(100, h));
  const fill = document.getElementById('health-fill');
  if (fill && Math.abs(h - hud.health) > 0.05) fill.style.width = h + '%';
  const now = performance.now();
  if (h < hud.health - 0.4 && now - hud.flash > 320) {
    hud.flash = now;
    const dmg = document.getElementById('damage');
    if (G && dmg) {
      G.killTweensOf(dmg);
      G.fromTo(dmg, { autoAlpha: Math.min(0.9, 0.35 + (hud.health - h) / 40) },
        { autoAlpha: 0, duration: 0.55, ease: 'power2.out' });
    }
  }
  hud.health = h;
}

function hudWanted(n) {
  n = Math.max(0, Math.min(5, n | 0));
  if (n === hud.wanted) return;
  const wl = document.getElementById('wanted');
  if (!wl) return;
  const stars = wl.children;
  for (let i = 0; i < 5 && i < stars.length; ++i) {
    const on = i < n;
    const el = stars[i];
    if ((el.textContent === '★') === on) continue;
    el.textContent = on ? '★' : '☆';
    if (G) {
      if (on) G.fromTo(el, { scale: 1.8, color: '#fff6cf' }, { scale: 1, color: '#ffd15c', duration: 0.55, ease: 'elastic.out(1, 0.45)' });
      else G.fromTo(el, { scale: 0.6, color: '#6d6047' }, { scale: 1, color: '#ffd15c', duration: 0.3, ease: 'power2.out' });
    }
  }
  hud.wanted = n;
  if (!G) return;
  if (n >= 4) {
    if (!hud.pulse) hud.pulse = G.to(wl, { color: '#ff5040', duration: 0.55, repeat: -1, yoyo: true, ease: 'sine.inOut' });
  } else if (hud.pulse) {
    hud.pulse.kill();
    hud.pulse = null;
    G.to(wl, { color: '#ffd15c', duration: 0.25, overwrite: true });
  }
}

function hudPrompt(text) {
  if (text === hud.prompt) return;
  hud.prompt = text;
  const pr = document.getElementById('prompt');
  if (!pr) return;
  if (!G) { pr.textContent = text; return; }
  G.killTweensOf(pr);
  if (text) {
    pr.textContent = text;
    G.fromTo(pr, { autoAlpha: 0, y: 12 }, { autoAlpha: 1, y: 0, duration: 0.28, ease: 'power3.out' });
  } else {
    G.to(pr, { autoAlpha: 0, y: 8, duration: 0.16, ease: 'power2.in',
      onComplete: () => { if (!hud.prompt) pr.textContent = ''; } });
  }
}

function hudSpeed(kmh, inCar) {
  const band = Math.floor(Math.max(0, kmh) / 20);
  if (inCar && hud.band >= 0 && band > hud.band && band > 0) {
    const el = document.getElementById('kmh');
    if (G && el) G.fromTo(el, { scale: 1 }, { scale: 1.1, duration: 0.11, yoyo: true, repeat: 1, ease: 'power2.out' });
  }
  hud.band = inCar ? band : 0;
}

window.__mwHudAnim = (kmh, health, cash, wanted, inCar, prompt) => {
  hudCash(cash);
  hudHealth(health);
  hudWanted(wanted);
  hudPrompt(prompt || '');
  hudSpeed(kmh, inCar);
};

// ------------------------------------------------------------------ bench

async function runBench() {
  const start = performance.now();
  while (!window.__mwReady && performance.now() - start < 25000) {
    await new Promise((r) => setTimeout(r, 120));
  }
  await new Promise((r) => setTimeout(r, 1800));

  const deltas = [];
  let last = performance.now();
  await new Promise((resolve) => {
    let n = 0;
    function tick(t) {
      deltas.push(t - last);
      last = t;
      if (++n < 240) requestAnimationFrame(tick);
      else resolve();
    }
    requestAnimationFrame(tick);
  });
  deltas.sort((a, b) => a - b);
  const pick = (q) => deltas[Math.min(deltas.length - 1, Math.floor(q * deltas.length))];

  let renderer = 'unknown';
  try {
    const gl = canvas.getContext('webgl2');
    if (gl) {
      const ext = gl.getExtension('WEBGL_debug_renderer_info');
      if (ext) renderer = gl.getParameter(ext.UNMASKED_RENDERER_WEBGL);
      else renderer = gl.getParameter(gl.RENDERER);
    }
  } catch (e) { /* ignore */ }

  const result = {
    ok: !!window.__mwReady,
    fps_p50: 1000 / pick(0.5),
    fps_p5: 1000 / pick(0.95),
    ms_p50: pick(0.5),
    ms_p95: pick(0.95),
    frames: deltas.length,
    width: canvas.width,
    height: canvas.height,
    renderer,
    ua: navigator.userAgent,
    ts: new Date().toISOString(),
  };
  console.log('[bench]', JSON.stringify(result));
  try {
    await fetch('/__bench', {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify(result),
    });
  } catch (e) { /* ignore */ }
}

// ------------------------------------------------------------------ boot

(async () => {
  sizeCanvas();
  try {
    const mod = await import('./build/mw.js');
    const createMw = mod.default || mod.createMw;
    Module = await createMw({
      canvas,
      locateFile: (path) => './build/' + path,
      print: (t) => console.log(t),
      printErr: (t) => console.warn(t),
    });
    window.__mwModule = Module;
    if (Module._mw_resize) Module._mw_resize(canvas.width, canvas.height);
    sizeCanvas();
    setTimeout(hudIntro, 150);
    if (new URLSearchParams(location.search).has('bench')) runBench();
  } catch (e) {
    console.error(e);
    fail('Falha ao carregar o jogo: ' + (e && e.message ? e.message : e));
  }
})();
