#!/usr/bin/env python3
# =============================================================================
#  makeFilelists.py
#  -----------------------------------------------------------------------------
#  Queries DAS for each dataset listed in datasets.txt and writes per-chunk
#  filelists into ./filelists/. Each chunk contains FILES_PER_CHUNK input
#  files. One Condor job is then submitted per chunk.
#
#  Usage:
#      ./makeFilelists.py [--files-per-chunk N] [--datasets datasets.txt]
#                        [--outdir filelists] [--xrootd-prefix PREFIX]
#                        [--dry-run]
#
#  Requirements (lxplus default has them):
#      - dasgoclient (from /cvmfs/cms.cern.ch/common)
#      - valid Grid proxy:   voms-proxy-init -voms cms
#
#  Output:
#      filelists/<dataset_short>_chunk000.txt   (one path per line)
#      filelists/<dataset_short>_chunk001.txt
#      ...
#      filelists/_index.txt   (chunk → dataset_short mapping, one line each)
# =============================================================================

import argparse
import os
import sys
import subprocess
import shutil
from pathlib import Path


def run_das(query):
    """Return list of strings from dasgoclient query, or [] on failure."""
    try:
        out = subprocess.check_output(
            ["dasgoclient", "--query", query],
            stderr=subprocess.STDOUT,
            text=True,
            timeout=300,
        )
    except subprocess.CalledProcessError as e:
        print(f"  [ERROR] DAS query failed: {query}", file=sys.stderr)
        print(e.output, file=sys.stderr)
        return []
    except FileNotFoundError:
        print("  [FATAL] dasgoclient not found. Run on lxplus, or source\n"
              "          /cvmfs/cms.cern.ch/cmsset_default.sh first.",
              file=sys.stderr)
        sys.exit(2)
    except subprocess.TimeoutExpired:
        print(f"  [ERROR] DAS query timed out: {query}", file=sys.stderr)
        return []
    return [line.strip() for line in out.splitlines() if line.strip()]


def read_datasets(path):
    """Parse datasets.txt → list of (short_name, das_path)."""
    pairs = []
    with open(path) as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            tokens = line.split()
            if len(tokens) < 2:
                continue
            pairs.append((tokens[0], tokens[1]))
    return pairs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--datasets", default="datasets.txt",
                    help="path to dataset list (default: datasets.txt)")
    ap.add_argument("--outdir", default="filelists",
                    help="output directory for per-chunk filelists")
    ap.add_argument("--files-per-chunk", type=int, default=8,
                    help="number of input files per Condor job (default: 8)")
    ap.add_argument("--xrootd-prefix",
                    default="root://cms-xrd-global.cern.ch/",
                    help="prefix prepended to /store/... paths")
    ap.add_argument("--dry-run", action="store_true",
                    help="print actions but do not create files")
    args = ap.parse_args()

    # -------------------------------------------------------------------
    # Check inputs
    # -------------------------------------------------------------------
    if not os.path.exists(args.datasets):
        print(f"[FATAL] {args.datasets} not found.", file=sys.stderr)
        sys.exit(1)

    datasets = read_datasets(args.datasets)
    if not datasets:
        print(f"[FATAL] {args.datasets} contains no datasets.", file=sys.stderr)
        sys.exit(1)

    if not shutil.which("dasgoclient"):
        print("[WARN] dasgoclient not in PATH. Make sure you run this on lxplus\n"
              "       after sourcing /cvmfs/cms.cern.ch/cmsset_default.sh", file=sys.stderr)

    # -------------------------------------------------------------------
    # Prepare output dir
    # -------------------------------------------------------------------
    outdir = Path(args.outdir)
    if not args.dry_run:
        outdir.mkdir(parents=True, exist_ok=True)

    index_lines = []
    total_chunks = 0
    total_files  = 0

    # -------------------------------------------------------------------
    # Process each dataset
    # -------------------------------------------------------------------
    for short_name, das_path in datasets:
        print(f"\n→ querying DAS for {short_name}")
        print(f"  dataset = {das_path}")

        files = run_das(f"file dataset={das_path}")
        if not files:
            print(f"  [SKIP] no files returned for {short_name}")
            continue

        # Prepend xrootd prefix (DAS returns /store/... paths)
        full_paths = [args.xrootd_prefix + f if f.startswith("/store/") else f
                      for f in files]

        n = len(full_paths)
        nchunk = (n + args.files_per_chunk - 1) // args.files_per_chunk
        print(f"  found {n} files → {nchunk} chunks")

        for ic in range(nchunk):
            lo = ic * args.files_per_chunk
            hi = min(lo + args.files_per_chunk, n)
            chunk_name = f"{short_name}_chunk{ic:03d}.txt"
            chunk_path = outdir / chunk_name

            if args.dry_run:
                print(f"  (dry-run) would write {chunk_path}  ({hi-lo} files)")
            else:
                with open(chunk_path, "w") as f:
                    for p in full_paths[lo:hi]:
                        f.write(p + "\n")

            index_lines.append(f"{chunk_name}  {short_name}")
            total_chunks += 1

        total_files += n

    # -------------------------------------------------------------------
    # Write index
    # -------------------------------------------------------------------
    index_path = outdir / "_index.txt"
    if args.dry_run:
        print(f"\n(dry-run) would write {index_path}  ({len(index_lines)} entries)")
    else:
        with open(index_path, "w") as f:
            for line in index_lines:
                f.write(line + "\n")

    # -------------------------------------------------------------------
    # Summary
    # -------------------------------------------------------------------
    print(f"\n========================================")
    print(f"  datasets processed: {len(datasets)}")
    print(f"  total input files : {total_files}")
    print(f"  total chunks (jobs): {total_chunks}")
    print(f"  files/chunk        : {args.files_per_chunk}")
    print(f"  index file         : {index_path}")
    print(f"========================================")


if __name__ == "__main__":
    main()

