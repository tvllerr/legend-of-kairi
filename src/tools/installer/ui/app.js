"use strict";
// Legend of Kairi installer page (Sora in BotW, s168). Talks to installer.py on 127.0.0.1 (token from the URL).
const TOKEN = new URLSearchParams(location.search).get("t") || "";
const $ = (id) => document.getElementById(id);
let stillKey = "";
let S = null, view = "setup", shownView = null, seq = 0, logOpen = false, helpSel = "h-sw", helpInit = false;

// (s184) DEV preview: &demo=1 fakes the polled state locally (setup 3 s, a ~25 s install, then done). Inert without the param.
const DEMO = new URLSearchParams(location.search).get("demo") === "1";
const DEMO_FREEZE = new URLSearchParams(location.search).get("demo_t");   // (s185) DEV only: &demo_t=12 freezes the fake clock at 12 s (for screenshots)
// (s185) the new interaction code lives in feel.js; a failure there must never break the installer page
const F = (name, ...a) => { try { return window.Feel && Feel[name] ? Feel[name](...a) : undefined; } catch (e) { console.error(e); } };
let DEMO_T0 = null;   // (s185) the fake install waits on the setup screen until Install is pressed; pressed again = it starts over
let demoSfx = null;
const DEMO_END = new URLSearchParams(location.search).get("demo_end");   // (s185 polish) DEV only: failed | cancelled = the fake install ends that way at 15 s
function demoState() {
  const t0 = DEMO_FREEZE !== null ? Math.min(28, +DEMO_FREEZE) : DEMO_T0 === null ? 0 : Math.min(28, (performance.now() - DEMO_T0) / 1000);
  const stopAt = DEMO_END === "failed" || DEMO_END === "cancelled" ? 15 : 99, t = Math.min(t0, stopAt);
  const run = Math.min(1, Math.max(0, (t - 3) / 25));   // clock stops at done
  const status = t < 3 ? "setup" : t >= stopAt ? DEMO_END : run >= 1 ? "done" : "running";
  const keys = [["kh2", "Unpack the KH2 files the mod needs"], ["bakes", "Bake Sora, the Drive Forms, the party and the summons"],
    ["fxrec", "Record the effects"], ["fx", "Build the effect packs"], ["other", "HUD and sounds"], ["verify", "Check every file"], ["game", "Set up the game"]];
  const n = keys.length;
  const phases = keys.map(([key, label], i) => {
    const f = Math.min(1, Math.max(0, run * n - i));
    return { key, label, state: status === "setup" ? "wait" : f >= 1 ? "done" : f > 0 ? (status === "failed" ? "failed" : "run") : "wait", frac: status === "setup" ? 0 : f,
             running: f > 0 && f < 1 ? ["form_valor", "summon_stitch"] : [] };
  });
  return { version: "0.0.0-demo", status, root: "D:/Demo/Legend of Kairi", mode: "install", updating: false, dlonly: false, resumable: false,
    sw: { path: "D:/Demo/SavageWind", ok: true, msg: "Found it" }, kh2: { path: "D:/Demo/KH2", ok: true, auto: true, msg: "Found it" },
    upd: { path: "", ok: false, msg: "" }, disk: { free: 200, need: 40, ok: true }, phases, frac: status === "setup" ? 0 : run,
    elapsed: Math.max(0, t - 3) * 36, eta: status === "running" ? Math.round((1 - run) * 900) : null, build_id: "demo",
    error: status === "failed" ? "Some files did not come out right; press Install again to rebuild them. (2 file(s): fx/kh2_fx.bin, sora_model.bin)" : "",
    launchers: status === "done" ? { solo: true } : {}, seq: 0, lines: [], sw_version: "0.4.1" };
}
async function api(path, body) {
  if (DEMO && path === "/api/install") { DEMO_T0 = performance.now() - 3000; return {}; }
  if (DEMO && path !== "/api/open") {   // (s185) the Open buttons are real in the demo too; Play / desktop shortcut fake success ({})
    if (!path.startsWith("/api/state")) return {};
    if (demoSfx === null) {             // the sounds are real in the demo: ask the server once whether they exist
      demoSfx = false;
      fetch("/api/state?since=0", { headers: { "X-Token": TOKEN } }).then((r) => r.json()).then((s) => { demoSfx = !!s.sfx; }).catch(() => {});
    }
    return Object.assign(demoState(), { sfx: demoSfx });
  }
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
    if (window.kairiSfx) kairiSfx.onState(st);   // s184 menu sounds
    // an install started any other way (API call, resume) must show the progress screen too
    if (DEMO && st.status !== "setup" && !was) view = "run";
    if (busy.includes(st.status) && (!was || !busy.includes(was)) && view !== "run") view = "run";
    render();
    taskbar(st);
    if (still) { const key = st.status + pct(st.frac) + st.phases.map((p) => p.state[0]).join(""); if (key !== stillKey) { stillKey = key; frame(performance.now()); } }
  } catch (e) { /* server gone: window closing */ }
  setTimeout(poll, 700);
}

