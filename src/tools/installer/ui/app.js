"use strict";
// Legend of Kairi installer page (Sora in BotW, s168). Talks to installer.py on 127.0.0.1 (token from the URL).
const TOKEN = new URLSearchParams(location.search).get("t") || "";
const $ = (id) => document.getElementById(id);
let S = null, view = "setup", seq = 0, logOpen = false, helpSel = "h-sw";

async function api(path, body) {
  const r = await fetch(path, body === undefined ? { headers: { "X-Token": TOKEN } } :
    { method: "POST", headers: { "X-Token": TOKEN, "Content-Type": "application/json" }, body: JSON.stringify(body) });
  return r.json();
}

// ------------------------------------------------------------------ state polling
async function poll() {
  try {
    const st = await api("/api/state?since=" + seq);
    if (st.lines && st.lines.length) {
      const log = $("log");
      const stick = log.scrollTop + log.clientHeight >= log.scrollHeight - 4;
      log.textContent = (log.textContent + st.lines.join("\n") + "\n").split("\n").slice(-500).join("\n");
      if (stick) log.scrollTop = log.scrollHeight;
    }
    seq = st.seq;
    const was = S && S.status, busy = ["running", "verifying"];
    S = st;
    // an install started any other way (API call, resume) must show the progress screen too
    if (busy.includes(st.status) && (!was || !busy.includes(was)) && view !== "run") view = "run";
    render();
  } catch (e) { /* server gone: window closing */ }
  setTimeout(poll, 700);
}

const short = (p) => p.length > 34 ? "..." + p.slice(-33) : p;
const mmss = (s) => { s = Math.max(0, Math.round(s)); const m = Math.floor(s / 60); return m + ":" + String(s % 60).padStart(2, "0"); };

function render() {
  if (!S) return;
  $("ver").textContent = "Version " + S.version;
  for (const id of ["s-setup", "s-run", "s-credits"]) $(id).hidden = id !== "s-" + view;
  document.querySelectorAll(".swv").forEach((e) => (e.textContent = S.sw_version));
  $("root-path").textContent = S.root;
  if (view === "setup") renderSetup();
  if (view === "run") renderRun();
}

function setVal(el, st, item, autoText) {
  el.classList.remove("ok", "bad");
  st.classList.remove("ok", "bad");
  if (!item.path) { el.textContent = "Choose..."; st.textContent = ""; return; }
  el.textContent = item.ok ? (item.auto ? autoText : short(item.path)) : "Check this";
  el.classList.add(item.ok ? "ok" : "bad");
  st.textContent = (item.ok ? "✓ " : "") + item.msg + (item.ok ? "  " + item.path : "");
  st.classList.add(item.ok ? "ok" : "bad");
}

function renderSetup() {
  setVal($("v-sw"), $("st-sw"), S.sw, "");
  setVal($("v-kh2"), $("st-kh2"), S.kh2, "Found (Steam)");
  const ready = S.sw.ok && S.kh2.ok && S.disk.ok;
  $("r-go").disabled = !ready;
  $("go-lbl").textContent = S.resumable ? "Continue the install" : "Install Legend of Kairi";
  $("v-go").textContent = ready ? (S.resumable ? "Continue" : "About 15 min") : "";
  const d = S.disk, st = $("st-go");
  st.classList.remove("ok", "bad");
  st.textContent = d.free === null ? "" : `Needs about ${d.need} GB free on that drive while it works (${d.free} GB free now).`;
  if (d.free !== null) st.classList.add(d.ok ? "ok" : "bad");
  document.querySelectorAll(".hp").forEach((h) => h.classList.toggle("on", h.id === helpSel));
  document.querySelectorAll(".rows .row").forEach((r) => r.classList.toggle("sel", r.dataset.help === helpSel));
}

const SHORT = { kh2: "KH2 files", bakes: "Characters", fxrec: "Effects", fx: "Effect packs", other: "HUD + sounds", verify: "Checking files", game: "Game setup" };
const pct = (f) => Math.round(Math.min(1, Math.max(0, f)) * 100);
function nowPhase() {   // the running phase that is furthest behind (several run at once), else the first running one
  const run = S.phases.filter((p) => p.state === "run");
  if (!run.length) return null;
  return run.reduce((a, b) => (b.frac < a.frac ? b : a), run[0]);
}

