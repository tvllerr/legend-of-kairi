"use strict";
// Menu sounds for the installer (s184). The files are KH2's own menu sounds, extracted from the player's KH2 install by installer.py
// (uisfx.py) into <root>/.installer-ui/sfx and served as /sfx/<name> (token header). Nothing plays until /api/state says "sfx": true.
// , muted, setMuted(b), onState(st) }. Everything is best effort: any error = silence.
(function () {
  const NAMES = ["move", "ok", "back", "done"];
  const VOL = 0.35;
  const TOKEN = new URLSearchParams(location.search).get("t") || "";
  const BACK_IDS = new Set(["b-back", "b-cback", "b-cancel"]);
  let ctx = null, gain = null, bufs = {}, avail = false, loading = false, lastStatus = null, lastMove = 0;
  let muted = false;
  try { muted = localStorage.getItem("kairiSfxMuted") === "1"; } catch (e) { /* storage blocked */ }

  function context() {
    if (ctx) return ctx;
    try {
      const AC = window.AudioContext || window.webkitAudioContext;
      if (!AC) return null;
      ctx = new AC();
      gain = ctx.createGain();
      gain.gain.value = VOL;
      gain.connect(ctx.destination);
    } catch (e) { ctx = null; }
    return ctx;
  }

  async function load() {
    if (loading || !avail) return;
    loading = true;
    try {
      const c = context();
      if (!c) return;
      for (const n of NAMES) {
        if (bufs[n]) continue;
        try {
          const r = await fetch("/sfx/" + n, { headers: { "X-Token": TOKEN } });
          if (!r.ok) continue;
          bufs[n] = await c.decodeAudioData(await r.arrayBuffer());
        } catch (e) { /* this one stays silent */ }
      }
    } finally { loading = false; }
  }

  function play(name, rate) {
    try {
      if (muted || !avail || !document.hasFocus() || document.hidden) return;
      const b = bufs[name], c = ctx;
      if (!b || !c) return;
      if (c.state === "suspended") c.resume();
      const s = c.createBufferSource();      // one shot, never looped
      s.buffer = b;
      if (rate && rate !== 1) s.playbackRate.value = Math.min(4, Math.max(0.25, rate));   // (s185) star chimes: the same sound at other pitches
      s.connect(gain);
      s.start();
      // (s184 review) let the audio engine sleep again ~1.5 s after the sound ends (no open audio stream during the build)
      clearTimeout(play.nap);
      play.nap = setTimeout(() => { try { if (ctx && ctx.state === "running") ctx.suspend(); } catch (e) { /* ignore */ } },
                            (b.duration || 0) * 1000 / Math.min(4, Math.max(0.25, rate || 1)) + 1500);
    } catch (e) { /* ignore */ }
  }

  function setMuted(m) {
    muted = !!m;
    api.muted = muted;
    try { localStorage.setItem("kairiSfxMuted", muted ? "1" : "0"); } catch (e) { /* ignore */ }
    paint();
  }

  const api = { play, muted, setMuted };
  window.kairiSfx = api;

  // the state poll tells whether the sounds exist; "done" plays once when a run finishes
  api.onState = function (st) {
    try {
      const was = lastStatus;
      lastStatus = st.status;
      if (!!st.sfx !== avail) { avail = !!st.sfx; if (avail) load(); paint(); }
      if (was && was !== "done" && st.status === "done" && ["running", "verifying"].includes(was)) play("done");
    } catch (e) { /* ignore */ }
  };

  // sounds from the existing controls, by delegation (no per-button wiring): hover / focus = move, click = ok, back / stop = back
  const rowOf = (t) => t && t.closest ? t.closest(".rows .row") : null;
  document.addEventListener("mouseover", (e) => { const r = rowOf(e.target); if (r && r !== rowOf(e.relatedTarget)) move(); });
  document.addEventListener("focusin", (e) => { if (rowOf(e.target)) move(); });
  function move() {
    const t = performance.now();
    if (t - lastMove < 90) return;      // throttle
    lastMove = t;
    play("move");
  }
  document.addEventListener("click", (e) => {
    const b = e.target && e.target.closest ? e.target.closest("button") : null;
    if (!b || b.disabled || b.id === "sfx-mute") return;
    play(BACK_IDS.has(b.id) ? "back" : "ok");
  }, true);

  // mute button (bottom-left corner, quiet): its look is in app.css (the page's CSP allows no injected styles)
  function paint() {
    const b = document.getElementById("sfx-mute");
    if (!b) return;
    b.hidden = !avail;
    b.setAttribute("aria-pressed", muted ? "true" : "false");
    b.title = muted ? "Sounds off (click to turn on)" : "Sounds on (click to mute)";
    const x = b.querySelector(".x");
    if (x) x.style.display = muted ? "" : "none";
  }
  window.addEventListener("DOMContentLoaded", () => {
    const b = document.getElementById("sfx-mute");
    if (b) b.addEventListener("click", () => setMuted(!muted));
    paint();
  });
  if (document.readyState !== "loading") { const b = document.getElementById("sfx-mute"); if (b) b.addEventListener("click", () => setMuted(!muted)); paint(); }
})();
