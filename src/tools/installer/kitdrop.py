"""(s291) quicker Windows updates: which done markers an update with ANOTHER kit must drop (plan recon/s282/S282_UPDATE_SPEED_PLAN.md).

Before s291 an update whose kit.json differs dropped EVERY marker in work/done (s224: update.invalidate only knows data changes,
so an old kit tool copied into work/ - cs_build -> work/cs/sorabake - stayed and broke form_final). Now:
  - every build writes work/kit_used.json = a byte copy of the kit.json whose tools made the markers (installer.py);
  - an update with another kit compares that file with the new kit.json, tool by tool (sb.KIT_SRC: the "src" hash + every
    file under tools/<tool>/). Only the markers of the steps that copy a changed tool (sb.KIT_STEPS) + every step after them
    (closure over the sorabuild --list deps) are dropped; update.invalidate then runs as before (data changes).
  - no kit_used.json (an install made before s291): the OLD kit.json that update.prepare moved to .kairi-update/bak/kit/
    stands in for it when its sha256 is the kit of made-for.txt (s291 option 1, the user's choice); else drop ALL;
  - the installer window + the license texts (NO_BUILD_EFFECT) never count (s291 option 2, the user's choice);
  - anything else in the kit changed (python, dotnet, openkh, busybox, ffmpeg, vgmstream, a tool that is not in sb.KIT_STEPS,
    a tool that was removed) -> None = drop ALL (as before s291).
Pure functions (no file writes): installer.drop_other_release_work does the deleting.
"""
import hashlib
import json
import re

KIT_USED = "kit_used.json"   # in work/, next to made-for.txt
# python's own byte-code cache: a kit made from a copy of a kit whose python already ran carries new __pycache__/*.pyc (kit185 ->
# kit280w: 26 of them, nothing else outside the tools). Python uses such a file only for the .py next to it with the same mtime +
# size (a changed .py is itself a changed file) and never loads a __pycache__ .pyc without its .py, so they change nothing.
PYCACHE = re.compile(r"(^|/)__pycache__/[^/]+\.pyc$")
# (s291 option 2) kit paths no build step reads, so a change there drops no marker (exact prefixes, nothing else):
#   tools/installer_host/                        the installer window (WebView2 host, mkkit build_host; only installer.show_window)
#   LICENSES/                                     license texts (mkkit make_licenses; mkzip copies them, the credits page opens them)
#   dotnet/shared/Microsoft.WindowsDesktop.App/   WinForms for that window only: mkkit refuses a kit tool whose runtimeconfig
#                                                 names any framework but Microsoft.NETCore.App
# The rest of dotnet/ (dotnet.exe, host/fxr, Microsoft.NETCore.App) runs the tools: a change there = drop ALL.
NO_BUILD_EFFECT = ("tools/installer_host/", "LICENSES/", "dotnet/shared/Microsoft.WindowsDesktop.App/")


def kit_sha(raw):
    """the kit part of installer.work_tag: sha256 of kit.json's bytes, first 16 hex digits"""
    return hashlib.sha256(raw).hexdigest()[:16]


def load_kit(raw):
    """kit.json bytes -> dict with "src" + "files" dicts, or None when it is not one"""
    try:
        kj = json.loads(raw.decode("utf-8"))
    except (ValueError, UnicodeDecodeError, AttributeError):
        return None
    if not isinstance(kj, dict) or not isinstance(kj.get("src"), dict) or not isinstance(kj.get("files"), dict):
        return None
    return kj


def _tool_of(rel, tools):
    """'tools/sorabake/x.dll' -> 'sorabake' when that is a kit tool, else None"""
    m = re.match(r"tools/([^/]+)/", rel)
    return m.group(1) if m and m.group(1) in tools else None


def changed_tools(old_kj, new_kj):
    """-> (changed tool names, other changed files) between two kit.json dicts. A tool is changed when its "src" hash differs or a
    file under tools/<tool>/ is new, gone or another sha256 (a rebuild against other OpenKH DLLs keeps the src hash).
    Top-level keys other than "src" / "files" ("made", "base", "rebuilt") never count, nor python's __pycache__/*.pyc (PYCACHE)
    nor the NO_BUILD_EFFECT paths."""
    tools = set(old_kj["src"]) | set(new_kj["src"])
    changed = {t for t in tools if old_kj["src"].get(t) != new_kj["src"].get(t)}
    a, b = old_kj["files"], new_kj["files"]
    other = []
    for rel in sorted(set(a) | set(b)):
        if a.get(rel) == b.get(rel):
            continue
        t = _tool_of(rel, tools)
        if t:
            changed.add(t)
        elif not PYCACHE.search(rel) and not rel.startswith(NO_BUILD_EFFECT):
            other.append(rel)
    return sorted(changed), other


