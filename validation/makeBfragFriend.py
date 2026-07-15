#!/usr/bin/env python3
# =============================================================================
#  makeBfragFriend.py — recover the *real* B-fragmentation weights for a
#  NanoAOD-based GenCatTree by joining them from the original MiniAOD ntuple.
#
#  WHY THIS EXISTS
#  ---------------
#  The B-fragmentation systematic weights (Frag_Central/Up/Down, Peterson,
#  Semilep_BrUp/Down) are produced in the MiniAOD SSBAnalyzer by the custom
#  CMSSW module `bfragWgtProducer`. They are NOT present in standard NanoAOD,
#  and they cannot be recomputed from NanoAOD content (the per-jet B-hadron
#  fragmentation function reweighting needs information dropped during NanoAOD
#  production). The PSWeight (ISR/FSR) branches in GenCatTree are a *parton
#  shower* proxy, which is a different systematic — not a substitute.
#
#  Since the weights already exist in your MiniAOD SSBAnalyzer output, the
#  correct way to "maximally recover" the MiniAOD output is to JOIN them onto
#  the GenCatTree by the event identifier (run, luminosityBlock, event), then
#  attach the result as a ROOT friend tree (TTree::AddFriend). After that, the
#  analysis sees Bfrag_* alongside the GenCat branches as if they were one tree.
#
#  WHAT IT DOES
#  ------------
#    1. Reads (run, lumi, event) + the 6 B-frag weights from the MiniAOD tree,
#       building an in-memory dictionary keyed by (run, lumi, event).
#    2. Walks the GenCatTree in its existing event order and, for each entry,
#       looks up the weights by its (run, lumi, event) key.
#    3. Writes a friend tree `BfragFriend` with EXACTLY the same number of
#       entries, in the SAME order as GenCatTree, so AddFriend aligns 1:1.
#    4. Reports match statistics (matched / missing / key collisions).
#
#  The friend tree is written to its own file; GenCatTree is never modified.
#
#  USAGE
#  -----
#    python makeBfragFriend.py \
#        --nano   gencat_output.root        --nano-tree  GenCatTree \
#        --mini   ssb_miniaod_ntuple.root   --mini-tree  SSBTree \
#        --out    bfrag_friend.root
#
#    # then in analysis / ROOT:
#    #   TFile* f  = TFile::Open("gencat_output.root");
#    #   TTree* t  = (TTree*)f->Get("GenCatTree");
#    #   t->AddFriend("BfragFriend", "bfrag_friend.root");
#    #   t->Draw("Bfrag_Central");   // now available alongside GenPar_* etc.
#
#  BRANCH NAMES
#  ------------
#  MiniAOD weight branch names vary between SSBAnalyzer versions. Override the
#  defaults with --mini-weight-map if yours differ (see --help). The defaults
#  follow the bfragWgtProducer convention documented for this analysis.
# =============================================================================

import argparse
import sys

import numpy as np

try:
    import uproot
except ImportError:
    sys.exit("[FATAL] uproot is required: pip install uproot awkward")


# Default mapping: output friend branch  ->  MiniAOD source branch.
# Override individual entries with --mini-weight-map "Out=Source,Out2=Source2".
DEFAULT_WEIGHT_MAP = {
    "Bfrag_Central":     "Frag_Cen_Weight",
    "Bfrag_Up":          "Frag_Up_Weight",
    "Bfrag_Down":        "Frag_Down_Weight",
    "Bfrag_Peterson":    "Frag_Peterson_Weight",
    "Bfrag_SemilepUp":   "Semilep_BrUp_Weight",
    "Bfrag_SemilepDown": "Semilep_BrDown_Weight",
}

# Event-id branch names. NanoAOD/GenCatTree uses run/luminosityBlock/event.
# MiniAOD SSBTree commonly uses Info_EventNumber/Info_RunNumber/Info_Lumi or
# similar; override with --mini-id-branches if needed.
NANO_ID = ("run", "luminosityBlock", "event")


def parse_kv(s):
    """Parse 'a=b,c=d' into a dict."""
    out = {}
    if not s:
        return out
    for piece in s.split(","):
        piece = piece.strip()
        if not piece:
            continue
        if "=" not in piece:
            sys.exit(f"[FATAL] bad key=value: '{piece}'")
        k, v = piece.split("=", 1)
        out[k.strip()] = v.strip()
    return out


