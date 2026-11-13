/* Lost Odyssey Recomp - dual-screen companion (AYN Thor and other dual-screen handhelds).
 * The native runtime publishes a JSON snapshot (companion/companion.cpp); this page polls it
 * through the LO bridge (DualScreenCompanion.java) and sends held gamepad buttons back.
 * Opened in a desktop browser without the bridge, it runs on built-in sample data. */
(function () {
  'use strict';

  // ---------------------------------------------------------------- i18n
  const STRINGS = {
    es: {
      party: 'Grupo', map: 'Mapa', bag: 'Bolsa', pad: 'Mando', reserve: 'Reserva', places: 'Lugares',
      items: 'Objetos', materials: 'Materiales', settings: 'Ajustes', language: 'Idioma',
      optHaptics: 'Vibración al tocar', optBattle: 'Mostrar el mando al entrar en combate',
      optTeleport: 'Permitir teletransporte (depuración)',
      teleportWarn: 'El teletransporte usa la herramienta de depuración del port. Guarda antes de usarlo.',
      disable: 'Apagar panel', close: 'Cerrar', wake: 'Toca para despertar',
      waitingBoot: 'Esperando al juego…', waitingTitle: 'Pantalla de título · carga una partida',
      modes: { field: 'Explorando', battle: 'Combate', menu: 'Menú', event: 'Escena', title: 'Título' },
      disc: 'Disco {n}', rowFront: 'Delante', rowBack: 'Detrás', hp: 'PS', mp: 'PM', exp: 'EXP',
      weapon: 'Arma', ring: 'Anillo', accessories: 'Accesorios', skills: 'Habilidades aprendidas',
      freeSlot: 'Libre', none: '—', unknown: 'Desconocido #{id}',
      stale: 'En combate: valores del último momento fuera de combate.',
      noReserve: 'Nadie en reserva.', emptyBag: 'Nada por aquí.',
      noMap: 'Sin datos de mapa aquí (combate, escena o menú).', noPois: 'No hay lugares de interés cargados.',
      selectPoi: 'Toca un lugar para ver la distancia.', distance: '{d} m', teleport: 'Ir aquí',
      teleportQueued: 'Teletransporte en cola…', teleportRefused: 'El juego no puede teletransportarte ahora.',
      back: 'Back', backHelp: 'Minimapa', start: 'Start', startHelp: 'Menú', ring: 'Anillo', ringHelp: 'RT · golpe de anillo',
      debug: 'Depuración', ltHelp: 'Avance rápido (mantener)',
      battleBanner: '⚔ Combate · RT para el anillo en el momento justo', you: 'Tú',
      upAxis: 'Norte ↑ (eje del juego)',
      cats: { save: 'Punto de guardado', exit: 'Salida', gimmick: 'Mecanismo', pickup: 'Objeto / punto de contacto', entrance: 'Entrada del mapa', other: 'Punto' },
      themeOn: 'Tema del juego cargado desde tus archivos.', themeOff: 'Sin tema del juego: crea la carpeta companion-theme con tools/companion_theme.py.'
    },
    en: {
      party: 'Party', map: 'Map', bag: 'Bag', pad: 'Pad', reserve: 'Reserve', places: 'Places',
      items: 'Items', materials: 'Materials', settings: 'Settings', language: 'Language',
      optHaptics: 'Vibrate on touch', optBattle: 'Show the pad when a battle starts',
      optTeleport: 'Allow teleport (debug)',
      teleportWarn: 'Teleport uses the port\'s debug tool. Save before using it.',
      disable: 'Turn panel off', close: 'Close', wake: 'Tap to wake',
      waitingBoot: 'Waiting for the game…', waitingTitle: 'Title screen · load a save',
      modes: { field: 'Exploring', battle: 'Battle', menu: 'Menu', event: 'Scene', title: 'Title' },
      disc: 'Disc {n}', rowFront: 'Front', rowBack: 'Back row', hp: 'HP', mp: 'MP', exp: 'EXP',
      weapon: 'Weapon', ring: 'Ring', accessories: 'Accessories', skills: 'Learned skills',
      freeSlot: 'Free', none: '—', unknown: 'Unknown #{id}',
      stale: 'In battle: values from the last moment outside battle.',
      noReserve: 'Nobody in reserve.', emptyBag: 'Nothing here.',
      noMap: 'No map data here (battle, scene or menu).', noPois: 'No points of interest loaded.',
      selectPoi: 'Tap a place to see its distance.', distance: '{d} m', teleport: 'Go here',
      teleportQueued: 'Teleport queued…', teleportRefused: 'The game cannot teleport you right now.',
      back: 'Back', backHelp: 'Minimap', start: 'Start', startHelp: 'Menu', ring: 'Ring', ringHelp: 'RT · ring timing',
      debug: 'Debug', ltHelp: 'Fast-forward (hold)',
      battleBanner: '⚔ Battle · RT for ring timing', you: 'You',
      upAxis: 'North ↑ (game axis)',
      cats: { save: 'Save point', exit: 'Exit', gimmick: 'Mechanism', pickup: 'Item / touch point', entrance: 'Map entrance', other: 'Point' },
      themeOn: 'Game theme loaded from your files.', themeOff: 'No game theme: build the companion-theme folder with tools/companion_theme.py.'
    }
  };

  const bridge = window.LO || null;
  const settings = Object.assign(
    { lang: 'auto', haptics: true, battlePad: true, teleport: false, view: 'party', bag: 'items' },
    JSON.parse(localStorage.getItem('lo-companion') || '{}'));
  const save = () => localStorage.setItem('lo-companion', JSON.stringify(settings));

  function lang() {
    if (settings.lang !== 'auto') return settings.lang;
    const l = ((bridge && bridge.language && bridge.language()) || navigator.language || 'es').toLowerCase();
    return l.startsWith('es') ? 'es' : 'en';
  }
  let T = STRINGS[lang()];
  const fmt = (s, v) => s.replace(/\{(\w+)\}/g, (_, k) => v[k]);
  const esc = (s) => String(s == null ? '' : s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  const num = (n) => Number(n || 0).toLocaleString(lang() === 'es' ? 'es-ES' : 'en-US');
  const $ = (id) => document.getElementById(id);

  function applyI18n() {
    T = STRINGS[lang()];
    document.documentElement.lang = lang();
    document.querySelectorAll('[data-i18n]').forEach((el) => { el.textContent = T[el.dataset.i18n] || ''; });
    document.querySelectorAll('[data-i18n-title]').forEach((el) => { el.title = T[el.dataset.i18nTitle] || ''; });
  }

  function haptic() { if (settings.haptics && bridge && bridge.haptic) bridge.haptic(); }

  // ---------------------------------------------------------------- state polling
  let state = {};
  let lastSerial = -1;
  let selectedChar = null;
  let selectedPoi = null;
  let previousMode = '';
  let viewBeforeBattle = null;

  function readState() {
    if (!bridge) return Mock.next();
    try { return JSON.parse(bridge.state() || '{}'); } catch (e) { return {}; }
  }

  function poll() {
    const next = readState();
    if (next.serial === lastSerial && next.serial !== undefined) return;
    lastSerial = next.serial;
    state = next;
    onModeChange();
    render();
  }

  function onModeChange() {
    const mode = state.mode || '';
    if (mode === previousMode) return;
    if (settings.battlePad && mode === 'battle' && currentView !== 'pad') {
      viewBeforeBattle = currentView;
      showView('pad', true);
    } else if (previousMode === 'battle' && viewBeforeBattle) {
      showView(viewBeforeBattle, true);
      viewBeforeBattle = null;
    }
    previousMode = mode;
  }

  // ---------------------------------------------------------------- views
  let currentView = settings.view;
  function hasGame() { return state.mode && state.mode !== 'title' && Array.isArray(state.characters); }

  function showView(view, auto) {
    currentView = view;
    if (!auto) { settings.view = view; save(); }
    document.querySelectorAll('.tab').forEach((t) => t.classList.toggle('active', t.dataset.view === view));
    render();
  }

  function render() {
    const live = hasGame();
    const visible = live || currentView === 'pad' ? currentView : 'waiting';
    document.querySelectorAll('.view').forEach((v) => v.classList.toggle('active', v.id === 'view-' + visible));
    renderHeader();
    if (visible === 'waiting') $('waitingText').textContent = state.mode === 'title' ? T.waitingTitle : T.waitingBoot;
    if (visible === 'party') renderParty();
    if (visible === 'map') renderMap();
    if (visible === 'bag') renderBag();
    if (visible === 'pad') renderPad();
  }

  function renderHeader() {
    const mode = state.mode || '';
    $('location').textContent = state.location || 'Lost Odyssey';
    const m = $('mode');
    m.textContent = T.modes[mode] || '—';
    m.className = 'chip mode ' + mode;
    $('disc').textContent = state.disc ? fmt(T.disc, { n: state.disc }) : '—';
    $('disc').classList.toggle('hidden', !state.disc);
    $('gold').textContent = state.gold != null ? num(state.gold) : '';
    $('goldBox').classList.toggle('hidden', state.gold == null);
  }

  // Party ------------------------------------------------------------
  const FACES = ['kaim', 'seth', 'ming', 'sarah', 'jansen', 'cooke', 'mack', 'sed', 'tolten'];
  const face = (c, small) => (theme ? `background-image:var(--t-face${small ? '_small' : ''}_${FACES[c.id]})` : '');
  const sprite = (key) => `<i class="sprite" style="background-image:var(--t-${key})"></i>`;
  const pct = (a, b) => (b > 0 ? Math.max(0, Math.min(100, (a / b) * 100)) : 0);

  function gauges(c) {
    const low = c.maxHp > 0 && c.hp / c.maxHp < 0.25;
    return `
      <span class="gline"><span class="glabel">${T.hp}</span><span class="gval num ${c.hp <= 0 ? 'ko' : ''}">${num(c.hp)}<small> / ${num(c.maxHp)}</small></span>
        <span class="gauge hp ${low ? 'low' : ''}"><i style="width:${pct(c.hp, c.maxHp)}%"></i></span></span>
      <span class="gline"><span class="glabel">${T.mp}</span><span class="gval num">${num(c.mp)}<small> / ${num(c.maxMp)}</small></span>
        <span class="gauge mp"><i style="width:${pct(c.mp, c.maxMp)}%"></i></span></span>
      <span class="gauge exp"><i style="width:${pct(c.exp, 100)}%"></i></span>`;
  }

  function charById(id) { return (state.characters || []).find((c) => c.id === id); }

  function renderParty() {
    const party = state.party || [];
    const chars = state.characters || [];
    if (selectedChar == null || !charById(selectedChar)) selectedChar = party[0] != null ? party[0] : (chars[0] && chars[0].id);
    $('staleNote').textContent = T.stale;
    $('staleNote').classList.toggle('hidden', !!state.partyLive);

    let html = '';
    for (let i = 0; i < 5; i++) {
      const c = charById(party[i]);
      if (!c) { html += '<div class="member empty"></div>'; continue; }
      const sel = c.id === selectedChar;
      html += `<button class="member ${sel ? 'sel' : ''}" data-char="${c.id}">
        ${sel ? '<i class="cursor"></i>' : ''}
        <span class="portrait" style="${face(c)}"><span class="initial">${esc(c.name[0])}</span>
          <span class="row-tag">${c.backRow ? T.rowBack : T.rowFront}</span><span class="who">${esc(c.name)}</span></span>
        <span class="stats">${gauges(c)}</span>
      </button>`;
    }
    $('partyList').innerHTML = html;

    const reserve = chars.filter((c) => !party.includes(c.id));
    $('reserveList').innerHTML = reserve.length ? reserve.map((c) => `
      <button class="mini ${c.id === selectedChar ? 'sel' : ''}" data-char="${c.id}" style="${face(c, true)}"><span>${esc(c.name)}</span></button>`).join('')
      : `<div class="muted">${T.noReserve}</div>`;

    const c = charById(selectedChar);
    if (!c) { $('charDetail').innerHTML = ''; return; }
    const named = (e) => (e ? esc(e.name || fmt(T.unknown, { id: e.id })) : T.none);
    const acc = [];
    for (let i = 0; i < (c.accessorySlots || 0); i++) {
      const a = c.accessories[i];
      acc.push(a ? `<span class="acc">${named(a)}</span>` : `<span class="acc free">${T.freeSlot}</span>`);
    }
    $('charDetail').innerHTML = `
      <div class="plate-title"><span>${esc(c.name)}</span><span>${c.backRow ? T.rowBack : T.rowFront}</span></div>
      <div class="hero"><span class="pic" style="${face(c, true)}"></span>
        <div class="big"><div><b>${T.hp}</b> <span class="num">${num(c.hp)} / ${num(c.maxHp)}</span></div>
        <div><b>${T.mp}</b> <span class="num">${num(c.mp)} / ${num(c.maxMp)}</span></div>
        <div><b>${T.exp}</b> <span class="num">${Math.round(c.exp)}%</span></div></div></div>
      <div class="list">
        <div class="equip">${sprite('icon_weapon')}<div class="grow"><div class="k">${T.weapon}</div><div class="v">${named(c.weapon)}</div></div></div>
        <div class="equip">${sprite('icon_ring')}<div class="grow"><div class="k">${T.ring}</div><div class="v">${named(c.ring)}</div></div></div>
        <div class="equip">${sprite('icon_emblem')}<div class="grow"><div class="k">${T.accessories}</div><div class="acc-list">${acc.join('') || T.none}</div></div></div>
        <div class="equip">${sprite('icon_quill')}<div class="grow"><div class="k">${T.skills}</div><div class="v num">${num(c.skills)}</div></div></div>
      </div>`;
  }

  // Map -------------------------------------------------------------
  // The patched APK reports a category instead of a label: number repeats per category.
  function labelPois(list) {
    const seen = {};
    return list.map((p) => {
      if (p.label) return p;
      seen[p.cat] = (seen[p.cat] || 0) + 1;
      const name = (T.cats && T.cats[p.cat]) || p.cat || '?';
      const total = list.filter((o) => o.cat === p.cat).length;
      return Object.assign({}, p, { label: total > 1 ? `${name} ${seen[p.cat]}` : name });
    });
  }
  let zoom = 1;
  function poiDistance(p, m) { return Math.hypot(p.x - m.x, p.y - m.y); }

  function renderMap() {
    const m = state.map || {};
    const pois = labelPois((m.available && m.pois) || []);
    $('mapNote').textContent = m.available ? T.upAxis : T.noMap;
    $('poiCount').textContent = pois.length ? String(pois.length) : '';
    const sorted = pois.map((p) => ({ p, d: poiDistance(p, m) })).sort((a, b) => a.d - b.d);
    $('poiList').innerHTML = !m.available ? `<div class="muted small" style="padding:1vh">${T.noMap}</div>`
      : !sorted.length ? `<div class="muted small" style="padding:1vh">${T.noPois}</div>`
      : sorted.map(({ p, d }) => `<button class="poi ${p.id === selectedPoi ? 'sel' : ''}" data-poi="${esc(p.id)}">
          <span>${esc(p.label)}</span><span class="d">${fmt(T.distance, { d: num(Math.round(d / 100)) })}</span></button>`).join('');
    const sel = pois.find((p) => p.id === selectedPoi);
    $('poiDetail').innerHTML = !sel ? `<span class="muted small">${T.selectPoi}</span>`
      : `<span>${esc(sel.label)}</span>` + (settings.teleport ? `<button class="btn gold" id="teleportBtn">${T.teleport}</button>` : '');
    drawRadar();
  }

  function drawRadar() {
    const canvas = $('radar');
    const rect = canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    if (canvas.width !== Math.round(rect.width * dpr) || canvas.height !== Math.round(rect.height * dpr)) {
      canvas.width = Math.round(rect.width * dpr); canvas.height = Math.round(rect.height * dpr);
    }
    const g = canvas.getContext('2d');
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    const w = rect.width, h = rect.height, cx = w / 2, cy = h / 2, R = Math.min(w, h) / 2 - 18;
    g.clearRect(0, 0, w, h);

    // rings and cross
    g.strokeStyle = 'rgba(220,224,230,0.16)'; g.lineWidth = 1;
    for (let i = 1; i <= 4; i++) { g.beginPath(); g.arc(cx, cy, (R * i) / 4, 0, Math.PI * 2); g.stroke(); }
    g.beginPath(); g.moveTo(cx - R, cy); g.lineTo(cx + R, cy); g.moveTo(cx, cy - R); g.lineTo(cx, cy + R); g.stroke();
    const sweep = g.createRadialGradient(cx, cy, 0, cx, cy, R);
    sweep.addColorStop(0, 'rgba(200,210,200,0.08)'); sweep.addColorStop(1, 'rgba(200,210,200,0)');
    g.fillStyle = sweep; g.beginPath(); g.arc(cx, cy, R, 0, Math.PI * 2); g.fill();
    if (compassImg && compassImg.complete && compassImg.naturalWidth) { g.globalAlpha = 0.25; const cs = R * 0.9; g.drawImage(compassImg, cx - cs / 2, cy - cs / 2, cs, cs); g.globalAlpha = 1; }

    const m = state.map || {};
    if (!m.available) return;
    const pois = labelPois(m.pois || []);
    const far = Math.max(1500, ...pois.map((p) => poiDistance(p, m)));
    const scale = (R / far) * zoom;
    radarHit = [];
    g.font = `${Math.round(h * 0.034)}px ${getComputedStyle(document.body).fontFamily}`;
    g.textBaseline = 'middle';
    for (const p of pois) {
      let dx = (p.x - m.x) * scale, dy = -(p.y - m.y) * scale;
      const d = Math.hypot(dx, dy), clipped = d > R;
      if (clipped) { dx *= R / d; dy *= R / d; }
      const x = cx + dx, y = cy + dy, sel = p.id === selectedPoi;
      g.fillStyle = sel ? '#ffffff' : clipped ? 'rgba(190,196,204,0.45)' : '#c3c8cf';
      g.beginPath(); g.arc(x, y, sel ? 9 : 6, 0, Math.PI * 2); g.fill();
      if (sel) { g.strokeStyle = '#ffffff'; g.lineWidth = 2; g.beginPath(); g.arc(x, y, 15, 0, Math.PI * 2); g.stroke(); }
      if (!clipped || sel) {
        g.fillStyle = sel ? '#ffffff' : 'rgba(225,229,234,0.8)'; g.shadowColor = '#000'; g.shadowBlur = 4;
        const right = x < w - 180;
        g.textAlign = right ? 'left' : 'right';
        g.fillText(p.label, x + (right ? 14 : -14), y); g.shadowBlur = 0;
      }
      radarHit.push({ id: p.id, x, y });
    }
    // player
    g.fillStyle = '#6fd08c'; g.strokeStyle = '#03060c'; g.lineWidth = 3;
    g.beginPath(); g.arc(cx, cy, 10, 0, Math.PI * 2); g.fill(); g.stroke();
    g.fillStyle = '#6fd08c'; g.textAlign = 'left'; g.fillText(T.you, cx + 14, cy + 20);
  }
  let radarHit = [];

  // Bag -------------------------------------------------------------
  function renderBag() {
    document.querySelectorAll('.seg').forEach((s) => s.classList.toggle('active', s.dataset.bag === settings.bag));
    const list = state[settings.bag] || [];
    $('bagGrid').innerHTML = list.length ? list.map((e) =>
      `<div class="item">${sprite(settings.bag === 'items' ? 'icon_item' : 'icon_emblem')}<span class="n">${esc(e.name)}</span><span class="c num">× ${num(e.count)}</span></div>`).join('')
      : `<div class="empty-msg">${T.emptyBag}</div>`;
  }

  // Pad -------------------------------------------------------------
  function renderPad() {
    $('battleBanner').textContent = T.battleBanner;
    $('battleBanner').classList.toggle('hidden', state.mode !== 'battle');
  }

  const MASK = { UP: 0x1, DOWN: 0x2, LEFT: 0x4, RIGHT: 0x8, START: 0x10, BACK: 0x20,
    LB: 0x100, RB: 0x200, A: 0x1000, B: 0x2000, X: 0x4000, Y: 0x8000 };
  const held = new Map(); // pointerId -> {el, names}
  let sentKey = '';

  function sendButtons() {
    let mask = 0, lt = 0, rt = 0;
    for (const { names } of held.values()) for (const n of names) {
      if (n === 'LT') lt = 255; else if (n === 'RT') rt = 255; else mask |= MASK[n] || 0;
    }
    const key = mask + ':' + lt + ':' + rt;
    if (key === sentKey) return;
    sentKey = key;
    if (bridge) bridge.buttons(mask, lt, rt);
  }

  function release(id) {
    const h = held.get(id);
    if (!h) return;
    held.delete(id);
    if (![...held.values()].some((o) => o.el === h.el)) h.el.classList.remove('pressed');
    sendButtons();
  }
  function releaseAll() { for (const id of [...held.keys()]) release(id); sentKey = ''; if (bridge) bridge.buttons(0, 0, 0); }

  document.querySelectorAll('.pbtn').forEach((el) => {
    el.addEventListener('pointerdown', (e) => {
      e.preventDefault();
      el.setPointerCapture(e.pointerId);
      held.set(e.pointerId, { el, names: el.dataset.btn.split('+') });
      el.classList.add('pressed');
      haptic();
      sendButtons();
    });
    ['pointerup', 'pointercancel', 'lostpointercapture'].forEach((t) => el.addEventListener(t, (e) => release(e.pointerId)));
  });
  window.addEventListener('blur', releaseAll);
  document.addEventListener('visibilitychange', () => { if (document.hidden) releaseAll(); });

  // ---------------------------------------------------------------- interactions
  document.querySelectorAll('.tab').forEach((t) => t.addEventListener('click', () => { haptic(); showView(t.dataset.view); }));
  document.querySelectorAll('.seg').forEach((s) => s.addEventListener('click', () => { haptic(); settings.bag = s.dataset.bag; save(); renderBag(); }));

  $('view-party').addEventListener('click', (e) => {
    const b = e.target.closest('[data-char]');
    if (!b) return;
    haptic(); selectedChar = Number(b.dataset.char); renderParty();
  });
  $('poiList').addEventListener('click', (e) => {
    const b = e.target.closest('[data-poi]');
    if (!b) return;
    haptic(); selectedPoi = b.dataset.poi === selectedPoi ? null : b.dataset.poi; renderMap();
  });
  $('radar').addEventListener('click', (e) => {
    const r = e.currentTarget.getBoundingClientRect();
    const x = e.clientX - r.left, y = e.clientY - r.top;
    let best = null, bd = 40;
    for (const h of radarHit) { const d = Math.hypot(h.x - x, h.y - y); if (d < bd) { bd = d; best = h; } }
    if (best) { haptic(); selectedPoi = best.id; renderMap(); }
  });
  $('poiDetail').addEventListener('click', (e) => {
    if (e.target.id !== 'teleportBtn' || !selectedPoi) return;
    haptic();
    const ok = bridge ? bridge.action(JSON.stringify({ type: 'teleportPoi', id: selectedPoi })) : true;
    e.target.textContent = ok ? T.teleportQueued : T.teleportRefused;
    e.target.disabled = true;
  });
  const setZoom = (z) => { zoom = Math.max(0.25, Math.min(16, z)); drawRadar(); };
  $('zoomIn').addEventListener('click', () => { haptic(); setZoom(zoom * 1.5); });
  $('zoomOut').addEventListener('click', () => { haptic(); setZoom(zoom / 1.5); });
  $('zoomFit').addEventListener('click', () => { haptic(); setZoom(1); });

  // pinch zoom on the radar
  const pinch = new Map(); let pinchStart = 0, zoomStart = 1;
  const radar = $('radar');
  radar.addEventListener('pointerdown', (e) => { pinch.set(e.pointerId, e); if (pinch.size === 2) { const [a, b] = [...pinch.values()]; pinchStart = Math.hypot(a.clientX - b.clientX, a.clientY - b.clientY); zoomStart = zoom; } });
  radar.addEventListener('pointermove', (e) => {
    if (!pinch.has(e.pointerId)) return;
    pinch.set(e.pointerId, e);
    if (pinch.size === 2 && pinchStart > 0) { const [a, b] = [...pinch.values()]; setZoom(zoomStart * Math.hypot(a.clientX - b.clientX, a.clientY - b.clientY) / pinchStart); }
  });
  ['pointerup', 'pointercancel'].forEach((t) => radar.addEventListener(t, (e) => { pinch.delete(e.pointerId); pinchStart = 0; }));

  // sleep (black AMOLED screen)
  $('sleepBtn').addEventListener('click', () => { haptic(); releaseAll(); $('sleep').classList.remove('hidden'); });
  $('sleep').addEventListener('click', () => { haptic(); $('sleep').classList.add('hidden'); });

  // settings
  const opt = { optLang: 'lang', optHaptics: 'haptics', optBattle: 'battlePad', optTeleport: 'teleport' };
  $('settingsBtn').addEventListener('click', () => {
    haptic();
    for (const [id, key] of Object.entries(opt)) {
      const el = $(id);
      if (el.type === 'checkbox') el.checked = !!settings[key]; else el.value = settings[key];
    }
    const noTeleport = state.features && state.features.teleport === false;
    $('optTeleport').closest('.row').classList.toggle('hidden', noTeleport);
    document.querySelector('[data-i18n="teleportWarn"]').classList.toggle('hidden', noTeleport);
    $('settings').classList.remove('hidden');
  });
  for (const [id, key] of Object.entries(opt)) {
    $(id).addEventListener('change', (e) => {
      settings[key] = e.target.type === 'checkbox' ? e.target.checked : e.target.value;
      save(); applyI18n(); render();
    });
  }
  $('closeSettings').addEventListener('click', () => { haptic(); $('settings').classList.add('hidden'); });
  $('disableBtn').addEventListener('click', () => { releaseAll(); if (bridge && bridge.disable) bridge.disable(); });
  window.addEventListener('resize', () => { if (currentView === 'map') drawRadar(); });

  // ---------------------------------------------------------------- sample data (desktop preview)
  const Mock = {
    t: 0,
    next() {
      this.t++;
      const mode = new URLSearchParams(location.search).get('mode') || 'field';
      const wobble = Math.sin(Math.floor(this.t / 10) / 2);
      const ch = (id, name, hp, maxHp, mp, maxMp, exp, back, weapon, ring, acc, slots, skills) =>
        ({ id, name, hp, maxHp, mp, maxMp, exp, backRow: back, weapon, ring, accessories: acc, accessorySlots: slots, skills });
      return {
        serial: Math.floor(this.t / 10), mode, disc: 1, paused: false, location: 'Uhra · Distrito Comercial',
        partyLive: mode !== 'battle', gold: 48210, party: [0, 1, 4, 2, 3],
        characters: [
          ch(0, 'Kaim', Math.round(1920 + 80 * wobble), 2104, 212, 240, 63, false, { id: 1, name: 'Platinum Sword' }, { id: 7, name: 'Fire Ring' }, [{ id: 3, name: 'Bracelet' }, { id: 9, name: 'Hard Turtle Shell' }], 4, 37),
          ch(1, 'Seth', 1670, 1820, 140, 180, 12, false, { id: 2, name: 'White Fang' }, { id: 8, name: 'Aqua Ring' }, [{ id: 5, name: 'Sky Boots' }], 3, 29),
          ch(2, 'Ming', 980, 1120, 610, 640, 88, true, { id: 30, name: 'Chief\'s Staff' }, null, [{ id: 11, name: 'Mana Mantle' }], 2, 41),
          ch(3, 'Sarah', 210, 1050, 702, 720, 40, true, { id: 31, name: 'Holy Bow' }, { id: 9, name: 'Wind Ring' }, [], 2, 44),
          ch(4, 'Jansen', 1203, 1290, 515, 520, 5, false, { id: 32, name: 'Rod of Fire' }, null, [{ id: 12, name: 'Spell Pin' }], 2, 22),
          ch(5, 'Cooke', 760, 800, 330, 350, 70, true, null, null, [], 2, 15),
          ch(6, 'Mack', 790, 820, 300, 320, 71, true, null, null, [], 2, 14)
        ],
        items: [{ name: 'Healing Herb', count: 24 }, { name: 'Healing Medicine', count: 9 }, { name: 'Mana Herb', count: 17 },
          { name: 'Antidote', count: 6 }, { name: 'Eye Drops', count: 3 }, { name: 'Smelling Salts', count: 4 },
          { name: 'Cure-All', count: 2 }, { name: 'Flare Bomb', count: 5 }, { name: 'Aqua Bomb', count: 5 }],
        materials: [{ name: 'Magma Fragment', count: 3 }, { name: 'Sun Crystal', count: 1 }, { name: 'Cold Water Stone', count: 2 }],
        map: mode === 'field' ? {
          available: true, x: 1000 + 200 * wobble, y: 2000, z: 0, revision: 1,
          pois: [
            { id: '1', label: 'Inn', x: 2400, y: 3100, z: 0 }, { id: '2', label: 'Item Shop', x: -400, y: 2600, z: 0 },
            { id: '3', label: 'Save Point', x: 1300, y: 900, z: 0 }, { id: '4', label: 'Weapon Shop', x: 3600, y: 1500, z: 0 },
            { id: '5', label: 'Harbor', x: -2600, y: -1800, z: 0 }, { id: '6', label: 'Grand Staff', x: 9000, y: 7000, z: 0 }]
        } : { available: false }
      };
    }
  };

  // ---------------------------------------------------------------- theme from the player's game files
  // Android serves Android/data/<app>/files/companion-theme/ at ./theme/ (DualScreenCompanion.java);
  // a desktop preview can point ?theme= at any folder. The mod itself ships no game art.
  let theme = null;
  let compassImg = null;
  function loadTheme() {
    const base = new URLSearchParams(location.search).get('theme') || 'theme/';
    fetch(base + 'theme.json').then((r) => (r.ok ? r.json() : null)).then((json) => {
      if (!json || !json.pieces) throw new Error('no theme');
      theme = json.pieces;
      const root = document.documentElement.style;
      for (const [key, piece] of Object.entries(theme)) root.setProperty('--t-' + key, `url("${base}${piece.file}")`);
      document.querySelectorAll('.sprite[data-sprite]').forEach((el) => { el.style.backgroundImage = `var(--t-${el.dataset.sprite})`; });
      if (theme.compass) { compassImg = new Image(); compassImg.onload = () => { if (currentView === 'map') drawRadar(); }; compassImg.src = base + theme.compass.file; }
      document.body.classList.add('themed');
      render();
    }).catch(() => { theme = null; }).finally(() => { $('themeStatus').textContent = theme ? T.themeOn : T.themeOff; });
  }

  // ---------------------------------------------------------------- boot
  applyI18n();
  loadTheme();
  document.querySelectorAll('.tab').forEach((t) => t.classList.toggle('active', t.dataset.view === currentView));
  poll();
  setInterval(poll, 200);
})();