def seed_markers(old_kj, new_kj, kit_steps):
    """-> (markers to drop, why). markers None = drop ALL; else the steps that copy a changed tool (maybe none at all)."""
    tools, other = changed_tools(old_kj, new_kj)
    if other:
        return None, "other kit files changed (%s%s)" % (", ".join(other[:3]), ", ..." if len(other) > 3 else "")
    gone = [t for t in tools if t not in new_kj["src"]]
    if gone:
        return None, "kit tool(s) %s are gone from the new kit" % ", ".join(gone)
    unknown = [t for t in tools if t not in kit_steps]
    if unknown:
        return None, "no step list for kit tool(s) %s (sb.KIT_STEPS)" % ", ".join(unknown)
    seeds = set()
    for t in tools:
        seeds |= set(kit_steps[t])
    return seeds, "kit tool(s) changed: %s" % (", ".join(tools) or "none")


def parse_deps(steps_list_text):
    """sorabuild --list text -> {step: [deps]} (the same line format as update.work_inputs)"""
    deps = {}
    for ln in steps_list_text.splitlines():
        m = re.match(r"(\S+)\s+(\S+)\s+(done|-)\s+deps=(\S+)\s+out=(.*)$", ln.rstrip())
        if m:
            deps[m.group(1)] = [] if m.group(4) == "-" else m.group(4).split(",")
    return deps


def downstream(steps_list_text, seeds):
    """seeds + every step that depends on one of them (directly or through other steps). -> sorted list, or None when a seed that
    is not sorabuild bookkeeping ("_" name) is no step of this --list (a renamed step: drop ALL to be safe) or the list is empty"""
    deps = parse_deps(steps_list_text)
    if not deps:
        return None
    if any(s not in deps and not s.startswith("_") for s in seeds):
        return None
    users = {}
    for n, ds in deps.items():
        for d in ds:
            users.setdefault(d, set()).add(n)
    out, todo = set(seeds), list(seeds)
    while todo:
        for u in users.get(todo.pop(), ()):
            if u not in out:
                out.add(u)
                todo.append(u)
    return sorted(out)


def plan(old_tag_kit, used_raw, new_raw, kit_steps, list_steps, bak_raw=None):
    """the whole decision. old_tag_kit = the kit part of made-for.txt's tag; used_raw = work/kit_used.json bytes (None = none);
    new_raw = the new kit.json bytes; list_steps() -> sorabuild --list text (called only when needed; may raise);
    bak_raw = (s291 option 1) <root>/.kairi-update/bak/kit/kit.json bytes (the old install's own kit, moved there by
    update.prepare), used ONLY when there is no kit_used.json and only when its sha is made-for.txt's kit.
    -> (markers to drop or None = ALL, why)"""
    if used_raw is None:
        if bak_raw is None:
            return None, "no %s and no old kit.json in the update backup" % KIT_USED
        if not old_tag_kit or kit_sha(bak_raw) != old_tag_kit:
            return None, "no %s, and the old kit.json in the update backup is not the kit of made-for.txt" % KIT_USED
        used_raw = bak_raw
    elif not old_tag_kit or kit_sha(used_raw) != old_tag_kit:
        return None, "%s is not the kit of made-for.txt" % KIT_USED
    old_kj = load_kit(used_raw)
    new_kj = load_kit(new_raw) if new_raw is not None else None
    if old_kj is None or new_kj is None:
        return None, "a kit.json cannot be read"
    seeds, why = seed_markers(old_kj, new_kj, kit_steps)
    if seeds is None:
        return None, why
    if not seeds:
        return [], why
    try:
        text = list_steps()
    except Exception as e:  # noqa: BLE001 - any failure = the safe answer
        return None, "the step list failed (%s)" % e
    out = downstream(text or "", seeds)
    if out is None:
        return None, "a step of sb.KIT_STEPS (%s) is not in the step list" % ", ".join(sorted(seeds))
    return out, why
