"""Quick comparison: LAB halo-guided core vs eval_core1_v1."""
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def load_lab(path: Path) -> dict[int, dict]:
    with path.open() as f:
        rows = list(csv.DictReader(f))
    frames: dict[int, dict] = {}
    for r in rows:
        fid = int(r["frame"])
        if fid not in frames:
            frames[fid] = {"red": None, "green": None}
        frames[fid][r["kind"]] = {
            "status": r["refine_status"],
            "u": float(r["u_px"]),
            "v": float(r["v_px"]),
            "halo_area": int(r["halo_area"]),
            "halo_a": float(r["halo_mean_a_delta"]),
            "core_dia": r["core_diameter_px"],
            "core_l": r["core_mean_l"],
            "core_offset": r["core_offset_px"],
        }
    return frames


def load_eval(path: Path) -> dict[int, dict]:
    with path.open() as f:
        rows = list(csv.DictReader(f))
    data = {}
    for r in rows:
        fid = int(r["frame"])
        red_found = r["red_found"] == "True"
        green_found = r["green_found"] == "True"
        data[fid] = {
            "red_found": red_found,
            "red_u": float(r["red_u_px"]) if r["red_u_px"] and red_found else None,
            "red_v": float(r["red_v_px"]) if r["red_v_px"] and red_found else None,
            "green_found": green_found,
            "green_u": float(r["green_u_px"]) if r["green_u_px"] and green_found else None,
            "green_v": float(r["green_v_px"]) if r["green_v_px"] and green_found else None,
        }
    return data


def main() -> None:
    lab = load_lab(ROOT / "outputs" / "lab_halo_guided_core_validate" / "summary.csv")
    eval_core1 = load_eval(ROOT / "outputs" / "vision_runtime_eval_core1_v1" / "summary.csv")

    common_frames = sorted(set(lab) & set(eval_core1))
    print(f"Common frames: {len(common_frames)}")

    # --- 总体统计 ---
    lab_total = len(lab)
    lab_red = sum(1 for f in lab.values() if f["red"])
    lab_grn = sum(1 for f in lab.values() if f["green"])
    lab_both = sum(1 for f in lab.values() if f["red"] and f["green"])
    lab_core = sum(
        1 for f in lab.values() for c in [f["red"], f["green"]] if c and c["status"] == "core_found"
    )
    lab_cands = sum(1 for f in lab.values() for c in [f["red"], f["green"]] if c)

    print(f"\n=== LAB halo-guided core (raw, core_diameter_min=3.0) ===")
    print(f"Frames: {lab_total}")
    print(f"Red halo:   {lab_red}/{lab_total} = {lab_red/lab_total*100:.0f}%")
    print(f"Green halo: {lab_grn}/{lab_total} = {lab_grn/lab_total*100:.0f}%")
    print(f"Both found: {lab_both}/{lab_total} = {lab_both/lab_total*100:.0f}%")
    print(f"White core: {lab_core}/{lab_cands} = {lab_core/lab_cands*100:.0f}%")
    zero_lab = [fid for fid, f in lab.items() if not f["red"] and not f["green"]]
    print(f"Zero-halo frames: {zero_lab}")

    eval_total = len(eval_core1)
    eval_red = sum(1 for f in eval_core1.values() if f["red_found"])
    eval_grn = sum(1 for f in eval_core1.values() if f["green_found"])
    eval_both = sum(1 for f in eval_core1.values() if f["red_found"] and f["green_found"])

    print(f"\n=== eval_core1_v1 (HSV, current src/vision) ===")
    print(f"Frames: {eval_total}")
    print(f"Red detected:   {eval_red}/{eval_total} = {eval_red/eval_total*100:.0f}%")
    print(f"Green detected: {eval_grn}/{eval_total} = {eval_grn/eval_total*100:.0f}%")
    print(f"Both detected:  {eval_both}/{eval_total} = {eval_both/eval_total*100:.0f}%")

    # --- 逐帧对比 ---
    print(f"\n=== Frame-by-frame comparison (common frames only) ===")
    lab_match = 0
    eval_match = 0
    for fid in common_frames:
        fl = lab[fid]
        fe = eval_core1[fid]

        def row(kind, lab_d, eval_found, eval_u, eval_v):
            nonlocal lab_match, eval_match
            if lab_d:
                lab_str = f"{(lab_d['u']):.0f},{(lab_d['v']):.0f} [{lab_d['status']}]"
                if eval_found:
                    lab_match += 1
            else:
                lab_str = "MISS"
            if eval_found:
                eval_str = f"found ({eval_u:.0f},{eval_v:.0f})"
                if lab_d:
                    eval_match += 1
            else:
                eval_str = "MISS"
            return lab_str, eval_str

        r_lab, r_eval = row("red", fl["red"], fe["red_found"], fe["red_u"], fe["red_v"])
        g_lab, g_eval = row("green", fl["green"], fe["green_found"], fe["green_u"], fe["green_v"])
        print(f"  {fid:>4}  red  LAB: {r_lab:>22}  |  eval: {r_eval}")
        print(f"        green LAB: {g_lab:>22}  |  eval: {g_eval}")

    print(f"\n=== Core-found detail frames ===")
    for fid in sorted(lab):
        fl = lab[fid]
        for kind in ["red", "green"]:
            c = fl[kind]
            if c and c["status"] == "core_found":
                print(
                    f"  frame {fid:>4} {kind}: center=({c['u']:.1f},{c['v']:.1f}) "
                    f"halo_area={c['halo_area']} halo_a_delta={c['halo_a']:.1f} "
                    f"core_dia={c['core_dia']} core_l={c['core_l']} "
                    f"offset={c['core_offset']}"
                )


if __name__ == "__main__":
    main()