// (s185) the window program (WebView2) shows the progress on its taskbar button: one tiny message, only when it changes.
// Outside the window program (the Edge fallback, a browser) window.chrome.webview does not exist and this does nothing.
let tbLast = "";
function taskbar(st) {
  const wv = window.chrome && window.chrome.webview;
  if (!wv || typeof wv.postMessage !== "function" || !st) return;
  const s = st.status, state = s === "running" || s === "verifying" ? "run" : s === "failed" ? "fail" : s === "done" ? "done" : "none";
  const v = state === "done" ? 1000 : state === "none" ? 0 : Math.round(Math.min(1, Math.max(0, +st.frac || 0)) * 1000);
  const m = "p:" + v + ":" + state;
  if (m === tbLast) return;
  tbLast = m;
  try { wv.postMessage(m); } catch (e) { /* the window program ignores what it does not know */ }
}

const short = (p) => p.length > 34 ? "..." + p.slice(-33) : p;
const mmss = (s) => { s = Math.max(0, Math.round(s)); const m = Math.floor(s / 60); return m + ":" + String(s % 60).padStart(2, "0"); };

function render() {
  if (!S) return;
  $("ver").textContent = "Version " + S.version;
  for (const id of ["s-setup", "s-run", "s-credits"]) $(id).hidden = id !== "s-" + view;
  if (shownView !== view) {     // (s184) a short fade/slide into the new screen (CSS transform + opacity only, once per change)
    if (shownView !== null && !still) {   // (s185) forward = in from the right, back (to the setup screen) = in from the left
      const el = $("s-" + view); el.classList.remove("enter", "fwd", "back"); void el.offsetWidth; el.classList.add("enter", view === "setup" ? "back" : "fwd");
    }
    shownView = view;
  }
  document.querySelectorAll(".swv").forEach((e) => (e.textContent = S.sw_version));
  document.querySelectorAll(".newv").forEach((e) => (e.textContent = S.version));
  $("root-path").textContent = S.root;
  if (view === "setup") renderSetup();
  if (view === "run") renderRun();
  F("onRender", S, view);
}

function setVal(el, st, item, autoText) {
  const was = el.classList.contains("ok");
  el.classList.remove("ok", "bad");
  st.classList.remove("ok", "bad");
  if (!item.path) { el.textContent = "Choose..."; st.textContent = ""; F("found", el, false, false); return; }
  F("found", el, !was, !!item.ok);      // (s185) a folder that turns OK draws its check mark once
  el.textContent = item.ok ? (item.auto ? autoText : short(item.path)) : "Check this";
  el.classList.add(item.ok ? "ok" : "bad");
  st.textContent = (item.ok ? "✓ " : "") + item.msg + (item.ok ? "  " + item.path : "");
  st.classList.add(item.ok ? "ok" : "bad");
}

let LEAD = "", doneReady = false, doneSeenAt = 0;
function renderSetup() {
  setVal($("v-sw"), $("st-sw"), S.sw, "");
  setVal($("v-kh2"), $("st-kh2"), S.kh2, "Found (Steam)");
  const ready = S.sw.ok && S.kh2.ok && S.disk.ok;
  const U = S.upd || {}, upd = !!S.updating, ui = U.info || {};
  if (!helpInit) { helpInit = true; if (upd) helpSel = "h-go"; }     // (s184) opened by Update.bat: show the Update tip first
  $("r-go").disabled = !ready || (S.dlonly && !upd);     // (s184) a copy Update.bat downloaded may only update its parent
  // (s183) an older install picked: the big button updates that folder instead of installing here
  $("go-lbl").textContent = upd ? (ui.resume ? "Finish the update" : "Update Legend of Kairi") : S.resumable ? "Continue the install" : "Install Legend of Kairi";
  const sv = (v) => String(v || "?").replace(/-.*/, "");
  $("v-go").textContent = !ready ? "" : upd ? `${sv(ui.version)} → ${sv(S.version)}` : (S.resumable ? "Continue" : "About 15 min");
  if (!LEAD) LEAD = $("setup-lead").textContent;
  $("setup-lead").textContent = upd ? `Your Legend of Kairi ${ui.version} is brought up to ${S.version} in its own folder: only what changed is ` +
    "rebuilt and checked, usually a few minutes. Your saves and your games stay as they are." : LEAD;
  const vu = $("v-upd"), su = $("st-upd");
  const wasU = vu.classList.contains("ok");
  vu.classList.remove("ok", "bad"); su.classList.remove("ok", "bad");
  F("found", vu, !wasU, !!(U.path && U.ok));
  if (!U.path) { vu.textContent = "Choose..."; su.textContent = ""; }
  else {
    vu.textContent = U.ok ? short(U.path) : "Check this";
    vu.classList.add(U.ok ? "ok" : "bad");
    su.textContent = (U.ok ? "✓ " : "") + U.msg + (U.ok ? "  " + U.path : "");
    su.classList.add(U.ok ? "ok" : "bad");
  }
  $("b-updclear").hidden = !U.path;
  const d = S.disk, st = $("st-go");
  st.classList.remove("ok", "bad");
  st.textContent = d.free === null ? "" : `Needs about ${d.need} GB free on that drive while it works (${d.free} GB free now).`;
  if (d.free !== null) st.classList.add(d.ok ? "ok" : "bad");
  document.querySelectorAll(".hp").forEach((h) => h.classList.toggle("on", h.id === helpSel));
  document.querySelectorAll(".rows .row").forEach((r) => r.classList.toggle("sel", r.dataset.help === helpSel));
  F("ptr");     // (s185) the menu pointer follows the selected row
}

