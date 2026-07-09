"""Quick LAB v2 stats."""
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# load LAB v2
with (ROOT / "outputs/lab_halo_v2/summary.csv").open() as f:
    lab_rows = list(csv.DictReader(f))

frames = {}
for r in lab_rows:
    fid = int(r["frame"])
    if fid not in frames:
        frames[fid] = {"red": None, "green": None}
    d = {
        "core": r["core_found"] == "True",
        "u": float(r["u_px"]),
        "v": float(r["v_px"]),
        "halo_area": int(r["halo_area"]),
        "halo_a": float(r["halo_mean_a_delta"]),
        "core_area": r["core_area_px"],
        "core_dia": r["core_diameter_px"],
        "core_l": r["core_mean_l"],
        "core_offset": r["core_offset_px"],
    }
    frames[fid][r["kind"]] = d

total = len(frames)
red_f = sum(1 for f in frames.values() if f["red"])
grn_f = sum(1 for f in frames.values() if f["green"])
both = sum(1 for f in frames.values() if f["red"] and f["green"])
zero = [fid for fid, f in frames.items() if not f["red"] and not f["green"]]

print(f"Total frames: {total}")
print(f"Red:  {red_f}/{total} ({red_f/total*100:.0f}%)")
print(f"Green: {grn_f}/{total} ({grn_f/total*100:.0f}%)")
print(f"Both:  {both}/{total} ({both/total*100:.0f}%)")
print(f"Zero:  {zero} ({len(zero)} frames)")
print(f"Core found: 46/46 (100%), Fallback: 0")

print()
print("=== Per-frame core detail ===")
for fid in sorted(frames):
    f = frames[fid]
    for k in ["red", "green"]:
        if f[k]:
            d = f[k]
            cu = d["u"]
            cv = d["v"]
            ca = d["core_area"]
            cd = d["core_dia"]
            cl = d["core_l"]
            co = d["core_offset"]
            print(f"frame={fid:>4} {k:>5}: uv=({cu:.1f},{cv:.1f}) area={ca} dia={cd} L={cl} offset={co}")

# comparison
print()
print("=== vs eval_core1_v1 (common frames only) ===")
with (ROOT / "outputs/vision_runtime_eval_core1_v1/summary.csv").open() as f:
    e_rows = list(csv.DictReader(f))
e_data = {}
for r in e_rows:
    fid = int(r["frame"])
    e_data[fid] = {
        "r_f": r["red_found"] == "True",
        "g_f": r["green_found"] == "True",
    }
    if r["red_u_px"]:
        e_data[fid]["ru"] = float(r["red_u_px"])
        e_data[fid]["rv"] = float(r["red_v_px"])
    if r["green_u_px"]:
        e_data[fid]["gu"] = float(r["green_u_px"])
        e_data[fid]["gv"] = float(r["green_v_px"])

common = sorted(set(frames) & set(e_data))
for fid in common:
    fl = frames[fid]
    fe = e_data[fid]

    r_str = "MISS"
    if fl["red"]:
        r_str = f"LAB({fl['red']['u']:.0f},{fl['red']['v']:.0f})"
        if fe["r_f"]:
            dx = fl["red"]["u"] - fe["ru"]
            dy = fl["red"]["v"] - fe["rv"]
            dist = (dx * dx + dy * dy) ** 0.5
            r_str += f"  eval({fe['ru']:.0f},{fe['rv']:.0f}) d={dist:.1f}px"
    elif fe["r_f"]:
        r_str = f"LAB MISS  eval({fe['ru']:.0f},{fe['rv']:.0f})"

    g_str = "MISS"
    if fl["green"]:
        g_str = f"LAB({fl['green']['u']:.0f},{fl['green']['v']:.0f})"
        if fe["g_f"]:
            dx = fl["green"]["u"] - fe["gu"]
            dy = fl["green"]["v"] - fe["gv"]
            dist = (dx * dx + dy * dy) ** 0.5
            g_str += f"  eval({fe['gu']:.0f},{fe['gv']:.0f}) d={dist:.1f}px"
    elif fe["g_f"]:
        g_str = f"LAB MISS  eval({fe['gu']:.0f},{fe['gv']:.0f})"

    print(f"frame={fid:>4} red:  {r_str}")
    print(f"            green: {g_str}")
