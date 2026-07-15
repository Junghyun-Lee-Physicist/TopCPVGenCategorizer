#!/usr/bin/env python3
# =============================================================================
#  makeCondorIndex.py
#  -----------------------------------------------------------------------------
#  Reads <indir>/_index.txt  (chunk_name  dataset_short)  and writes
#  <out>                     (chunk_name, dataset_short, chunk_idx)
#  in the comma-separated form Condor's 'queue from' expects.
#
#  Usage:
#      ./makeCondorIndex.py [--indir filelists] [--out index_for_condor.txt]
# =============================================================================

import argparse
import os
import re
import sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--indir", default="filelists",
                    help="directory containing _index.txt (default: filelists)")
    ap.add_argument("--out", default="index_for_condor.txt",
                    help="output index file for Condor 'queue from'")
    args = ap.parse_args()

    src = os.path.join(args.indir, "_index.txt")
    dst = args.out

    if not os.path.exists(src):
        print(f"[FATAL] {src} not found. Run ./makeFilelists.py first.",
              file=sys.stderr)
        sys.exit(1)

    rx = re.compile(r"_chunk(\d+)\.txt$")

    n = 0
    with open(src) as fi, open(dst, "w") as fo:
        for line in fi:
            line = line.strip()
            if not line:
                continue
            toks = line.split()
            if len(toks) < 2:
                continue
            chunk_name, dataset_short = toks[0], toks[1]
            m = rx.search(chunk_name)
            if not m:
                print(f"  [skip] cannot parse chunk index from {chunk_name}",
                      file=sys.stderr)
                continue
            idx = int(m.group(1))
            fo.write(f"{chunk_name}, {dataset_short}, {idx}\n")
            n += 1

    print(f"  wrote {dst}  ({n} jobs)")


if __name__ == "__main__":
    main()