def main():
    ap = argparse.ArgumentParser(
        description="Build a B-fragmentation weight friend tree for GenCatTree "
                    "by joining weights from the MiniAOD ntuple on (run,lumi,event).")
    ap.add_argument("--nano", required=True, help="GenCatTree ROOT file")
    ap.add_argument("--nano-tree", default="GenCatTree", help="tree name (default GenCatTree)")
    ap.add_argument("--mini", required=True, help="MiniAOD SSBAnalyzer ROOT file (has B-frag weights)")
    ap.add_argument("--mini-tree", default="SSBTree", help="MiniAOD tree name (default SSBTree)")
    ap.add_argument("--out", required=True, help="output friend ROOT file")
    ap.add_argument("--friend-tree", default="BfragFriend", help="friend tree name (default BfragFriend)")
    ap.add_argument("--mini-id-branches", default="",
                    help="MiniAOD (run,lumi,event) branch names as 'run=...,lumi=...,event=...'. "
                         "If omitted, tries common names automatically.")
    ap.add_argument("--mini-weight-map", default="",
                    help="override weight branches as 'Bfrag_Central=...,Bfrag_Up=...'")
    ap.add_argument("--default-weight", type=float, default=1.0,
                    help="weight written when a GenCat event has no MiniAOD match (default 1.0)")
    ap.add_argument("--step", type=int, default=1_000_000, help="batch size for reading")
    args = ap.parse_args()

    weight_map = dict(DEFAULT_WEIGHT_MAP)
    weight_map.update(parse_kv(args.mini_weight_map))

    # ---- resolve MiniAOD id branch names ------------------------------------
    mini = uproot.open(args.mini)[args.mini_tree]
    mini_keys = set(mini.keys())

    id_override = parse_kv(args.mini_id_branches)
    if id_override:
        mini_run, mini_lumi, mini_evt = (id_override.get("run"),
                                         id_override.get("lumi"),
                                         id_override.get("event"))
    else:
        # try a list of common conventions
        def pick(cands):
            for c in cands:
                if c in mini_keys:
                    return c
            return None
        mini_run  = pick(["run", "Info_RunNumber", "Run", "runNumber", "Evt_Run"])
        mini_lumi = pick(["luminosityBlock", "Info_Lumi", "Lumi", "lumiBlock", "Evt_Lumi"])
        mini_evt  = pick(["event", "Info_EventNumber", "Event", "eventNumber", "Evt_Event"])

    missing_id = [n for n, v in [("run", mini_run), ("lumi", mini_lumi), ("event", mini_evt)] if v is None]
    if missing_id:
        sys.exit(f"[FATAL] could not resolve MiniAOD id branch(es): {missing_id}. "
                 f"Pass --mini-id-branches 'run=...,lumi=...,event=...'. "
                 f"Available keys (sample): {sorted(list(mini_keys))[:40]}")

    missing_w = [src for src in weight_map.values() if src not in mini_keys]
    if missing_w:
        sys.exit(f"[FATAL] MiniAOD tree missing weight branch(es): {missing_w}. "
                 f"Override with --mini-weight-map. Available: {sorted(list(mini_keys))[:60]}")

    print(f"[info] MiniAOD id branches : run='{mini_run}' lumi='{mini_lumi}' event='{mini_evt}'")
    print(f"[info] weight branches     : {weight_map}")

    out_names = list(weight_map.keys())
    src_names = [weight_map[n] for n in out_names]

    # ---- build the lookup dictionary from MiniAOD ---------------------------
    # key = (run<<...) packed into a python int via tuple; value = tuple of weights
    print("[info] indexing MiniAOD weights ...")
    table = {}
    n_mini = 0
    dup = 0
    for batch in mini.iterate([mini_run, mini_lumi, mini_evt] + src_names,
                              step_size=args.step, library="np"):
        runs = batch[mini_run].astype(np.uint64)
        lumis = batch[mini_lumi].astype(np.uint64)
        evts = batch[mini_evt].astype(np.uint64)
        ws = [batch[s].astype(np.float32) for s in src_names]
        for i in range(len(runs)):
            key = (int(runs[i]), int(lumis[i]), int(evts[i]))
            val = tuple(float(w[i]) for w in ws)
            if key in table:
                dup += 1
            table[key] = val
            n_mini += 1
    print(f"[info] indexed {n_mini} MiniAOD rows, {len(table)} unique keys"
          + (f", {dup} duplicate keys (last wins)" if dup else ""))

    # ---- walk GenCatTree and emit aligned friend ----------------------------
    nano = uproot.open(args.nano)[args.nano_tree]
    nano_keys = set(nano.keys())
    for idb in NANO_ID:
        if idb not in nano_keys:
            sys.exit(f"[FATAL] GenCatTree lacks '{idb}'. Re-run TopCPVGenCategorizer "
                     f"with the event-id branches enabled (v1.5+).")

    n_nano = nano.num_entries
    print(f"[info] GenCatTree has {n_nano} entries; matching ...")

    # output arrays, one per friend branch, aligned to GenCatTree order
    out_arrays = {name: np.empty(n_nano, dtype=np.float32) for name in out_names}
    matched = 0
    missing = 0
    pos = 0
    for batch in nano.iterate(list(NANO_ID), step_size=args.step, library="np"):
        runs = batch[NANO_ID[0]].astype(np.uint64)
        lumis = batch[NANO_ID[1]].astype(np.uint64)
        evts = batch[NANO_ID[2]].astype(np.uint64)
        for i in range(len(runs)):
            key = (int(runs[i]), int(lumis[i]), int(evts[i]))
            v = table.get(key)
            if v is None:
                missing += 1
                for name in out_names:
                    out_arrays[name][pos] = args.default_weight
            else:
                matched += 1
                for j, name in enumerate(out_names):
                    out_arrays[name][pos] = v[j]
            pos += 1

    assert pos == n_nano, f"internal: wrote {pos} of {n_nano}"
    frac = (100.0 * matched / n_nano) if n_nano else 0.0
    print(f"[info] matched {matched}/{n_nano} ({frac:.2f}%); "
          f"{missing} unmatched (filled with {args.default_weight})")
    if missing and matched == 0:
        print("[WARN] zero matches — check that the MiniAOD and NanoAOD are the "
              "SAME dataset/era and that id branch names are correct.")

    # ---- write friend (same #entries, same order → AddFriend aligns 1:1) ----
    with uproot.recreate(args.out) as fout:
        fout[args.friend_tree] = out_arrays
    print(f"[done] wrote friend tree '{args.friend_tree}' to {args.out}")
    print("\nAttach in ROOT / analysis:")
    print(f'    TTree* t = (TTree*)TFile::Open("{args.nano}")->Get("{args.nano_tree}");')
    print(f'    t->AddFriend("{args.friend_tree}", "{args.out}");')
    print(f'    t->Draw("{out_names[0]}");')


if __name__ == "__main__":
    main()
