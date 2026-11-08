"""s164: scheduler test for recordall.Gate with fake jobs (no recordings). python -E -P test_recordall_gate.py -> exit 0 on pass."""
import concurrent.futures as cf
import os
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import recordall


def run(order, jobs, free=lambda: 16000, grow=0.6, secs=0.3):
    """order: string like 'LHLLHHL'; returns [(name, heavy, t0, t1)]"""
    g = recordall.Gate(free, grow_s=grow, poll=(0.02, 0.05), log=lambda m: None)
    rows, lk = [], threading.Lock()

    def job(it):
        i, c = it
        h = c == "H"
        n = g.start("f%d" % i, h)
        t0 = time.time()
        try:
            time.sleep(secs * (3 if h else 1) * (1 + i % 3 / 2))
        finally:
            t1 = time.time()
            g.finish(n, h)
        with lk:
            rows.append(("f%d" % i, h, t0, t1))

    with cf.ThreadPoolExecutor(max_workers=jobs) as ex:
        list(ex.map(job, enumerate(order)))
    return rows


def overlap(a, b):
    return a[2] < b[3] - 1e-3 and b[2] < a[3] - 1e-3


def check_heavy_alone(rows):
    for a in rows:
        if a[1]:
            for b in rows:
                assert a is b or not overlap(a, b), "heavy %s overlaps %s" % (a[0], b[0])


def main():
    # (a)+(c): mixed orders, jobs 2 and 3, heavy never overlaps; completes (a deadlock would hit the 60 s guard)
    for order in ("LHLLHHL", "HHLL", "LLLHLLHL", "HLHLHL"):
        for jobs in (2, 3):
            t = time.time()
            rows = run(order, jobs)
            assert len(rows) == len(order) and time.time() - t < 60, "deadlock/slow " + order
            check_heavy_alone(rows)
            # start order = list order (no overtaking)
            st = [r[0] for r in sorted(rows, key=lambda r: r[2])]
            assert st == ["f%d" % i for i in range(len(order))], (order, st)
    # (b) two light files overlap with jobs 2 (short grow, so the second may start while the first runs)
    rows = run("LLLL", 2, grow=0.05, secs=0.5)
    assert any(overlap(a, b) for a in rows for b in rows if a is not b), "no light overlap"
    # (d) jobs 1 works and never overlaps
    rows = run("LHLLH", 1)
    assert len(rows) == 5 and not any(overlap(a, b) for a in rows for b in rows if a is not b)
    # RAM gate: starts wait while free RAM is low, and resume (no deadlock)
    lowuntil = time.time() + 0.5
    rows = run("LL", 2, free=lambda: 1000 if time.time() < lowuntil else 8000)
    assert min(r[2] for r in rows) >= lowuntil - 0.01, "started with low RAM"
    # list parsing
    import tempfile
    with tempfile.TemporaryDirectory() as d:
        p = os.path.join(d, "l.txt")
        open(p, "w").write("# c\nobj/A.a.us a1\nobj/B.a.us b1 heavy x\nobj/P_EX200.a.us ex200 99\n")
        assert recordall.read_list(p) == [("obj/A.a.us", "a1", False), ("obj/B.a.us", "b1", True), ("obj/P_EX200.a.us", "ex200", True)]
    print("PASS")


main()
