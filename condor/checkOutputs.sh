#!/bin/bash
# =============================================================================
#  checkOutputs.sh — classify the status of every chunk in a submission
#  -----------------------------------------------------------------------------
#  For a given submission TAG, cross-checks three independent sources:
#     (1) the expected chunk list      (submissions/<TAG>/index_for_condor.txt)
#     (2) HTCondor job state/history   (running? queued? exit code?)
#     (3) the EOS output files         (exists? size OK? opens in ROOT?)
#
#  and classifies each chunk into exactly one bucket:
#     DONE      — valid output on EOS (passes VERIFY_LEVEL)
#     RUNNING   — a condor job for this chunk is still idle/running/held
#     BAD       — output missing, too small, or unreadable, and not running
#
#  Only BAD chunks are real candidates for resubmission. RUNNING chunks are left
#  alone so we never double-submit.
#
#  Usage:
#      ./checkOutputs.sh -t TAG [-c CONFIG] [-v LEVEL] [-w OUTFILE]
#        -t TAG       submission tag (the submissions/<TAG> dir)
#        -c CONFIG    config file (default: config.sh)
#        -v LEVEL     override VERIFY_LEVEL: exists | size | root
#        -w OUTFILE   write the BAD-chunk index to OUTFILE (for resubmit)
#                     default: submissions/<TAG>/index_bad.txt
#
#  Exit code: number of BAD chunks (0 = all good), capped at 250.
# =============================================================================

set -u
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd "$SCRIPT_DIR"

TAG=""
CONFIG="config.sh"
VERIFY_OVERRIDE=""
OUTFILE=""
while getopts "t:c:v:w:h" opt; do
    case $opt in
        t) TAG="$OPTARG" ;;
        c) CONFIG="$OPTARG" ;;
        v) VERIFY_OVERRIDE="$OPTARG" ;;
        w) OUTFILE="$OPTARG" ;;
        h) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "invalid option; use -h"; exit 64 ;;
    esac
done

[ -f "$CONFIG" ] || { echo "[FATAL] config '$CONFIG' not found"; exit 64; }
# shellcheck disable=SC1090
source "$CONFIG"
[ -n "$VERIFY_OVERRIDE" ] && VERIFY_LEVEL="$VERIFY_OVERRIDE"

[ -z "$TAG" ] && { echo "[FATAL] -t TAG is required"; exit 64; }
SUBDIR="$SUBMIT_ROOT/$TAG"
INDEX_FULL="$SUBDIR/index_for_condor.txt"
[ -f "$INDEX_FULL" ] || { echo "[FATAL] $INDEX_FULL not found"; exit 64; }
[ -z "$OUTFILE" ] && OUTFILE="$SUBDIR/index_bad.txt"

echo "=================================================================="
echo "  checkOutputs — tag: $TAG"
echo "  verify level : $VERIFY_LEVEL   (exists|size|root)"
echo "  EOS base     : $EOS_REDIRECTOR/$EOS_OUTBASE"
echo "  min size     : $MIN_OUTPUT_SIZE bytes"
echo "=================================================================="

# ----------------------------------------------------------------------
# (2) Gather the set of chunks that condor still considers active.
#     We match on the Args string which contains "<chunk_name> <short> <idx>".
#     A chunk is "RUNNING" if it appears in condor_q (idle/run/held).
# ----------------------------------------------------------------------
echo
echo "[1/3] Querying condor for active jobs ..."
ACTIVE_CHUNKS=""
if command -v condor_q >/dev/null 2>&1; then
    # ProcId-agnostic: pull the Args of all of the user's jobs
    ACTIVE_CHUNKS=$(condor_q "$USER" -af Args 2>/dev/null \
                    | awk '{print $1}' | grep -v '^$' || true)
    NACT=$(echo "$ACTIVE_CHUNKS" | grep -c . || echo 0)
    echo "  $NACT active (idle/running/held) job(s) seen in condor_q"
else
    echo "  [WARN] condor_q not available — RUNNING detection disabled"
fi

# ----------------------------------------------------------------------
# (3) Build the set of EOS output files once (single listing).
# ----------------------------------------------------------------------
echo
echo "[2/3] Listing EOS outputs ..."
# Map: filename(without dir) -> size. We get this from a recursive ls -l.
# xrdfs ls -l format: "flags  size  date time  /full/path"
declare -A EOS_SIZE
if command -v xrdfs >/dev/null 2>&1; then
    while read -r line; do
        [ -z "$line" ] && continue
        fpath=$(echo "$line" | awk '{print $NF}')
        fsize=$(echo "$line" | awk '{print $(NF-3)}')
        case "$fpath" in
            *_chunk*.root)
                base=$(basename "$fpath")
                EOS_SIZE["$base"]="$fsize"
                ;;
        esac
    done < <(xrdfs "$EOS_REDIRECTOR" ls -l -R "$EOS_OUTBASE" 2>/dev/null || true)
    echo "  found ${#EOS_SIZE[@]} output .root file(s) on EOS"
