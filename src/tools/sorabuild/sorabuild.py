"""sorabuild: ONE data builder for Sora in BotW (friend installer W1).

  python -E -P sorabuild.py --config config.thispc.json all           every step not done yet, in dependency order
  python -E -P sorabuild.py --config ... fx_base sfx_base             these steps (always re-run) + their missing deps
  python -E -P sorabuild.py --config ... --group hud                  every step of one group
  python -E -P sorabuild.py --config ... --list                       steps, groups, outputs, done state
  --out / --work / --kh2 / --fxcache (old: --phh) / --romfs override the config.  --force re-runs done steps too.

Then: python -E -P verify.py --out <out> --ref <live data dir> [--manifest manifest.json]
"""
import argparse
import glob
import importlib.util
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sb  # noqa: E402


def load_steps():
    steps = {}
    for f in sorted(glob.glob(os.path.join(sb.HERE, "steps", "*.py"))):
        g = os.path.splitext(os.path.basename(f))[0]
        if g.startswith("_"):
            continue
        spec = importlib.util.spec_from_file_location("sbsteps_" + g, f)
        m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(m)
        for s in getattr(m, "STEPS", []):
            s.group = s.group or g
            if s.name in steps:
                raise SystemExit("[sorabuild] step %s defined twice (%s, %s)" % (s.name, steps[s.name].group, g))
            steps[s.name] = s
    for s in steps.values():
        for d in s.deps:
            if d not in steps:
                raise SystemExit("[sorabuild] step %s needs unknown step %s" % (s.name, d))
    return steps


def is_done(ctx, s):
    if not os.path.exists(os.path.join(ctx.work, "done", s.name)):
        return False
    if getattr(s, "stale", None) and s.stale(ctx):  # e.g. sounds_build after a mode switch / new recipe
        return False
    return all(os.path.exists(os.path.join(ctx.out, o)) for o in s.outputs) and \
        all(os.path.exists(os.path.join(ctx.work, o)) for o in s.work_outputs)


def order(steps, wanted):
    seen, out = set(), []

    def visit(n, stack):
        if n in seen:
            return
        if n in stack:
            raise SystemExit("[sorabuild] dependency loop: " + " -> ".join(stack + [n]))
        for d in steps[n].deps:
            visit(d, stack + [n])
        seen.add(n)
        out.append(n)
    for n in wanted:
        visit(n, [])
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("steps", nargs="*")
    ap.add_argument("--config")
    ap.add_argument("--group", action="append", default=[])
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--jobs", type=int, default=0, help="steps at once (default config 'jobs', else %d)" % 3)
    for k in sb.CFG_KEYS:
        ap.add_argument("--" + k)
    ap.add_argument("--phh", dest="fxcache", help="deprecated alias of --fxcache")
    a = ap.parse_args()
    cfg = sb.load_cfg(a.config, {k: getattr(a, k) for k in sb.CFG_KEYS})
    if not a.list:
        sb.pick_kh2(cfg)  # (s168) config kh2img: extract only the needed KH2 files first
    elif cfg.get("kh2img") and not cfg.get("kh2"):
        cfg["kh2"] = os.path.join(cfg["work"], "kh2")
    ctx = sb.Ctx(cfg)
    steps = load_steps()

    if a.list:
        for n in order(steps, list(steps)):
            s = steps[n]
            print("%-24s %-8s %-5s deps=%s out=%s" % (n, s.group, "done" if is_done(ctx, s) else "-",
                                                     ",".join(s.deps) or "-", " ".join(s.outputs) or "-"))
        return

    named = list(a.steps)
    if "all" in named:
        named = [n for n in named if n != "all"] + list(steps)
        explicit = set(a.steps) - {"all"}
    else:
        explicit = set(named)
    for g in a.group:
        named += [n for n, s in steps.items() if s.group == g]
        explicit |= {n for n, s in steps.items() if s.group == g}
    for n in named:
        if n not in steps:
            raise SystemExit("[sorabuild] unknown step %s (see --list)" % n)
    if not named:
        ap.print_help()
        return

    # (s154 review M1/M2) a chain is all-or-nothing: if one of its steps has to run, all of them run (in order)
    run = order(steps, named)
    # s163: a step also runs when one of its deps runs (drop_dependents below clears its marker mid-run), so the chain test
    # uses that closure: fxrec_record re-ran (writer fix) -> fx_base..chick re-ran but sfx_base..skate stayed "done" and
    # fxsfx_chick found no work/fxchain/sfx.bin (sfx_final removes it).
    for _ in range(3):
        will = set()
        for n in run:
            if a.force or n in explicit or not is_done(ctx, steps[n]) or any(d in will for d in steps[n].deps):
                will.add(n)
        hot = {steps[n].chain for n in will if steps[n].chain}
        for c in hot:
            members = [n for n, s in steps.items() if s.chain == c]
            explicit |= set(members)
            run = order(steps, run + members)
    # dependents of every step that will run lose their done marker (so a lone re-run cannot leave stale later steps)
    users = {}
    for n, s in steps.items():
        for d in s.deps:
            users.setdefault(d, set()).add(n)

    def drop_dependents(n, seen):
        for u in users.get(n, ()):
            if u not in seen:
                seen.add(u)
                m = os.path.join(ctx.work, "done", u)
                if os.path.exists(m):
                    os.remove(m)
                drop_dependents(u, seen)

    sb.check_inputs(cfg)
    # (s168) decide up front what runs (same rule as the old serial loop: not done / named / forced / a dep runs), drop the
    # markers, then run the steps in parallel as their deps finish
    todo, tset = [], set()
    for n in run:
        if a.force or n in explicit or not is_done(ctx, steps[n]) or any(d in tset for d in steps[n].deps):
            todo.append(n)
            tset.add(n)
        else:
            print("[sorabuild] %s: done, skipped" % n)
    for n in todo:
        m = os.path.join(ctx.work, "done", n)
        if os.path.exists(m):
            os.remove(m)
        drop_dependents(n, set())
    jobs = a.jobs or int(cfg.get("jobs", 0) or 0) or DEFAULT_JOBS
    t0 = time.time()
    run_parallel(ctx, steps, todo, tset, max(1, jobs), float(cfg.get("min_free_gb", MIN_FREE_GB)))
    print("[sorabuild] all requested steps done, %.1f min" % ((time.time() - t0) / 60))


