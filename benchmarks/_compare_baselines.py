import json
import sys
from pathlib import Path

def load(path):
    with open(path) as f:
        data = json.load(f)
    return {b["name"]: b for b in data["benchmarks"]}

def fmt_time(b):
    t = b["cpu_time"]
    u = b.get("time_unit", "ns")
    if u == "ns":
        return t, "ns"
    if u == "us":
        return t * 1e3, "ns"
    if u == "ms":
        return t * 1e6, "ns"
    return t, u

def main(a_label, b_label):
    root = Path(__file__).parent
    a = load(root / f"baseline-{a_label}-ecshotpath.json")
    b = load(root / f"baseline-{b_label}-ecshotpath.json")

    keys = sorted(set(a) | set(b))
    rows = []
    for name in keys:
        if name not in a or name not in b:
            continue
        ta, _ = fmt_time(a[name])
        tb, _ = fmt_time(b[name])
        pct = (tb / ta - 1) * 100 if ta else 0
        rows.append((abs(pct), name, ta, tb, pct))

    rows.sort(reverse=True)
    print(f"{'benchmark':<55} {'refactor':>12} {'tier1':>12} {'delta':>8}")
    print("-" * 90)
    for _, name, ta, tb, pct in rows[:25]:
        sign = "+" if pct >= 0 else ""
        print(f"{name:<55} {ta:12.1f} {tb:12.1f} {sign}{pct:6.1f}%")

    improved = sum(1 for r in rows if r[4] < -1)
    regressed = sum(1 for r in rows if r[4] > 1)
    print(f"\nTotal: {len(rows)} benchmarks | improved (>1%): {improved} | regressed (>1%): {regressed}")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