else
    echo "  [WARN] xrdfs not available — cannot verify EOS outputs"
fi

# ----------------------------------------------------------------------
# Helper: ROOT-level verification of a single EOS file.
# Returns 0 if the tree opens and has >0 entries.
# ----------------------------------------------------------------------
root_ok() {
    local url="$1"
    command -v root >/dev/null 2>&1 || return 0   # if no ROOT, skip (treat as ok-by-size)
    local n
    n=$(root -l -b -q "$url" \
         -e "TTree*t=(TTree*)_file0->Get(\"$OUTPUT_TREE_NAME\"); printf(\"NENT=%lld\\n\", t? t->GetEntries() : -1);" \
         2>/dev/null | sed -n 's/^NENT=//p')
    [ -n "$n" ] && [ "$n" -gt 0 ] 2>/dev/null
}

# ----------------------------------------------------------------------
# (1) Walk the expected chunk list and classify each.
# ----------------------------------------------------------------------
echo
echo "[3/3] Classifying chunks ..."
> "$OUTFILE"

N_DONE=0; N_RUNNING=0; N_BAD=0
BAD_DETAIL=""

while IFS= read -r line; do
    [ -z "$line" ] && continue
    chunk=$(echo "$line"  | cut -d, -f1 | tr -d ' ')      # *.txt
    short=$(echo "$line"  | cut -d, -f2 | tr -d ' ')
    base="${chunk%.txt}"                                   # drop .txt
    rootname="${base}.root"
    eos_url="$EOS_REDIRECTOR/$EOS_OUTBASE/$short/$rootname"

    # Is a condor job for this chunk still active?
    if echo "$ACTIVE_CHUNKS" | grep -qx "$chunk"; then
        N_RUNNING=$((N_RUNNING+1))
        continue
    fi

    # Output present?
    size="${EOS_SIZE[$rootname]:-}"
    if [ -z "$size" ]; then
        N_BAD=$((N_BAD+1)); echo "$line" >> "$OUTFILE"
        BAD_DETAIL+="  MISSING   $short/$rootname\n"
        continue
    fi

    # Size check
    if [ "$VERIFY_LEVEL" != "exists" ]; then
        if [ "$size" -lt "$MIN_OUTPUT_SIZE" ] 2>/dev/null; then
            N_BAD=$((N_BAD+1)); echo "$line" >> "$OUTFILE"
            BAD_DETAIL+="  TOOSMALL  $short/$rootname (${size}B < ${MIN_OUTPUT_SIZE}B)\n"
            continue
        fi
    fi

    # ROOT open / entry check
    if [ "$VERIFY_LEVEL" = "root" ]; then
        if ! root_ok "$eos_url"; then
            N_BAD=$((N_BAD+1)); echo "$line" >> "$OUTFILE"
            BAD_DETAIL+="  UNREADABLE $short/$rootname (tree missing or 0 entries)\n"
            continue
        fi
    fi

    N_DONE=$((N_DONE+1))
done < "$INDEX_FULL"

TOTAL=$((N_DONE + N_RUNNING + N_BAD))

# ----------------------------------------------------------------------
# Report
# ----------------------------------------------------------------------
echo
echo "=================================================================="
echo "  Summary for tag '$TAG'  (total expected: $TOTAL)"
echo "    DONE     : $N_DONE   (valid output)"
echo "    RUNNING  : $N_RUNNING   (still active in condor — left alone)"
echo "    BAD      : $N_BAD   (missing/truncated/unreadable, not running)"
echo "=================================================================="
if [ "$N_BAD" -gt 0 ]; then
    echo
    echo "  BAD chunk detail:"
    echo -e "$BAD_DETAIL" | sed '/^$/d'
    echo
    echo "  BAD-chunk index written to: $OUTFILE"
    echo "  Resubmit with:  ./resubmit_failed.sh -t $TAG"
fi

# exit code = number of bad chunks (capped so it stays a valid 0-255 code)
[ "$N_BAD" -gt 250 ] && exit 250
exit "$N_BAD"