const PRETTY = (n) => n.replace(/^(form|summon|party|fxsfx|fx|sfx|hud)_/, "").replace(/_/g, " ");

function renderRun() {
  const s = S.status;
  const running = s === "running" || s === "verifying";
  $("run-title").textContent = { running: "Installing Legend of Kairi", verifying: "Checking every file", done: "Legend of Kairi is ready",
    failed: "The install stopped", cancelled: "Install stopped", setup: "Installing Legend of Kairi" }[s];
  document.title = running ? "Legend of Kairi - installing" : "Legend of Kairi";   // the window program asks before closing while installing
  $("run-lead").textContent = s === "done"
    ? `Every file matches the reference build (build id ${S.build_id}). It took ${mmss(S.elapsed)}. The mod's files are in ${S.root}/data.`
    : s === "failed" ? "Something went wrong. The details are below; the logs folder has everything to send to tvllerr."
    : s === "cancelled" ? "Nothing is lost: Install again and it carries on where it stopped."
    : "You can keep using your PC. If you close this window the install stops, and the next time it carries on where it stopped.";
  $("all-fill").style.width = (s === "done" ? 100 : S.frac * 100).toFixed(1) + "%";
  const np = nowPhase(), all = s === "done" ? 100 : pct(S.frac);
  $("now").textContent = s === "done" ? "Done  ·  100%" : running ? (np ? `${SHORT[np.key] || np.label} ${pct(np.frac)}%  ·  overall ${all}%` : `Overall ${all}%`) : `Overall ${all}%`;
  $("t-el").textContent = mmss(S.elapsed);
  $("t-eta").textContent = running ? (S.eta == null ? "working out the time left..." : "about " + Math.max(1, Math.round(S.eta / 60)) + " min left") : "";
  const ol = $("phases");
  ol.innerHTML = "";
  for (const p of S.phases) {
    const li = document.createElement("li");
    li.className = p.state;
    const sub = p.state === "run" && p.running.length ? `<span class="sub">${p.running.slice(0, 4).map(PRETTY).join(", ")}</span>` : "";
    li.innerHTML = `<span class="ic"></span><span>${p.label}${sub}</span><div class="pb"><div class="bar"><div class="fill" style="width:${(p.frac * 100).toFixed(1)}%"></div></div><span class="pc">${p.state === "done" ? 100 : pct(p.frac)}%</span></div>`;
    ol.appendChild(li);
  }
  const game = S.phases.find((p) => p.key === "game");
  if (s === "verifying" && game && game.state === "run") $("run-title").textContent = "Setting up the game";
  const L = S.launchers || {};
  $("play").hidden = !(s === "done" && L.solo);
  $("b-folder").hidden = !(s === "done" && L.solo);
  if (s === "done" && L.solo) {
    $("play-note").textContent = `They are in ${S.root} .` + (L.peers_set ? " Your friend's address is already set." : "");
  }
  $("err").hidden = !S.error;
  $("err").textContent = S.error;
  $("b-cancel").hidden = !running;
  $("b-back").hidden = running;
}

// ------------------------------------------------------------------ events
async function pick(what) {
  const r = await api("/api/pick", { what });
  if (r.error) alert(r.error);
  helpSel = what === "sw" ? "h-sw" : "h-kh2";
  poll1();
}
async function poll1() { const st = await api("/api/state?since=" + seq); seq = st.seq; S = st; render(); }

$("r-sw").onclick = () => pick("sw");
$("r-kh2").onclick = () => pick("kh2");
$("r-go").onclick = async () => {
  const r = await api("/api/install", {});
  if (r.error) { alert(r.error); return; }
  view = "run"; $("log").textContent = ""; poll1();
};
$("r-credits").onclick = () => { view = "credits"; loadCredits(); render(); };
$("b-cback").onclick = () => { view = "setup"; render(); };
$("b-back").onclick = () => { view = "setup"; render(); };
$("b-cancel").onclick = async () => { if (confirm("Stop the install? It carries on where it stopped next time.")) { await api("/api/cancel", {}); poll1(); } };
$("b-log").onclick = () => { logOpen = !logOpen; $("log").hidden = !logOpen; $("b-log").textContent = logOpen ? "Hide details" : "Show details"; };
$("b-folder").onclick = () => api("/api/open", { what: "root" });
$("b-logs").onclick = () => api("/api/open", { what: "log" });
$("b-lic").onclick = () => api("/api/open", { what: "licenses" });
document.querySelectorAll(".rows .row").forEach((r) => {
  const sel = () => { helpSel = r.dataset.help; if (S && view === "setup") renderSetup(); };
  r.addEventListener("mouseenter", sel);
  r.addEventListener("focus", sel);
});
window.addEventListener("beforeunload", (e) => { if (S && ["running", "verifying"].includes(S.status)) { e.preventDefault(); e.returnValue = ""; } });