# (s168) parallel runner. Seconds per step on the dev PC (s168kit3 parallel kit run, jobs 3): only used to start the longest chains first.
EST = {"fxrec_record": 618, "form_final": 204, "form_final_s91": 134, "form_master": 99, "sora_base": 91, "form_limit": 90,
       "form_valor": 78, "party_goofy": 76, "form_final_s92": 72, "summon_genie": 69, "form_roxas": 65, "sora_hits": 57,
       "form_anti": 57, "form_mickey": 44, "form_wisdom": 43, "skate": 42, "fx_base": 39, "fxsfx_sumdata": 37,
       "party_donald": 32, "summon_stitch": 27, "summon_peterpan": 23, "fxsfx_knock": 22, "fxsfx_chick": 19,
       "summon_chicken": 16, "lim133": 16, "sounds_build": 15, "knock149": 14, "summon_tink": 12, "fxsfx_stitch": 11,
       "hud_itempic": 11, "logo_build": 25, "ragmap_build": 2, "sounds_mine": 3}
DEFAULT_JOBS = 3        # steps at once (fxrec_record runs its own 2 files x 5 recorders inside one step)
MIN_FREE_GB = 2.5       # a new step starts only while this much RAM is free (or nothing else runs)
TOOL_BUILDS = {"cs_build", "hud_build", "fx_trigjson", "fxrec_build"}   # dotnet / clang builds: one at a time


def run_parallel(ctx, steps, todo, tset, jobs, min_free_gb):
    import copy
    import queue
    import threading

    users = {}
    for n in todo:
        for d in steps[n].deps:
            users.setdefault(d, []).append(n)
    prio = {}
    for n in reversed(todo):  # todo is in dependency order: a step's priority = its time + the longest chain after it
        prio[n] = EST.get(n, 5) + max([prio[u] for u in users.get(n, ()) if u in prio] or [0])
    pending, running, finished, failed = list(todo), {}, set(), []
    q = queue.Queue()
    total = len(todo)

    def work(n):
        c = copy.copy(ctx)
        c.step = n
        t = time.time()
        try:
            c.log("start")
            steps[n].run(c)
            for o in steps[n].outputs:
                c.need(os.path.join(c.out, o))
            for o in steps[n].work_outputs:
                c.need(os.path.join(c.work, o))
            with open(os.path.join(c.work, "done", n), "w", encoding="utf-8") as f:
                json.dump({"time": time.strftime("%Y-%m-%d %H:%M:%S"), "secs": round(time.time() - t, 1)}, f)
            c.log("done %.1f s" % (time.time() - t))
            q.put((n, None, time.time() - t))
        except BaseException as e:   # SystemExit from ctx.run / ctx.need, or a bug in the step
            q.put((n, e, time.time() - t))

    def mem_ok():
        f = sb.free_mb()
        return f is None or f >= min_free_gb * 1024

    print("[sorabuild] %d steps, up to %d at once (free RAM %s MB)" % (total, jobs, sb.free_mb()), flush=True)
    waited = False
    while pending or running:
        if not failed:
            ready = sorted((n for n in pending if all(d not in tset or d in finished for d in steps[n].deps)), key=lambda n: -prio[n])
            for n in ready:
                if len(running) >= jobs:
                    break
                if n in TOOL_BUILDS and any(r in TOOL_BUILDS for r in running):
                    continue
                if running and not mem_ok():
                    if not waited:
                        print("[sorabuild] waiting for free RAM (%s MB free, want %d) before %s" % (sb.free_mb(), min_free_gb * 1024, n),
                              flush=True)
                    waited = True
                    break
                waited = False
                th = threading.Thread(target=work, args=(n,), daemon=True)
                running[n] = th
                pending.remove(n)
                th.start()
                if len(running) > 1:
                    break     # one new step per round once others run: its RAM shows up before the next check
        if not running:
            if failed or not pending:
                break
            raise SystemExit("[sorabuild] nothing can start: %s wait for steps that did not run" % ", ".join(pending))
        try:
            n, err, secs = q.get(timeout=2)
        except queue.Empty:
            continue
        running.pop(n).join()
        if err is not None:
            failed.append((n, err))
            print("[sorabuild] %s FAILED: %s" % (n, err), flush=True)
            if running:
                print("[sorabuild] waiting for the running steps to end: %s" % ", ".join(running), flush=True)
        else:
            finished.add(n)
            print("[progress] %d/%d %s %.1f s" % (len(finished), total, n, secs), flush=True)
    if failed:
        n, err = failed[0]
        if isinstance(err, SystemExit):
            raise err
        raise SystemExit("[sorabuild] %s crashed: %r" % (n, err))


if __name__ == "__main__":
    main()
