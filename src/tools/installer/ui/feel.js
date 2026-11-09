"use strict";
// Legend of Kairi installer: look and feel (s185). Loaded after app.js, which calls these through the F() wrapper and shares its globals
// (S, view, W, H, g, layers, stars, risers, still, calm, MX/MY/MK, KX, PR/PY/SQ ...). Everything here is optional: if this file fails,
// the installer still works. Cheap by design: one delegated listener each, timers that only act while the window is in front and visible,
// canvas effects drawn from the pre-made sprites (layers.gold / layers.blue / the shard sprites below), nothing in Calm beyond a pulse.
window.Feel = (function () {
  const Feel = {};
  const nowS = () => performance.now() / 1000;
  const inFront = () => !document.hidden && document.hasFocus();
  const vis = (el) => !!el && !el.hidden && !el.disabled && el.offsetParent !== null;
  let usedKeys = false;      // keyboard / pad used: the Play button may take focus by itself

  // ------------------------------------------------------------------ hover light + press ripple (delegated, one listener each)
  let pm = null, pmRaf = 0;
  const lit = (t) => (t && t.closest ? t.closest(".row, .btn") : null);
  document.addEventListener("pointermove", (e) => {
    const el = lit(e.target);
    if (!el || el.disabled) return;
    pm = [el, e.clientX, e.clientY];
    if (pmRaf) return;
    pmRaf = requestAnimationFrame(() => {
      pmRaf = 0;
      const r = pm[0].getBoundingClientRect();
      pm[0].style.setProperty("--mx", (pm[1] - r.left).toFixed(0) + "px");
      pm[0].style.setProperty("--my", (pm[2] - r.top).toFixed(0) + "px");
    });
  }, { passive: true });
  document.addEventListener("pointerdown", (e) => {
    if (still || calm || e.button) return;
    const el = lit(e.target);
    if (!el || el.disabled) return;
    const r = el.getBoundingClientRect(), s = document.createElement("span");
    s.className = "rip";
    s.style.left = (e.clientX - r.left).toFixed(0) + "px";
    s.style.top = (e.clientY - r.top).toFixed(0) + "px";
    s.addEventListener("animationend", () => s.remove(), { once: true });
    for (const o of el.querySelectorAll(".rip")) o.remove();
    el.appendChild(s);
    setTimeout(() => s.remove(), 800);     // (also when the row is hidden and the animation never ends)
  }, true);

  // ------------------------------------------------------------------ the menu pointer: one gold heart that slides to the selected row
  let ptrY = null;
  function placePtr(snap) {
    const p = $("ptr"), sel = document.querySelector("#s-setup .rows .row.sel");
    if (!p) return;
    if ($("s-setup").hidden || !sel || !sel.offsetHeight) { p.classList.remove("on"); ptrY = null; return; }
    const y = Math.round(sel.offsetTop + (sel.offsetHeight - 14) / 2);
    if (y === ptrY && p.classList.contains("on") && !snap) return;
    const first = !p.classList.contains("on") || snap;
    if (first) p.classList.add("snap");
    p.style.transform = "translateY(" + y + "px)";
    p.classList.add("on");
    ptrY = y;
    if (first) requestAnimationFrame(() => requestAnimationFrame(() => p.classList.remove("snap")));
  }
  Feel.ptr = () => placePtr(false);
  if (document.fonts && document.fonts.ready) document.fonts.ready.then(() => placePtr(true));
  addEventListener("load", () => placePtr(true));

  // ------------------------------------------------------------------ keyboard: arrows, Escape = Back (never Stop)
  function navItems() {
    if (view === "setup") return [...document.querySelectorAll("#s-setup .rows .row")].filter(vis);
    return [...document.querySelectorAll(".screen:not([hidden]) .card .btn, .screen:not([hidden]) .actions .btn")].filter(vis);
  }
  function nav(dir) {            // dir -1 / +1; 0 = just focus the selected / first one
    const list = navItems();
    if (!list.length) return;
    const cur = list.indexOf(document.activeElement);
    let i;
    if (cur < 0) {
      const sel = view === "setup" ? list.indexOf(document.querySelector("#s-setup .rows .row.sel")) : -1;
      i = sel >= 0 ? sel : dir < 0 ? list.length - 1 : 0;
    } else i = Math.min(list.length - 1, Math.max(0, cur + dir));
    list[i].focus();
  }
  function goBack() {
    const b = vis($("b-back")) ? $("b-back") : vis($("b-cback")) ? $("b-cback") : null;
    if (b) b.click();
  }
  document.addEventListener("keydown", (e) => {
    if (e.altKey || e.ctrlKey || e.metaKey || e.defaultPrevented) return;
    const k = e.key;
    if (view === "setup" && (k === "ArrowDown" || k === "ArrowUp")) { usedKeys = true; e.preventDefault(); nav(k === "ArrowDown" ? 1 : -1); }
    else if (view !== "setup" && (k === "ArrowRight" || k === "ArrowLeft")) { usedKeys = true; e.preventDefault(); nav(k === "ArrowRight" ? 1 : -1); }
    else if (k === "Escape") { usedKeys = true; goBack(); }
  });

  // ------------------------------------------------------------------ gamepad: only while one is connected and the window is in front
  const pads = new Set();
  let padTimer = 0, held = {}, wasA = false, wasB = false;
  function syncPad() {
    const want = pads.size > 0 && inFront();
    if (want && !padTimer) padTimer = setInterval(padPoll, 100);
    else if (!want && padTimer) { clearInterval(padTimer); padTimer = 0; held = {}; }
  }
  function padPoll() {
    if (!inFront()) { syncPad(); return; }
    let list = [];
    try { list = [...navigator.getGamepads()].filter(Boolean); } catch (e) { return; }
    const dir = { up: false, down: false, left: false, right: false };
    let a = false, b = false;
    for (const p of list) {
      const bt = (i) => !!(p.buttons[i] && p.buttons[i].pressed), ax = p.axes || [];
      dir.up = dir.up || bt(12) || ax[1] < -0.6; dir.down = dir.down || bt(13) || ax[1] > 0.6;
      dir.left = dir.left || bt(14) || ax[0] < -0.6; dir.right = dir.right || bt(15) || ax[0] > 0.6;
      a = a || bt(0); b = b || bt(1);
    }
    const now = performance.now();
    for (const d of ["up", "down", "left", "right"]) {
      if (!dir[d]) { delete held[d]; continue; }
      if (held[d] === undefined) held[d] = now + 400;          // first press moves at once, then after 400 ms, then every ~150 ms
      else if (now >= held[d]) held[d] = now + 150;
      else continue;
      usedKeys = true;
      if (view === "setup" && (d === "up" || d === "down")) nav(d === "down" ? 1 : -1);
      else if (view !== "setup" && (d === "left" || d === "right")) nav(d === "right" ? 1 : -1);
    }
    if (a && !wasA) {
      usedKeys = true;
      const el = document.activeElement;
      if (el && el.matches && el.matches(".row, .btn") && vis(el)) el.click(); else nav(0);
    }
    if (b && !wasB) { usedKeys = true; goBack(); }
    wasA = a; wasB = b;
  }
  addEventListener("gamepadconnected", (e) => { pads.add(e.gamepad.index); syncPad(); });
  addEventListener("gamepaddisconnected", (e) => { pads.delete(e.gamepad.index); syncPad(); });
  addEventListener("focus", syncPad); addEventListener("blur", syncPad); document.addEventListener("visibilitychange", syncPad);

  // ------------------------------------------------------------------ folder found: a check mark that draws itself, a pop, a few sparks
  const SVGNS = "http://www.w3.org/2000/svg";
  Feel.found = (val, isNew, ok) => {
    const row = val.closest(".row");
    if (!row) return;
    let c = row.querySelector(".chk");
    if (!ok) { if (c) c.remove(); return; }
    if (c && !isNew) return;
    if (c) c.remove();
    c = document.createElementNS(SVGNS, "svg");
    c.setAttribute("viewBox", "0 0 20 20"); c.setAttribute("class", "chk");
    const p = document.createElementNS(SVGNS, "path");
    p.setAttribute("d", "M4 10.5 L8.4 15 L16 5.5");
    c.appendChild(p);
    row.insertBefore(c, val);
    if (still) return;
    c.classList.add("draw");
    val.classList.remove("pop"); void val.offsetWidth; val.classList.add("pop");
    val.addEventListener("animationend", () => val.classList.remove("pop"), { once: true });
    if (calm) return;
    const cx = c.offsetLeft + 9, cy = c.offsetTop + 9;
    for (let i = 0; i < 7; i++) {
      const s = document.createElement("i"), a = i / 7 * 6.283 + Math.random() * 0.6, d = 14 + Math.random() * 14;
      s.className = "spk"; s.style.left = cx + "px"; s.style.top = cy + "px";
      s.style.setProperty("--dx", (Math.cos(a) * d).toFixed(0) + "px"); s.style.setProperty("--dy", (Math.sin(a) * d).toFixed(0) + "px");
      s.addEventListener("animationend", () => s.remove(), { once: true });
      row.appendChild(s);
    }
  };

  // ------------------------------------------------------------------ the title card (done + a solo launcher) and the Play button
  let cardOn = false;
  Feel.card = (on) => {
    if (on === cardOn) return;
    cardOn = on;
    const c = $("card"), b = $("b-play");
    c.hidden = !on;
    if (on) {
      c.classList.remove("in"); void c.offsetWidth;
      if (!still) { c.classList.add("in"); setTimeout(() => c.classList.remove("in"), 1600); }
      if (usedKeys && (!document.activeElement || document.activeElement === document.body)) b.focus({ preventScroll: true });
    } else { b.disabled = false; b.textContent = "Play"; $("play-q").hidden = true; }
  };
  let playTimer = 0;
  $("b-play").addEventListener("click", async () => {
    const b = $("b-play");
    if (b.disabled) return;
    b.disabled = true; b.textContent = "Starting the game...";
    let r = {};
    try { r = await api("/api/play", {}); } catch (e) { r = { error: "The installer did not answer. Start the game from the folder instead." }; }
    if (r && r.error) { b.disabled = false; b.textContent = "Play"; alert(r.error); return; }
    $("play-q").hidden = false;
    clearTimeout(playTimer);
    playTimer = setTimeout(() => { b.disabled = false; b.textContent = "Play"; }, 15000);
  });

  // ------------------------------------------------------------------ sheen across the panel (setup / credits only, one-shot every ~12 s)
  const sheen = $("sheen");
  sheen.addEventListener("animationend", () => sheen.classList.remove("go"));
  setInterval(() => {
    if (still || calm || !inFront() || view === "run") return;
    sheen.classList.remove("go"); void sheen.offsetWidth; sheen.classList.add("go");
  }, 12000);

  // ------------------------------------------------------------------ "while you wait": what is being built + tips
  const TIPS = [
    "You start as Roxas. Play on and you become Sora.",
    "Drive Forms: Valor and Wisdom first. More unlock as you free the Divine Beasts.",
    "Summons are tied to your journey: Chicken Little comes with the paraglider, Genie with Vah Naboris, Peter Pan with Vah Medoh.",
    "Fall in a boss fight and King Mickey may step in to save you.",
    "There is a skateboard waiting in front of every Great Plateau shrine.",
    "The Keyblade can chop down trees.",
    "Use Stasis on an object, hit it with the Keyblade, and watch it fly.",
    "Up to four friends can play together. Everybody needs the very same Legend of Kairi release.",
    "Next time, 'Update Legend of Kairi.bat' fetches new releases for you.",
    "Something broken? 'Make bug report.bat' packs up what tvllerr needs to fix it.",
    // (s185, user) more of the cool things the mod does
    "Every Keyblade can be unlocked, each in its own way, as you make your way through Hyrule.",
    "Enemies drop HP, MP and Drive orbs, just like in Kingdom Hearts II.",
    "Sora levels up with EXP and learns his Kingdom Hearts II abilities along the way.",
    "Donald and Goofy fight at your side, and glide after you when you take to the air.",
    "Limits are here: Whirli-Goof and Comet with Goofy and Donald, Duck Flare with Donald.",
    "Call Peter Pan and Tinker Bell lights your way and heals you.",
    "Chicken Little brings FPS Mode: aim and fire for yourself.",
    "The Equipment and Customize menus look and work like Kingdom Hearts II.",
    "Sora and Roxas can pick up and swing Hyrule's own weapons too.",
    "Damage follows Kingdom Hearts II's real formula: your Strength against the enemy's Defense.",
    "In multiplayer everybody fights the same enemies: their health is shared between your games.",
    "Friends see it all: your Drive Forms, your outfits, even you on a skateboard.",
    "King Mickey can swim, climb and take a hit, just like Sora.",
    "The menu sounds you hear in this installer come from your own copy of Kingdom Hearts II.",
  ];
  let tipI = Math.floor(Math.random() * TIPS.length), waitOn = false, tipTimer = 0, nowShown = "";
  const T = (s) => s.replace(/\b[a-z]/g, (c) => c.toUpperCase());
  const who = (n) => {
    const m = /^form_(.+)$/.exec(n), x = T(PRETTY(m ? m[1] : n));
    return m && !/form$/i.test(x) ? x + " Form" : x;
  };
  function nowText() {
    const np = nowPhase();
    if (!np) { const p = S.phases.find((q) => q.state !== "done"); return p ? p.label : "Almost there"; }
    const r0 = np.running && np.running[0] ? np.running[0] : "";
    const n = r0 && !/^(fxrec|record|verify|copy|game)/i.test(r0) ? who(r0) : "";   // (s185) a job's own step name is not a thing being built ("Fxrec Record")
    switch (np.key) {
      case "copy": return "Copying the program files";
      case "kh2": return "Unpacking your KH2 files";
      case "bakes": return n ? "Baking " + n : "Baking the characters";
      case "fxrec": return n ? "Recording the effects of " + n : "Recording the effects";
      case "fx": return "Building the effect packs";
      case "other": return "Making the HUD and the sounds";
      case "verify": return "Checking every file";
      case "game": return "Setting up the game";
      default: return np.label;
    }
  }
  function layoutWait() {
    const w = $("wait");
    if (w.hidden && !waitOn) return;
    const pr = $("panel").getBoundingClientRect(), rx = 150 * (innerHeight / 760);
    const platTop = innerHeight * 0.62 - rx * 0.38 - 16;            // the face of the platform (app.js: PY, PR, SQ), plus a gap
    const left0 = pr.right + 16, avail = innerWidth - 16 - left0, width = Math.min(300, avail);
    if (width < 200) { w.hidden = true; return; }
    w.hidden = false;
    w.style.width = width + "px";
    const x = Math.min(innerWidth - 16 - width, Math.max(left0, innerWidth * 0.83 - width / 2));
    const h = w.offsetHeight, top = platTop - h;
    if (top < 10) { w.hidden = true; return; }                       // too short a window: it would cover the platform
    w.style.left = Math.round(x) + "px"; w.style.top = Math.round(top) + "px";
  }
  function setTip(i, fade) {
    const t = $("wtip");
    const put = () => { t.textContent = TIPS[i]; t.classList.remove("fade"); };
    if (!fade || still) { put(); return; }
    t.classList.add("fade");
    setTimeout(put, 360);
  }
  function nextTip(fade) { tipI = (tipI + 1) % TIPS.length; setTip(tipI, fade); }
  $("wait").addEventListener("click", () => nextTip(true));
  function updateWait() {
    const w = $("wait"), run = !!S && (S.status === "running" || S.status === "verifying");
    if (!run) { waitOn = false; w.hidden = true; w.classList.remove("show"); clearInterval(tipTimer); tipTimer = 0; nowShown = ""; return; }
    if (!waitOn) {
      waitOn = true; setTip(tipI, false);
      if (!still) { w.classList.remove("show"); void w.offsetWidth; w.classList.add("show"); }
      tipTimer = setInterval(() => { if (waitOn && inFront()) nextTip(true); }, 10000);
    }
    const txt = nowText();
    if (txt !== nowShown) { nowShown = txt; $("wnow").textContent = txt; }
    layoutWait();
  }

  Feel.onRender = () => { placePtr(false); updateWait(); };
  Feel.layout = () => { placePtr(true); layoutWait(); };

  // ------------------------------------------------------------------ canvas effects: shooting stars, sparkle bursts, rings, the done shards
  const RATES = [0.5, 0.56, 0.63, 0.75, 0.84, 1, 1.12, 1.26, 1.5];     // a pentatonic-ish ladder for the star chimes
  let shots = [], puffs = [], rings = [], shards = [], shardEnd = -1, shardT0 = 0, landed = true;
  const MAXP = 150;

  function burst(x, y, n, sp) {
    for (let i = 0; i < n && puffs.length < MAXP; i++) {
      const a = Math.random() * 6.283, v = sp * (0.4 + Math.random() * 0.8);
      puffs.push({ x, y, vx: Math.cos(a) * v, vy: Math.sin(a) * v - 10, t0: nowS(), life: 0, max: 0.6 + Math.random() * 0.6, r: 2 + Math.random() * 2.5 });
    }
  }
  function chime(rate) { try { if (window.kairiSfx) kairiSfx.play("move", rate); } catch (e) { /* silence */ } }
  $("bg").addEventListener("pointerdown", (e) => {
    if (e.button || !S) return;
    const x = e.clientX, y = e.clientY, t = nowS(), mk = MK();
    const fx = !still && !calm;                                       // Calm: sound + pulse only; reduced motion: sound only
    if (Math.hypot(x - MX(), y - (MY() + 2 * mk)) < 20 * mk) {       // the heart moon: one halo pulse
      if (!still) moonPulseAt = t;
      chime(0.6); return;
    }
    let best = null, bi = -1, bd = 22;                                // a star: generous hit radius
    for (let i = 0; i < stars.length; i++) { const d = Math.hypot(stars[i].x + 1 - x, stars[i].y + 1 - y); if (d < bd) { bd = d; best = stars[i]; bi = i; } }
    if (best) {
      chime(RATES[Math.floor(best.p / 6.2832 * 9) % 9]);
      if (fx) {
        stars.splice(bi, 1);
        const a = -0.25 * Math.PI - Math.random() * 0.5, v = 420 + Math.random() * 160;
        shots.push({ x: best.x, y: best.y, vx: Math.cos(a) * v * (Math.random() < 0.5 ? 1 : -1), vy: Math.sin(a) * v * 0.6 + 80, t0: t, life: 0, max: 0.75 });
        let nx, ny, tries = 0;
        do { nx = Math.random() * W; ny = Math.random() * H * 0.62; } while (Math.hypot((nx - MX()) / 1.1, ny - MY() - 4 * mk) < 24 * mk && ++tries < 6);
        stars.push({ x: nx, y: ny, r: 0.4 + Math.random() * 1.1, p: Math.random() * 6.28, s: 0.6 + Math.random() * 1.4, born: t + 0.5 });
      }
      return;
    }
    const cx = KX(), cy = PY(), rx = PR() * 1.05, ry = rx * SQ;       // the platform: a ring of light across the glass + risers
    if (((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1) {
      chime(0.84);
      if (fx || calm) rings.push({ x: cx, y: cy, t0: t });
      if (fx) for (let i = 0; i < 8 && risers.length < 90; i++) {
        const a = Math.random() * 6.283, r = Math.sqrt(Math.random()) * PR() * 0.85;
        risers.push({ x: cx + Math.cos(a) * r, y: cy + Math.sin(a) * r * SQ, vy: 25 + Math.random() * 45, life: 0, max: 1.2 + Math.random() * 1.6, p: Math.random() * 6.28, k: 0.7, a: 0.5 });
      }
      return;
    }
    if (fx) burst(x, y, 10, 70);                                       // empty sky: a small sparkle burst
  });

  // the 40 glass shards of the finish: pre-drawn once (a few colours), then only drawImage per frame
  let shardSprites = null;
  function makeShards() {
    const cols = [["#8fc2ff", "#2a62c8"], ["#4f9dff", "#173f94"], ["#ffe9a8", "#d9a232"], ["#ffb3d3", "#d9437f"], ["#ffffff", "#aac8f5"]];
    return cols.map(([c0, c1], k) => {
      const c = document.createElement("canvas"), n = 40;
      c.width = c.height = n;
      const x = c.getContext("2d"), j = (k * 37 % 7) / 7;
      x.beginPath(); x.moveTo(6 + j * 6, 34); x.lineTo(20, 4 + j * 5); x.lineTo(34 - j * 4, 30); x.lineTo(21, 36 - j * 3); x.closePath();
      const gr = x.createLinearGradient(6, 4, 34, 36);
      gr.addColorStop(0, c0); gr.addColorStop(1, c1);
      x.fillStyle = gr; x.fill(); x.lineJoin = "round"; x.strokeStyle = "#0a0f22"; x.lineWidth = 3; x.stroke();
      x.strokeStyle = "rgba(255,255,255,0.55)"; x.lineWidth = 1.2; x.beginPath(); x.moveTo(10, 30); x.lineTo(20, 9); x.stroke();
      return c;
    });
  }
  Feel.shards = (t, dur) => {
    if (!shardSprites) shardSprites = makeShards();
    const s = H / 760, cx = KX(), cy = PY(), rx = PR();
    shards = [];
    for (let i = 0; i < 40; i++) {
      const side = i % 4, e = Math.random();
      const sx = side === 0 ? -30 : side === 1 ? W + 30 : e * W, sy = side === 2 ? -30 : side === 3 ? H * 0.5 : e * H * 0.6;
      const a = Math.random() * 6.283, r = Math.sqrt(Math.random()) * rx * 0.85, delay = Math.random() * dur * 0.2;
      shards.push({ sx, sy, tx: cx + Math.cos(a) * r, ty: cy + Math.sin(a) * r * SQ, d: delay, fl: dur - delay, sp: shardSprites[i % shardSprites.length],
        size: (22 + Math.random() * 22) * Math.max(0.7, s), rot: Math.random() * 6.28, spin: (Math.random() - 0.5) * 14 });
    }
    shardT0 = t; shardEnd = t + dur; landed = false;
  };

  Feel.sky = (t, dt) => {            // shooting stars (behind the hills)
    if (calm) { shots.length = 0; return; }
    if (!shots.length) return;
    g.save(); g.globalCompositeOperation = "lighter";
    for (const p of shots) {
      p.life = t - p.t0; p.x += p.vx * dt; p.y += p.vy * dt;
      const u = p.life / p.max;
      for (let k = 0; k < 8; k++) {                                    // the streak = a few sprites along the path behind it
        const f = k * 0.018;
        g.globalAlpha = Math.max(0, (1 - u) * (1 - k / 8));
        const r = 7 - k * 0.55;
        g.drawImage(layers.blue, p.x - p.vx * f - r, p.y - p.vy * f - r, r * 2, r * 2);
      }
    }
    g.restore();
    shots = shots.filter((p) => p.life < p.max);
  };

  Feel.over = (t, dt) => {           // in front of the platform: shards, sparkle bursts, rings
    if (calm && !rings.length) { puffs.length = 0; shards.length = 0; if (shardEnd > 0) landed = true; return; }
    if (!still && shards.length) {
      for (const p of shards) {
        const u = (t - shardT0 - p.d) / p.fl;
        if (u < 0) continue;
        if (u >= 1) { p.done = true; if (puffs.length < MAXP) puffs.push({ x: p.tx, y: p.ty, vx: 0, vy: -12, t0: t, life: 0, max: 0.45, r: 6, a: 0.4 }); continue; }
        const k = u ** 1.7, x = p.sx + (p.tx - p.sx) * k, y = p.sy + (p.ty - p.sy) * k, sz = p.size * (1 - 0.45 * k);
        g.save(); g.translate(x, y); g.rotate(p.rot + p.spin * (1 - k) * 0.5);
        g.drawImage(p.sp, -sz / 2, -sz / 2, sz, sz);
        g.restore();
      }
      shards = shards.filter((p) => !p.done);
    }
    if (!landed && t >= shardEnd) {   // all landed: a ring of light and a burst from the glass
      landed = true;
      rings.push({ x: KX(), y: PY(), t0: t });
      for (let i = 0; i < 14 && risers.length < 90; i++) {
        const a = Math.random() * 6.283, r = Math.sqrt(Math.random()) * PR() * 0.9;
        risers.push({ x: KX() + Math.cos(a) * r, y: PY() + Math.sin(a) * r * SQ, vy: 25 + Math.random() * 55, life: 0, max: 1.2 + Math.random() * 1.8, p: Math.random() * 6.28, k: 0.7, a: 0.5 });
      }
    }
    if (puffs.length) {
      g.save(); g.globalCompositeOperation = "lighter";
      for (const p of puffs) {
        p.life = t - p.t0; p.x += p.vx * dt; p.y += p.vy * dt; p.vx *= 0.97; p.vy *= 0.97;   // life from the clock: slow frames never stretch it
        g.globalAlpha = Math.max(0, 1 - p.life / p.max) * (p.a || 1);
        g.drawImage(layers.gold, p.x - p.r * 3, p.y - p.r * 3, p.r * 6, p.r * 6);
      }
      g.restore();
      puffs = puffs.filter((p) => p.life < p.max);
    }
    if (rings.length) {
      g.save(); g.globalCompositeOperation = "lighter"; g.lineWidth = 3 * (H / 760);
      for (const r of rings) {
        const u = (t - r.t0) / 0.9;
        if (u >= 1) continue;
        const k = easeOut(Math.max(0, u)), rx = PR() * 1.02 * k;
        g.strokeStyle = `rgba(255,232,170,${(0.85 * (1 - u)).toFixed(3)})`;
        g.beginPath(); g.ellipse(r.x, r.y, Math.max(1, rx), Math.max(1, rx * SQ), 0, 0, 6.2832); g.stroke();
      }
      g.restore();
      rings = rings.filter((r) => t - r.t0 < 0.9);
    }
  };
  return Feel;
})();