let creditsLoaded = false;
async function loadCredits() {
  if (creditsLoaded) return;
  const c = await (await fetch("credits.json")).json();
  const box = $("credits");
  const esc = (t) => t.replace(/[&<>]/g, (ch) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;" }[ch]));
  for (const p of c.people) box.insertAdjacentHTML("beforeend", `<div class="c wide"><div class="n">${esc(p.name)}</div><div class="r">${esc(p.role)}</div></div>`);
  for (const t of c.tools) box.insertAdjacentHTML("beforeend", `<div class="c"><div class="n">${esc(t.name)}</div><div class="r">${esc(t.role)}</div><div class="l">${esc(t.license)}</div></div>`);
  box.insertAdjacentHTML("beforeend", `<div class="legal">${esc(c.legal)}</div>`);
  creditsLoaded = true;
}

// ------------------------------------------------------------------ background: night sky, light, falling sparkles, grass, the platform
// (s168) light on the PC: at most 30 frames a second on the menu and 20 while installing, nothing at all while the window is hidden;
// everything that does not move is drawn once into a layer (sky + moon, light beams, hills, the platform's pillar) and the glows are
// one pre-drawn sprite instead of a new gradient per sparkle per frame.
const FPS_MENU = 24, FPS_BUSY = 20, FPS_AWAY = 10, FPS_AWAY_BUSY = 8;   // AWAY = the window is not in front
const RS = 0.75;  // canvas pixels per CSS pixel: soft art scaled up looks the same at ~half the pixels (0.5 made the glass blocky)
const cv = $("bg"), g = cv.getContext("2d", { alpha: false });
const still = matchMedia("(prefers-reduced-motion: reduce)").matches;
let W = 0, H = 0, DPR = 1, stars = [], sparks = [], grass = [], hills = [], layers = {};
const rnd = (a, b) => a + Math.random() * (b - a);

function setup() {
  DPR = 1;   // soft background art: 1 canvas pixel per CSS pixel is plenty (and 2-4x less work on high-DPI screens)
  W = innerWidth; H = innerHeight;
  cv.width = Math.ceil(W * RS); cv.height = Math.ceil(H * RS);
  g.setTransform(RS, 0, 0, RS, 0, 0);
  stars = Array.from({ length: Math.round(W * H / 5200) }, () => ({ x: rnd(0, W), y: rnd(0, H * 0.62), r: rnd(0.4, 1.5), p: rnd(0, 6.28), s: rnd(0.6, 2) }))
    .filter((st) => Math.hypot((st.x - MX()) / 1.1, st.y - MY() - 4 * MK()) > 24 * MK());   // none in front of the moon
  sparks = Array.from({ length: 70 }, () => newSpark(true));
  grass = [];
  const layers = [{ n: W / 4.2, h: [0.07, 0.15], c: "#0f2d55", y: 0.925 }, { n: W / 3.4, h: [0.06, 0.13], c: "#0a2142", y: 0.955 },
                  { n: W / 2.8, h: [0.05, 0.11], c: "#040c1f", y: 1.0 }];
  layers.forEach((L, li) => {
    const blades = [];
    for (let i = 0; i < L.n; i++) blades.push({ x: rnd(-10, W + 10), h: rnd(L.h[0], L.h[1]) * H, w: rnd(2, 4.5), p: rnd(0, 6.28), lean: rnd(-0.25, 0.25) });
    grass.push({ ...L, li, blades });
  });
  hills = [0, 1].map((i) => ({ y: H * (0.74 + i * 0.06), a: H * (0.035 - i * 0.01), f: rnd(1.5, 2.6), p: rnd(0, 6), c: i ? "#0a1d3e" : "#0c2147" }));
  buildLayers();
}

// a W x H canvas at scale k (static art is soft: 1x is plenty) drawn by fn(ctx)
function layer(fn, k = RS) {
  const c = document.createElement("canvas");
  c.width = Math.ceil(W * k); c.height = Math.ceil(H * k);
  const x = c.getContext("2d");
  x.scale(k, k);
  fn(x);
  return c;
}
function sprite(rgb) {
  const c = document.createElement("canvas"), n = 32;
  c.width = c.height = n;
  const x = c.getContext("2d"), gr = x.createRadialGradient(n / 2, n / 2, 0, n / 2, n / 2, n / 2);
  gr.addColorStop(0, `rgba(${rgb},1)`); gr.addColorStop(1, `rgba(${rgb},0)`);
  x.fillStyle = gr; x.fillRect(0, 0, n, n);
  return c;
}
function buildLayers() {
  layers.sky = layer((x) => {
    const sky = x.createLinearGradient(0, 0, 0, H);
    sky.addColorStop(0, "#030818"); sky.addColorStop(0.45, "#0a1d48"); sky.addColorStop(0.8, "#173a7c"); sky.addColorStop(1, "#0b1c3c");
    x.fillStyle = sky; x.fillRect(0, 0, W, H);
    drawMoon(x);
  });
  layers.beams = layer((x) => {
    x.globalCompositeOperation = "lighter";
    const kx = KX(), gy = GY();
    for (let i = 0; i < 4; i++) {
      const top = kx - W * (0.18 - i * 0.07), w = W * (0.035 + i * 0.012), a = 0.06;
      const lg = x.createLinearGradient(0, 0, 0, gy);
      lg.addColorStop(0, `rgba(170,200,255,${a})`); lg.addColorStop(1, `rgba(255,220,140,${a * 0.7})`);
      x.fillStyle = lg;
      x.beginPath(); x.moveTo(top, 0); x.lineTo(top + w, 0); x.lineTo(kx + w * 0.9 + i * 6, gy); x.lineTo(kx - w * 0.6 + i * 6, gy); x.closePath(); x.fill();
    }
  });
  layers.back = layer((x) => {     // sky + moon glow + beams + hills in one picture: one blit per frame
    x.drawImage(layers.sky, 0, 0, W, H);
    x.globalCompositeOperation = "lighter";
    x.drawImage(layers.beams, 0, 0, W, H);
    x.globalCompositeOperation = "source-over";
    drawHillsTo(x);
  });
  layers.sky = layers.beams = null;
  layers.hills = null;
  function drawHillsTo(x) {
    for (const h of hills) {
      x.fillStyle = h.c; x.beginPath(); x.moveTo(0, H);
      for (let xx = 0; xx <= W + 20; xx += 20) x.lineTo(xx, h.y - Math.sin(xx / W * h.f * 3.14 + h.p) * h.a - Math.sin(xx / W * 9 + h.p) * h.a * 0.25);
      x.lineTo(W, H); x.closePath(); x.fill();
    }
  }
  layers.pillar = layer(drawPillar);
  layers.gold = sprite("255,225,140");
  layers.blue = sprite("210,230,255");
}

function newSpark(anywhere) {
  return { x: anywhere ? rnd(0, W) : rnd(-0.2 * W, W * 0.9), y: anywhere ? rnd(0, H) : rnd(-40, -5), vx: rnd(4, 14), vy: rnd(10, 26),
           r: rnd(0.8, 2.2), p: rnd(0, 6.28), life: 0 };
}

const KX = () => W * 0.83, GY = () => H * 0.86;

// ---- the heart-shaped moon (drawn by us: a soft pale-gold heart with cloudy patches and a warm glow), above the title
const MX = () => W * 0.2, MY = () => H * 0.085, MK = () => H * 0.0042;   // centre + size (1 heart unit; the heart is 32 units wide)
function heartPath(x, cx, cy, k) {
  x.beginPath();
  for (let i = 0; i <= 120; i++) {
    const t = i / 120 * Math.PI * 2;
    const hx = 16 * Math.sin(t) ** 3, hy = 13 * Math.cos(t) - 5 * Math.cos(2 * t) - 2 * Math.cos(3 * t) - Math.cos(4 * t);
    const px = cx + hx * k, py = cy - hy * k + 2 * k;
    if (i) x.lineTo(px, py); else x.moveTo(px, py);
  }
  x.closePath();
}
function drawMoon(x) {
  const cx = MX(), cy = MY(), k = MK();
  const rg = x.createRadialGradient(cx, cy + 4 * k, 0, cx, cy + 4 * k, k * 75);       // the sky lit around it
  rg.addColorStop(0, "rgba(255,236,185,0.30)"); rg.addColorStop(0.22, "rgba(190,190,190,0.10)"); rg.addColorStop(1, "rgba(0,0,0,0)");
  x.fillStyle = rg; x.fillRect(0, 0, W, H);
  x.save();                                                                              // soft bright rim
  x.shadowColor = "rgba(255,238,190,0.95)"; x.shadowBlur = k * 9 * RS;   // (shadowBlur ignores the layer scale)
  heartPath(x, cx, cy, k); x.fillStyle = "#fbefc8"; x.fill();
  x.restore();
  heartPath(x, cx, cy, k);
  const bg = x.createRadialGradient(cx - 3 * k, cy - 2 * k, 2 * k, cx, cy, 19 * k);
  bg.addColorStop(0, "#fffbea"); bg.addColorStop(0.6, "#f6e6b6"); bg.addColorStop(1, "#e6cc8c");
  x.fillStyle = bg; x.fill();
  x.save();                                                                              // cloudy patches (same every time)
  heartPath(x, cx, cy, k); x.clip();
  let seed = 7;
  const R = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
  for (let i = 0; i < 36; i++) {
    const bx = cx + (R() * 32 - 16) * k, by = cy + (R() * 30 - 12) * k, br = k * (2 + R() * 6.5), a = 0.07 + R() * 0.17;
    const cg = x.createRadialGradient(bx, by, 0, bx, by, br);
    cg.addColorStop(0, `rgba(160,128,80,${a.toFixed(3)})`); cg.addColorStop(1, "rgba(160,128,80,0)");
    x.fillStyle = cg; x.fillRect(bx - br, by - br, br * 2, br * 2);
  }
  x.restore();
}

function drawSky(t) {
  g.drawImage(layers.back, 0, 0, W, H);
  g.fillStyle = "#dce8ff";
  for (const st of stars) {
    g.globalAlpha = 0.35 + 0.65 * (0.5 + 0.5 * Math.sin(t * st.s + st.p));
    g.fillRect(st.x, st.y, st.r, st.r);
  }
  g.globalAlpha = 1;
}

// ---- stained-glass platform (our own pattern: heart medallion, flowers, waves, stars; no game art)
let glass = null, risers = [];
const PR = () => 150 * (H / 760);          // platform radius (screen px)
const PY = () => H * 0.62;                 // top face centre y
const SQ = 0.38;                           // ellipse squash (seen from a little above)

function star(c, x, y, r1, r2, n, rot) {
  c.beginPath();
  for (let i = 0; i < n * 2; i++) {
    const r = i % 2 ? r2 : r1, a = rot + i * Math.PI / n;
    c.lineTo(x + Math.cos(a) * r, y + Math.sin(a) * r);
  }
  c.closePath();
}

function makeGlass() {
  const R = 256, c = document.createElement("canvas");
  c.width = c.height = R * 2;
  const x = c.getContext("2d"), TAU = Math.PI * 2, lead = "#0a0f22";
  x.translate(R, R);
  x.lineJoin = "round";
  const ring = (r0, r1, n, colors, off = 0) => {
    for (let i = 0; i < n; i++) {
      const a0 = off + i * TAU / n, a1 = a0 + TAU / n;
      x.beginPath(); x.arc(0, 0, r1, a0, a1); x.arc(0, 0, r0, a1, a0, true); x.closePath();
      const gr = x.createRadialGradient(0, 0, r0, 0, 0, r1);
      const col = colors[i % colors.length];
      gr.addColorStop(0, col[0]); gr.addColorStop(1, col[1]);
      x.fillStyle = gr; x.fill();
      x.strokeStyle = lead; x.lineWidth = 6; x.stroke();
    }
  };
  // outer ring: deep / light blue panes, a gold star in each
  ring(0.84 * R, 0.99 * R, 16, [["#1f4fae", "#163a85"], ["#5f9cf0", "#3c76d6"]]);
  for (let i = 0; i < 16; i++) {
    const a = (i + 0.5) * TAU / 16, rr = 0.915 * R;
    star(x, Math.cos(a) * rr, Math.sin(a) * rr, 13, 5, 4, a);
    x.fillStyle = "#f6d06a"; x.fill(); x.strokeStyle = lead; x.lineWidth = 3; x.stroke();
  }
  // middle ring: night blue panes with a 5-petal flower (pink / white) each, wave arcs between
  ring(0.54 * R, 0.84 * R, 8, [["#183067", "#26489a"], ["#20397a", "#2f58b4"]], TAU / 16);
  for (let i = 0; i < 8; i++) {
    const a = i * TAU / 8 + TAU / 16 + TAU / 16, rr = 0.69 * R;
    const fx = Math.cos(a) * rr, fy = Math.sin(a) * rr;
    for (let k = 0; k < 5; k++) {
      const pa = a + k * TAU / 5;
      x.beginPath(); x.ellipse(fx + Math.cos(pa) * 17, fy + Math.sin(pa) * 17, 15, 9, pa, 0, TAU);
      x.fillStyle = i % 2 ? "#f6b9d6" : "#fff2fa"; x.fill(); x.strokeStyle = lead; x.lineWidth = 3; x.stroke();
    }
    x.beginPath(); x.arc(fx, fy, 8, 0, TAU); x.fillStyle = "#f6d06a"; x.fill(); x.stroke();
    // waves on the pane edge (towards the outside)
    const wa = a + TAU / 16;
    x.strokeStyle = "rgba(160,205,255,0.9)"; x.lineWidth = 4;
    for (let k = 0; k < 3; k++) {
      const r = (0.6 + k * 0.08) * R;
      x.beginPath(); x.arc(0, 0, r, wa - 0.12, wa + 0.12); x.stroke();
    }
  }
  // inner ring: gold and white rays
  ring(0.31 * R, 0.54 * R, 12, [["#ffe9a8", "#e9b44c"], ["#ffffff", "#cfe0ff"]]);
  // centre medallion: a heart on a sunburst, Kairi pink
  const cg = x.createRadialGradient(0, 0, 0, 0, 0, 0.31 * R);
  cg.addColorStop(0, "#fff8ea"); cg.addColorStop(1, "#f3c6dc");
  x.beginPath(); x.arc(0, 0, 0.31 * R, 0, TAU); x.fillStyle = cg; x.fill(); x.strokeStyle = lead; x.lineWidth = 7; x.stroke();
  star(x, 0, 0, 0.29 * R, 0.17 * R, 8, 0); x.fillStyle = "rgba(246,208,106,0.55)"; x.fill();
  const h = 0.16 * R;
  x.beginPath();
  x.moveTo(0, h * 0.95);
  x.bezierCurveTo(-h * 1.35, h * 0.05, -h * 0.95, -h * 1.05, 0, -h * 0.35);
  x.bezierCurveTo(h * 0.95, -h * 1.05, h * 1.35, h * 0.05, 0, h * 0.95);
  x.closePath();
  const hg = x.createLinearGradient(0, -h, 0, h);
  hg.addColorStop(0, "#ff8fc1"); hg.addColorStop(1, "#d9437f");
  x.fillStyle = hg; x.fill(); x.strokeStyle = lead; x.lineWidth = 6; x.stroke();
  // outer lead + gold trim
  x.beginPath(); x.arc(0, 0, 0.99 * R, 0, TAU); x.strokeStyle = lead; x.lineWidth = 8; x.stroke();
  return c;
}

function drawPillar(x) {
  const s = H / 760, cx = KX(), cy = PY(), rx = PR(), ry = rx * SQ, th = 22 * s, gy = GY();
  // pillar below the rim, fading into the mist above the grass
  const pg = x.createLinearGradient(0, cy + th, 0, gy);
  pg.addColorStop(0, "rgba(28,44,92,1)"); pg.addColorStop(0.55, "rgba(18,30,66,0.75)"); pg.addColorStop(1, "rgba(14,24,52,0)");
  x.fillStyle = pg;
  x.beginPath(); x.moveTo(cx - rx, cy + th); x.ellipse(cx, cy + th, rx, ry, 0, Math.PI, 0, true); x.lineTo(cx + rx, gy); x.lineTo(cx - rx, gy); x.closePath(); x.fill();
  x.strokeStyle = "rgba(120,160,240,0.10)"; x.lineWidth = 1.5 * s;
  for (let i = 1; i < 9; i++) {
    const xx = cx - rx + (2 * rx) * (0.5 - 0.5 * Math.cos(i * Math.PI / 9));
    x.beginPath(); x.moveTo(xx, cy + th + ry * Math.sin(Math.acos((xx - cx) / rx))); x.lineTo(xx, gy - 40 * s); x.stroke();
  }
  // rim (the side of the disc), gold trim at the bottom
  const rg = x.createLinearGradient(cx - rx, 0, cx + rx, 0);
  rg.addColorStop(0, "#101b3d"); rg.addColorStop(0.5, "#2a4688"); rg.addColorStop(1, "#101b3d");
  x.fillStyle = rg;
  x.beginPath(); x.moveTo(cx - rx, cy); x.lineTo(cx - rx, cy + th); x.ellipse(cx, cy + th, rx, ry, 0, Math.PI, 0, true);
  x.lineTo(cx + rx, cy); x.ellipse(cx, cy, rx, ry, 0, 0, Math.PI, false); x.closePath(); x.fill();
  x.strokeStyle = "rgba(244,197,78,0.85)"; x.lineWidth = 2 * s;
  x.beginPath(); x.ellipse(cx, cy + th, rx, ry, 0, Math.PI, 0, true); x.stroke();
}

function drawPlatform(t, dt) {
  const s = H / 760, cx = KX(), cy = PY(), rx = PR(), ry = rx * SQ, th = 22 * s, gy = GY();
  if (!glass) glass = makeGlass();
  // light column rising from the face
  const pulse = 0.5 + 0.5 * Math.sin(t * 1.1);
  const col = g.createLinearGradient(0, cy - H * 0.45, 0, cy);
  col.addColorStop(0, "rgba(180,210,255,0)"); col.addColorStop(1, `rgba(200,225,255,${0.10 + 0.05 * pulse})`);
  g.fillStyle = col;
  g.beginPath(); g.moveTo(cx - rx * 0.55, cy - H * 0.45); g.lineTo(cx + rx * 0.55, cy - H * 0.45); g.lineTo(cx + rx * 0.95, cy); g.lineTo(cx - rx * 0.95, cy); g.closePath(); g.fill();
  g.drawImage(layers.pillar, 0, 0, W, H);     // pillar + rim (static layer)
  // the glass face (cached pattern, slowly turning) + a band of light running round it
  g.save();
  g.beginPath(); g.ellipse(cx, cy, rx, ry, 0, 0, Math.PI * 2); g.clip();
  g.translate(cx, cy); g.scale(1, SQ);
  g.rotate(still ? 0 : t * 0.05);
  g.globalAlpha = 0.88 + 0.12 * pulse;
  g.drawImage(glass, -rx, -rx, rx * 2, rx * 2);
  g.globalAlpha = 1;
  if (g.createConicGradient) {
    const cg = g.createConicGradient(still ? 0 : t * 0.7, 0, 0);
    cg.addColorStop(0, "rgba(255,245,215,0)"); cg.addColorStop(0.06, "rgba(255,245,215,0.38)"); cg.addColorStop(0.14, "rgba(255,245,215,0)");
    cg.addColorStop(1, "rgba(255,245,215,0)");
    g.globalCompositeOperation = "lighter";
    g.fillStyle = cg; g.fillRect(-rx, -rx, rx * 2, rx * 2);
  }
  g.restore();
  g.strokeStyle = "rgba(244,197,78,0.95)"; g.lineWidth = 2.5 * s;
  g.beginPath(); g.ellipse(cx, cy, rx, ry, 0, 0, Math.PI * 2); g.stroke();
  // glow around the face
  const halo = g.createRadialGradient(cx, cy, rx * 0.3, cx, cy, rx * 1.7);
  halo.addColorStop(0, `rgba(170,205,255,${0.10 + 0.06 * pulse})`); halo.addColorStop(1, "rgba(170,205,255,0)");
  g.save(); g.globalCompositeOperation = "lighter"; g.fillStyle = halo; g.fillRect(cx - rx * 1.8, cy - rx * 1.8, rx * 3.6, rx * 3.6); g.restore();
  // sparkles rising from the glass
  if (risers.length < 26 && Math.random() < 0.35) {
    const a = Math.random() * Math.PI * 2, r = Math.sqrt(Math.random()) * rx * 0.9;
    risers.push({ x: cx + Math.cos(a) * r, y: cy + Math.sin(a) * r * SQ, vy: rnd(14, 30), life: 0, max: rnd(2.5, 4.5), p: rnd(0, 6.28) });
  }
  g.save(); g.globalCompositeOperation = "lighter";
  for (const p of risers) {
    if (!still) { p.life += dt; p.y -= p.vy * dt; p.x += Math.sin(t * 1.5 + p.p) * 8 * dt; }
    const rr = 2.2 * s;
    g.globalAlpha = Math.max(0, Math.sin(Math.PI * Math.min(1, p.life / p.max)) * 0.8);
    g.drawImage(layers.blue, p.x - rr * 4, p.y - rr * 4, rr * 8, rr * 8);
  }
  g.restore();
  risers = risers.filter((p) => p.life < p.max);
  // mist where the pillar meets the grass
  const mg = g.createRadialGradient(cx, gy - 10 * s, 0, cx, gy - 10 * s, rx * 2.2);
  mg.addColorStop(0, "rgba(140,170,230,0.20)"); mg.addColorStop(1, "rgba(140,170,230,0)");
  g.fillStyle = mg; g.fillRect(cx - rx * 2.4, gy - rx * 1.2, rx * 4.8, rx * 1.6);
}

function drawSparks(t, dt) {
  g.save(); g.globalCompositeOperation = "lighter";
  for (let i = 0; i < sparks.length; i++) {
    const p = sparks[i];
    if (!still) { p.x += p.vx * dt + Math.sin(t * 0.8 + p.p) * 6 * dt; p.y += p.vy * dt; }
    if (p.y > H * 0.95 || p.x > W + 10) { sparks[i] = newSpark(false); continue; }
    g.globalAlpha = 0.35 + 0.55 * (0.5 + 0.5 * Math.sin(t * 3 + p.p));
    g.drawImage(layers.gold, p.x - p.r * 4, p.y - p.r * 4, p.r * 8, p.r * 8);
  }
  g.restore();
}

function drawGrass(t, layer) {
  const gust = (x) => Math.sin(t * 0.9 - x / W * 5) * 0.6 + Math.sin(t * 2.1 + x * 0.02) * 0.25;
  const base = H * layer.y;
  g.fillStyle = layer.c;
  g.fillRect(0, base - 2, W, H - base + 2);
  g.beginPath();
  for (const b of layer.blades) {
    const sway = (still ? 0 : (gust(b.x) * 0.10 + Math.sin(t * 1.6 + b.p) * 0.05)) + b.lean * 0.4;
    const tipx = b.x + sway * b.h, tipy = base - b.h * (1 - Math.abs(sway) * 0.25);
    g.moveTo(b.x - b.w, base);
    g.quadraticCurveTo(b.x - b.w * 0.3 + sway * b.h * 0.3, base - b.h * 0.55, tipx, tipy);
    g.quadraticCurveTo(b.x + b.w * 0.3 + sway * b.h * 0.3, base - b.h * 0.55, b.x + b.w, base);
  }
  g.fill();
}

let last = performance.now();
function frame(now) {
  const t = now / 1000, dt = Math.min(0.1, (now - last) / 1000);
  last = now;
  drawSky(t);
  drawGrass(t, grass[0]);
  drawPlatform(t, dt);
  drawGrass(t, grass[1]);
  drawSparks(t, dt);
  drawGrass(t, grass[2]);
}
// one frame, then sleep until the next one is due (a timer, not requestAnimationFrame at the monitor's 60-165 Hz)
function tick() {
  if (document.hidden) { setTimeout(tick, 500); return; }          // minimized / hidden: draw nothing
  const busy = S && (S.status === "running" || S.status === "verifying");
  const t0 = performance.now();
  requestAnimationFrame((now) => {
    frame(now);
    const spent = performance.now() - t0;
    const fps = document.hasFocus() ? (busy ? FPS_BUSY : FPS_MENU) : (busy ? FPS_AWAY_BUSY : FPS_AWAY);
    setTimeout(tick, Math.max(1, 1000 / fps - spent));
  });
}
addEventListener("resize", () => { setup(); frame(performance.now()); });
setup();
if (still) frame(performance.now()); else tick();
poll();