const SHORT = { copy: "Program files", kh2: "KH2 files", bakes: "Characters", fxrec: "Effects", fx: "Effect packs", other: "HUD + sounds", verify: "Checking files", game: "Game setup" };
const pct = (f) => Math.round(Math.min(1, Math.max(0, f)) * 100);
function nowPhase() {   // the running phase that is furthest behind (several run at once), else the first running one
  const run = S.phases.filter((p) => p.state === "run");
  if (!run.length) return null;
  return run.reduce((a, b) => (b.frac < a.frac ? b : a), run[0]);
}

let scrolledTo = "";
const PRETTY = (n) => n.replace(/^(form|summon|party|fxsfx|fx|sfx|hud)_/, "").replace(/_/g, " ");

function renderRun() {
  const s = S.status;
  const running = s === "running" || s === "verifying";
  const up = S.mode === "update";
  $("run-title").textContent = up
    ? { running: "Updating Legend of Kairi", verifying: "Checking every file", done: `Updated to ${S.version}`,
        failed: "The update stopped", cancelled: "Update stopped", setup: "Updating Legend of Kairi" }[s]
    : { running: "Installing Legend of Kairi", verifying: "Checking every file", done: "Legend of Kairi is ready",
        failed: "The install stopped", cancelled: "Install stopped", setup: "Installing Legend of Kairi" }[s];
  document.title = running ? (up ? "Legend of Kairi - installing the update" : "Legend of Kairi - installing") : "Legend of Kairi";   // the window program asks before closing while installing
  const verb = up ? "Update" : "Install";
  $("run-lead").textContent = s === "done"
    ? (up ? `Your Legend of Kairi in ${S.root} is now ${S.version}. ` : "") +
      `Every file matches the reference build (build id ${S.build_id}). It took ${mmss(S.elapsed)}. The mod's files are in ${S.root}/data.`
    : s === "failed" ? "Something went wrong. The details are below; the logs folder has everything to send to tvllerr."
    : s === "cancelled" ? `Nothing is lost: ${verb} again and it carries on where it stopped.`
    : `You can keep using your PC. If you close this window the ${verb.toLowerCase()} stops, and the next time it carries on where it stopped.`;
  $("all-fill").style.width = (s === "done" ? 100 : S.frac * 100).toFixed(1) + "%";
  const np = nowPhase(), all = s === "done" ? 100 : pct(S.frac);
  $("now").textContent = s === "done" ? "Done  ·  100%" : running ? (np ? `${SHORT[np.key] || np.label} ${pct(np.frac)}%  ·  overall ${all}%` : `Overall ${all}%`) : `Overall ${all}%`;
  $("t-el").textContent = mmss(S.elapsed);
  $("t-eta").textContent = running ? (S.eta == null ? "working out the time left..." : "about " + Math.max(1, Math.round(S.eta / 60)) + " min left") : "";
  const ol = $("phases");
  // (s184) rows are kept and updated in place (not rebuilt each poll) so a finished row can play its one-shot shine
  if (ol.children.length !== S.phases.length || S.phases.some((p, i) => ol.children[i].dataset.key !== p.key)) {
    ol.innerHTML = "";
    for (const p of S.phases) {
      const li = document.createElement("li");
      li.dataset.key = p.key;
      li.innerHTML = `<span class="ic"></span><span><span class="pl"></span><span class="sub"></span></span><div class="pb"><div class="bar"><div class="fill"></div></div><span class="pc"></span></div>`;
      li.addEventListener("animationend", (e) => { if (e.animationName === "shine") li.classList.remove("shine"); });
      ol.appendChild(li);
    }
  }
  S.phases.forEach((p, i) => {
    const li = ol.children[i], prev = li.dataset.st;
    if (prev && prev !== "done" && p.state === "done" && !still) { li.classList.remove("shine"); void li.offsetWidth; li.classList.add("shine"); }
    li.dataset.st = p.state;
    for (const c of ["wait", "run", "done", "failed"]) li.classList.toggle(c, c === p.state);
    const sub = p.state === "run" && p.running.length ? p.running.slice(0, 4).map(PRETTY).join(", ") : "";
    li.querySelector(".pl").textContent = p.label;
    const se = li.querySelector(".sub");
    se.textContent = sub; se.hidden = !sub;
    li.querySelector(".fill").style.width = (p.frac * 100).toFixed(1) + "%";
    li.querySelector(".pc").textContent = (p.state === "done" ? 100 : pct(p.frac)) + "%";
  });
  // (s185 polish) a short window scrolls the phase list inside itself: when a new phase starts (or one fails), bring it into view once
  const cur = [...ol.querySelectorAll("li.run, li.failed")].pop();
  if (cur && cur.dataset.key !== scrolledTo) {
    scrolledTo = cur.dataset.key;
    if (ol.scrollHeight > ol.clientHeight + 2) {
      const top = cur.offsetTop - ol.offsetTop;
      if (top < ol.scrollTop || top + cur.offsetHeight > ol.scrollTop + ol.clientHeight)
        ol.scrollTop = Math.max(0, top - (ol.clientHeight - cur.offsetHeight) / 2);
    }
  }
  const game = S.phases.find((p) => p.key === "game");
  if (s === "verifying" && game && game.state === "run") $("run-title").textContent = "Setting up the game";
  const L = S.launchers || {};
  // (s185) done + a solo launcher: the run screen becomes a title card with a big Play button (after the shards have landed)
  if (s === "done") { if (!doneSeenAt) doneSeenAt = performance.now(); } else doneSeenAt = 0;
  const cardOn = s === "done" && !!L.solo && (doneReady || (doneSeenAt > 0 && performance.now() - doneSeenAt > 2500));
  $("wm-sub").textContent = up ? `Updated to ${S.version}` : "is ready";
  F("card", cardOn);
  // (s185 review) the plain done screen (title, bars) is hidden only when the card really shows: without feel.js it stays as it was
  $("s-run").classList.toggle("card-on", cardOn && !$("card").hidden);
  $("play").hidden = !cardOn;
  $("b-folder").hidden = !(s === "done" && L.solo);
  const bs = $("b-shortcut");
  bs.hidden = !(s === "done" && L.solo);
  if (s !== "done" && bs.disabled) { bs.disabled = false; bs.textContent = "Add a desktop shortcut"; }   // a new install: offer it again
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
  helpSel = what === "sw" ? "h-sw" : what === "upd" ? "h-upd" : "h-kh2";
  poll1();
}
async function poll1() { const st = await api("/api/state?since=" + seq); seq = st.seq; S = st; render(); }

$("r-sw").onclick = () => pick("sw");
$("r-kh2").onclick = () => pick("kh2");
$("r-upd").onclick = async () => { await pick("upd"); helpSel = "h-upd"; render(); };
$("b-updclear").onclick = async () => { await api("/api/update", { path: "" }); helpSel = "h-go"; poll1(); };
$("r-go").onclick = async () => {
  const r = await api("/api/install", {});
  if (r.error) { alert(r.error); return; }
  view = "run"; $("log").textContent = ""; poll1();
};
$("r-credits").onclick = () => { view = "credits"; loadCredits(); render(); };
$("b-cback").onclick = () => { view = "setup"; render(); };
$("b-back").onclick = () => { view = "setup"; render(); };
$("b-cancel").onclick = async () => { if (confirm(`Stop the ${S && S.mode === "update" ? "update" : "install"}? It carries on where it stopped next time.`)) { await api("/api/cancel", {}); poll1(); } };
$("b-log").onclick = () => { logOpen = !logOpen; $("log").hidden = !logOpen; $("b-log").textContent = logOpen ? "Hide details" : "Show details"; };
const openIt = async (what) => { const r = await api("/api/open", { what }); if (r.error) alert(r.error); };   // (s185) never a silent nothing
$("b-folder").onclick = () => openIt("root");
// (s185) opt-in desktop shortcut (everything else the installer writes stays in its one folder). In the demo api() answers {} itself.
$("b-shortcut").onclick = async () => {
  const b = $("b-shortcut");
  if (b.disabled) return;
  b.disabled = true; b.textContent = "Adding...";
  let r = {};
  try { r = await api("/api/shortcut", {}); } catch (e) { r = { error: "The installer did not answer." }; }
  if (r && r.error) { b.disabled = false; b.textContent = "Add a desktop shortcut"; alert(r.error); return; }
  b.textContent = "Shortcut added";
};
$("b-logs").onclick = () => openIt("log");
$("b-lic").onclick = () => openIt("licenses");
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
let RS = 0.75;  // canvas pixels per CSS pixel: soft art scaled up looks the same at ~half the pixels (0.5 made the glass blocky)
let sparkN = 70;   // the auto governor may lower these (RS, sparkN)
let calm = false;  // (s184) Calm: no sparks, lowest frame rate
try { calm = localStorage.getItem("kairi-calm") === "1"; } catch (e) { /* storage blocked: fine */ }
const cv = $("bg"), g = cv.getContext("2d", { alpha: false });
const still = matchMedia("(prefers-reduced-motion: reduce)").matches;
let W = 0, H = 0, DPR = 1, stars = [], sparks = [], grass = [], hills = [], layers = {};
const rnd = (a, b) => a + Math.random() * (b - a);

function setup() {
  DPR = 1;   // soft background art: 1 canvas pixel per CSS pixel is plenty (and 2-4x less work on high-DPI screens)
  W = innerWidth; H = innerHeight;
  cv.width = Math.ceil(W * RS); cv.height = Math.ceil(H * RS);
  g.setTransform(RS, 0, 0, RS, 0, 0);
  stars = Array.from({ length: Math.round(W * H / 5200) }, () => ({ x: rnd(0, W), y: rnd(0, H * 0.62), r: rnd(0.4, 1.5), p: rnd(0, 6.28), s: rnd(0.6, 2), born: -99 }))
    .filter((st) => Math.hypot((st.x - MX()) / 1.1, st.y - MY() - 4 * MK()) > 24 * MK());   // none in front of the moon
  sparks = Array.from({ length: sparkN }, () => newSpark(true));
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
  moonC = null; moonDrawn = -1;
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

// ---- (s184) the moon fills with light as the install goes: a warm lit copy of the heart, clipped to a soft rising edge, kept in ONE small
// canvas that is redrawn only when the shown progress moved by 0.5% or more (not per frame); per frame it is one drawImage + one glow sprite.
let moonC = null, moonBox = null, moonDrawn = -1;
function moonSpan() {
  const cy = MY(), k = MK();
  let y0 = 1e9, y1 = -1e9;
  for (let i = 0; i < 60; i++) {
    const t = i / 60 * Math.PI * 2, hy = 13 * Math.cos(t) - 5 * Math.cos(2 * t) - 2 * Math.cos(3 * t) - Math.cos(4 * t), py = cy - hy * k + 2 * k;
    y0 = Math.min(y0, py); y1 = Math.max(y1, py);
  }
  return [y0, y1];
}
function buildMoonLit(fr) {
  const cx = MX(), cy = MY(), k = MK(), [top, bot] = moonSpan(), soft = 3.5 * k, m = 2 * k;
  if (!moonC) {
    moonBox = { x: cx - 16 * k - m, y: top - m, w: 32 * k + 2 * m, h: bot - top + 2 * m };
    moonC = document.createElement("canvas");
    moonC.width = Math.ceil(moonBox.w * RS); moonC.height = Math.ceil(moonBox.h * RS);
  }
  const x = moonC.getContext("2d");
  x.setTransform(1, 0, 0, 1, 0, 0); x.clearRect(0, 0, moonC.width, moonC.height);
  x.setTransform(RS, 0, 0, RS, -moonBox.x * RS, -moonBox.y * RS);
  heartPath(x, cx, cy, k);
  const lg = x.createRadialGradient(cx - 3 * k, cy - 3 * k, 1 * k, cx, cy, 20 * k);
  lg.addColorStop(0, "#fffef2"); lg.addColorStop(0.55, "#ffe9a2"); lg.addColorStop(1, "#f7c75a");
  x.fillStyle = lg; x.fill();
  x.globalCompositeOperation = "destination-in";             // the soft rising edge
  const edge = bot - fr * (bot - top + 2 * soft);
  const mg = x.createLinearGradient(0, edge, 0, edge + 2 * soft);
  mg.addColorStop(0, "rgba(0,0,0,0)"); mg.addColorStop(1, "rgba(0,0,0,1)");
  x.fillStyle = mg; x.fillRect(moonBox.x - 1, moonBox.y - 1, moonBox.w + 2, moonBox.h + 2);
  x.globalCompositeOperation = "destination-over";           // the part not filled yet is veiled: an empty vessel that the light fills
  heartPath(x, cx, cy, k);
  x.fillStyle = `rgba(22,36,82,${(0.5 * Math.min(1, fr / 0.04)).toFixed(3)})`; x.fill();
  x.globalCompositeOperation = "source-over";
  moonDrawn = fr;
}

// ---- animation targets from the polled state (progress, per-phase segments, the finish)
let fd = 0, segLv = [], segGl = [], segFl = [], prevSt = [], doneAt = -1, doneLight = -1, moonPulseAt = -99;
const SHARD_T = 0.9;     // (s185) glass shards fly in first; the pillar of light and the title card follow
const easeOut = (u) => 1 - (1 - u) ** 3;
function animate(t, dt) {
  const st = S ? S.status : "setup", busy = st === "running" || st === "verifying", done = st === "done";
  const k1 = still ? 1 : 1 - Math.exp(-dt * 2.2);
  const tf = !S || st === "setup" ? 0 : done ? 1 : Math.min(1, Math.max(0, S.frac));
  fd += (tf - fd) * k1;
  if (Math.abs(tf - fd) < 0.002) fd = tf;
  if (done) {
    if (doneAt < 0) {
      const sh = still || calm ? 0 : SHARD_T;
      doneAt = still ? t - 99 : t; doneLight = doneAt + sh;
      if (sh) F("shards", t, sh);
    }
  } else { doneAt = -1; doneReady = false; }
  if (done && !doneReady && t >= doneLight) { doneReady = true; if (view === "run") renderRun(); }
  const ph = S && S.phases && S.phases.length ? S.phases : null, n = ph ? ph.length : 7;
  while (segLv.length < n) { segLv.push(0.6); segGl.push(0); segFl.push(0); prevSt.push(null); }
  segLv.length = segGl.length = segFl.length = prevSt.length = n;
  for (let i = 0; i < n; i++) {
    const p = ph ? ph[i] : null;
    let lv = null, gl = 0;
    if (p && prevSt[i] && prevSt[i] !== "done" && p.state === "done" && !still) phaseDone(i, t);   // (s185) a finished phase flashes its wedge
    prevSt[i] = p ? p.state : null;
    segFl[i] = Math.max(0, segFl[i] - dt * 0.8);
    if (st === "setup" || !p) lv = 0.6;
    else if (done || p.state === "done") lv = 1;
    else if (busy) { lv = p.state === "run" ? 0.2 + 0.3 * p.frac : 0; gl = p.state === "run" ? 1 : 0; }   // failed / stopped: stays as it was
    if (lv !== null) segLv[i] += (lv - segLv[i]) * k1;
    segGl[i] += (gl - segGl[i]) * k1;
  }
}

// (s185) a phase finished: its wedge flashes, the moon's halo pulses once and (not in Calm) sparkles rise from the wedge
function phaseDone(i, t) {
  segFl[i] = 1; moonPulseAt = t;
  if (calm) return;
  const N = segLv.length, a = -Math.PI / 2 + (i + 0.5) * Math.PI * 2 / N, rx = PR();
  for (let k = 0; k < 10 && risers.length < 90; k++) {
    const aa = a + rnd(-0.35, 0.35), r = rx * rnd(0.25, 0.9);
    risers.push({ x: KX() + Math.cos(aa) * r, y: PY() + Math.sin(aa) * r * SQ, vy: rnd(28, 70), life: 0, max: rnd(1.2, 2.6), p: rnd(0, 6.28), k: 0.7, a: 0.5 });
  }
}

function drawSky(t) {
  g.drawImage(layers.back, 0, 0, W, H);
  const pe = t - moonPulseAt, mp = pe >= 0 && pe < 1.3 ? Math.sin(Math.PI * pe / 1.3) ** 2 * 0.5 : 0;   // (s185) one halo pulse per finished phase / click
  if (fd > 0.003 || mp > 0) {
    if (fd > 0.003 && (!moonC || Math.abs(fd - moonDrawn) >= 0.005 || (fd === 1 && moonDrawn !== 1))) buildMoonLit(fd);
    const k = MK(), e = doneAt < 0 ? 9 : t - doneLight;
    const bloom = (e >= 0 && e < 1.6 ? Math.sin(Math.PI * Math.min(1, e / 1.6)) ** 2 * 0.55 : 0) + mp;    // one short bloom when it is done
    const r = k * (34 + 36 * fd + 30 * bloom);
    g.save(); g.globalCompositeOperation = "lighter"; g.globalAlpha = Math.min(1, 0.4 * fd + bloom);
    g.drawImage(layers.gold, MX() - r, MY() + 2 * k - r, r * 2, r * 2);
    g.restore();
    if (fd > 0.003) g.drawImage(moonC, moonBox.x, moonBox.y, moonBox.w, moonBox.h);
  }
  g.fillStyle = "#dce8ff";
  for (const st of stars) {
    g.globalAlpha = (0.35 + 0.65 * (0.5 + 0.5 * Math.sin(t * st.s + st.p))) * Math.min(1, Math.max(0, (t - st.born) / 1.5));
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
  // (s184) one wedge of the glass per phase: dim until its phase is done, a soft breathing light while it runs
  const N = segLv.length, ang = still ? 0 : t * 0.05;
  g.rotate(-ang);
  for (let i = 0; i < N; i++) {
    const a0 = -Math.PI / 2 + i * Math.PI * 2 / N, a1 = a0 + Math.PI * 2 / N + 0.01, dim = 0.78 * (1 - segLv[i]);
    if (dim > 0.01) { g.fillStyle = `rgba(5,11,28,${dim.toFixed(3)})`; g.beginPath(); g.moveTo(0, 0); g.arc(0, 0, rx * 1.02, a0, a1); g.closePath(); g.fill(); }
    if (segGl[i] > 0.02) {
      g.globalCompositeOperation = "lighter";
      g.fillStyle = `rgba(255,226,150,${((0.07 + 0.07 * pulse) * segGl[i]).toFixed(3)})`;
      g.beginPath(); g.moveTo(0, 0); g.arc(0, 0, rx * 1.02, a0, a1); g.closePath(); g.fill();
      g.globalCompositeOperation = "source-over";
    }
    if (segFl[i] > 0.02) {      // (s185) the wedge of a phase that just finished flashes bright, then settles lit
      g.globalCompositeOperation = "lighter";
      g.fillStyle = `rgba(255,238,185,${(0.7 * segFl[i] * segFl[i]).toFixed(3)})`;
      g.beginPath(); g.moveTo(0, 0); g.arc(0, 0, rx * 1.02, a0, a1); g.closePath(); g.fill();
      g.globalCompositeOperation = "source-over";
    }
  }
  g.lineWidth = 2.2 * s;          // thin gold dividers between finished wedges
  for (let i = 0; i < N; i++) {
    const lv = Math.min(segLv[i], segLv[(i + N - 1) % N]);
    if (lv < 0.8 || N < 2) continue;
    const a0 = -Math.PI / 2 + i * Math.PI * 2 / N;
    g.strokeStyle = `rgba(244,197,78,${(0.6 * lv).toFixed(3)})`;
    g.beginPath(); g.moveTo(0, 0); g.lineTo(Math.cos(a0) * rx, Math.sin(a0) * rx); g.stroke();
  }
  g.rotate(ang);
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
  const dg = doneAt < 0 ? 0 : Math.min(1, Math.max(0, (t - doneLight) / 1.2));      // finished: the whole platform glows
  halo.addColorStop(0, `rgba(${170 + 85 * dg | 0},${205 + 20 * dg | 0},${255 - 105 * dg | 0},${0.10 + 0.06 * pulse + 0.22 * dg})`); halo.addColorStop(1, "rgba(170,205,255,0)");
  g.save(); g.globalCompositeOperation = "lighter"; g.fillStyle = halo; g.fillRect(cx - rx * 1.8, cy - rx * 1.8, rx * 3.6, rx * 3.6);
  if (doneAt >= 0 && t >= doneLight) {         // the "you made it" pillar of light (after the shards landed): rises once (~1.5 s), then settles into a calm column
    const e = t - doneLight, rise = easeOut(Math.min(1, e / 1.1)), bump = e < 0.35 ? e / 0.35 : Math.max(0, 1 - (e - 0.35) / 1.15) ** 2;
    const A = 0.14 * Math.min(1, e / 0.8) + 0.5 * bump, hgt = H * 0.95 * rise;
    if (A > 0.005) {
      for (const [wk, am] of [[0.5, 1], [0.2, 1.3]]) {
        const bg = g.createLinearGradient(0, cy, 0, cy - hgt);
        bg.addColorStop(0, `rgba(255,240,200,${Math.min(1, A * am).toFixed(3)})`); bg.addColorStop(1, "rgba(255,240,200,0)");
        g.fillStyle = bg;
        g.beginPath(); g.moveTo(cx - rx * wk, cy); g.lineTo(cx - rx * wk * 0.6, cy - hgt); g.lineTo(cx + rx * wk * 0.6, cy - hgt); g.lineTo(cx + rx * wk, cy); g.closePath(); g.fill();
      }
    }
  }
  g.restore();
  // sparkles rising from the glass
  if (!calm && risers.length < 26 && Math.random() < 0.35) {
    const a = Math.random() * Math.PI * 2, r = Math.sqrt(Math.random()) * rx * 0.9;
    risers.push({ x: cx + Math.cos(a) * r, y: cy + Math.sin(a) * r * SQ, vy: rnd(14, 30), life: 0, max: rnd(2.5, 4.5), p: rnd(0, 6.28) });
  }
  g.save(); g.globalCompositeOperation = "lighter";
  for (const p of risers) {
    if (!still) { p.life += dt; p.y -= p.vy * dt; p.x += Math.sin(t * 1.5 + p.p) * 8 * dt; }
    const rr = 2.2 * s * (p.k || 1);      // (s185) burst sparkles are smaller and fainter (many together must not blow out to white)
    g.globalAlpha = Math.max(0, Math.sin(Math.PI * Math.min(1, p.life / p.max)) * 0.8 * (p.a || 1));
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
  if (calm) return;
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

// (s184) automatic detail: if drawing a frame averages over 12 ms for 3 s, halve the sparks, then lower the canvas resolution (twice at most)
let gvStep = 0, gvWarm = 0, gvStart = 0, gvN = 0, gvSum = 0;
function governor(ms, now) {
  if (now < gvWarm) return;
  if (!gvN) gvStart = now;
  gvN++; gvSum += ms;
  if (now - gvStart < 3000) return;
  const avg = gvSum / gvN;
  gvN = gvSum = 0;
  if (avg <= 12 || gvStep >= 3) return;
  gvStep++; gvWarm = now + 1500;
  if (gvStep === 1) { sparkN = 35; sparks.length = Math.min(sparks.length, sparkN); console.info("installer: frames avg " + avg.toFixed(1) + " ms, halving the sparkles"); }
  else { RS = gvStep === 2 ? 0.6 : 0.5; console.info("installer: frames avg " + avg.toFixed(1) + " ms, canvas scale " + RS); setup(); }
}
let last = performance.now();
function frame(now) {
  const t = now / 1000, dt = Math.min(0.1, (now - last) / 1000);
  last = now;
  const c0 = performance.now();
  animate(t, dt);
  drawSky(t);
  F("sky", t, dt);
  drawGrass(t, grass[0]);
  drawPlatform(t, dt);
  F("over", t, dt);
  drawGrass(t, grass[1]);
  drawSparks(t, dt);
  drawGrass(t, grass[2]);
  if (!still && document.hasFocus()) governor(performance.now() - c0, now);
}
// one frame, then sleep until the next one is due (a timer, not requestAnimationFrame at the monitor's 60-165 Hz)
// (s185, user) smooth 60 a second while NOTHING is being installed and the window is in front (menu, credits, the done screen):
// then every screen refresh is used (refreshes of a faster monitor are skipped). Installing, away or Calm = the low caps as before.
const FPS_SMOOTH = 60;
let lastDrawn = 0;
function tick() {
  if (document.hidden) { setTimeout(tick, 500); return; }          // minimized / hidden: draw nothing
  const busy = S && (S.status === "running" || S.status === "verifying");
  const smooth = !busy && !calm && document.hasFocus();
  const t0 = performance.now();
  requestAnimationFrame((now) => {
    if (smooth && now - lastDrawn < 1000 / FPS_SMOOTH - 3) { tick(); return; }
    lastDrawn = now;
    try { frame(now); } catch (e) { console.error(e); }     // (s184 review) one bad frame must not stop the background for good
    if (smooth) { tick(); return; }
    const spent = performance.now() - t0;
    const fps = calm || !document.hasFocus() ? (busy ? FPS_AWAY_BUSY : FPS_AWAY) : (busy ? FPS_BUSY : FPS_MENU);
    setTimeout(tick, Math.max(1, 1000 / fps - spent));
  });
}
// (s184) Calm toggle + pausing the little CSS spinner while the window is away
const calmBtn = $("calm");
function applyCalm() { document.documentElement.classList.toggle("calm", calm); calmBtn.setAttribute("aria-pressed", calm ? "true" : "false"); }
calmBtn.onclick = () => {
  calm = !calm; applyCalm();
  try { localStorage.setItem("kairi-calm", calm ? "1" : "0"); } catch (e) { /* ignore */ }
  if (calm) sparks.length = 0; else sparks = Array.from({ length: sparkN }, () => newSpark(true));
  frame(performance.now());
};
function applyAway() { document.documentElement.classList.toggle("away", document.hidden || !document.hasFocus()); }
addEventListener("focus", applyAway); addEventListener("blur", applyAway); document.addEventListener("visibilitychange", applyAway);
applyCalm(); applyAway();
setTimeout(() => document.body.classList.remove("intro"), 1500);
addEventListener("resize", () => { setup(); F("layout"); frame(performance.now()); });
// (s185) start after feel.js has been loaded too (DOMContentLoaded comes after every script)
function start() { setup(); if (calm) sparks.length = 0; if (still) frame(performance.now()); else tick(); poll(); }
if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", start); else start();
