#!/bin/bash
# =============================================================================
#  resubmit_failed.sh — Resubmit only the BAD chunks of a submission
#  -----------------------------------------------------------------------------
#  Delegates the "which chunks are bad" decision to checkOutputs.sh, which
#  classifies every chunk as DONE / RUNNING / BAD using three sources
#  (expected list, condor job state, EOS output integrity). Only BAD chunks
#  are resubmitted; RUNNING chunks are deliberately skipped so we never
#  double-submit a job that is still in flight.
#
#  Usage:
#      ./resubmit_failed.sh -t TAG [-n] [-c CONFIG] [-v LEVEL]
#        -t TAG     submission tag to resubmit
#        -n         dry-run: classify and show what would be resubmitted
#        -c CONFIG  alternate config (default: config.sh)
#        -v LEVEL   verification level passed to checkOutputs.sh
#                   (exists | size | root; default from config VERIFY_LEVEL)
# =============================================================================

set -e
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd "$SCRIPT_DIR"

TAG=""
DRYRUN=0
CONFIG="config.sh"
VERIFY_OVERRIDE=""
while getopts "t:nc:v:h" opt; do
    case $opt in
        t) TAG="$OPTARG" ;;
        n) DRYRUN=1 ;;
        c) CONFIG="$OPTARG" ;;
        v) VERIFY_OVERRIDE="$OPTARG" ;;
        h) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "invalid option; use -h"; exit 64 ;;
    esac
done

[ -f "$CONFIG" ] || { echo "[FATAL] config '$CONFIG' not found"; exit 1; }
# shellcheck disable=SC1090
source "$CONFIG"

[ -z "$TAG" ] && { echo "[FATAL] -t TAG is required"; exit 64; }
SUBDIR="$SUBMIT_ROOT/$TAG"
[ -d "$SUBDIR" ] || { echo "[FATAL] submission dir not found: $SUBDIR"; exit 1; }

# ----------------------------------------------------------------------
# 1. Classify chunks via checkOutputs.sh → writes BAD index.
# ----------------------------------------------------------------------
BAD_INDEX="$SUBDIR/index_bad.txt"
CHECK_ARGS=(-t "$TAG" -c "$CONFIG" -w "$BAD_INDEX")
[ -n "$VERIFY_OVERRIDE" ] && CHECK_ARGS+=(-v "$VERIFY_OVERRIDE")

echo "Running checkOutputs.sh to classify chunks ..."
echo "------------------------------------------------------------------"
# checkOutputs exits with N_BAD; don't let set -e abort us on nonzero.
set +e
./checkOutputs.sh "${CHECK_ARGS[@]}"
set -e
echo "------------------------------------------------------------------"

if [ ! -s "$BAD_INDEX" ]; then
    echo "No BAD chunks — nothing to resubmit."
    exit 0
fi
NBAD=$(wc -l < "$BAD_INDEX")

if [ $DRYRUN -eq 1 ]; then
    echo
    echo "(dry-run) would resubmit $NBAD chunk(s):"
    sed 's/^/  | /' "$BAD_INDEX"
    exit 0
fi

# ----------------------------------------------------------------------
# 2. Render a resubmit JDL pointing at the BAD index and submit.
# ----------------------------------------------------------------------
RESUB_JDL="$SUBDIR/submit_resubmit.jdl"
PROXY_DEST="$SUBDIR/x509up.proxy"

# Refresh proxy if needed
if ! voms-proxy-info -exists -valid 24:00 2>/dev/null; then
    echo "proxy not valid for 24h — initialising"
    voms-proxy-init -voms cms -valid ${PROXY_HOURS}:00
fi
cp -f "$(voms-proxy-info -path)" "$PROXY_DEST"

cat > "$RESUB_JDL" << JDL
universe                = vanilla
executable              = $SCRIPT_DIR/runJob.sh
MY.WantOS               = "$WORKER_OS"
arguments               = \$(chunk_name) \$(short) \$(idx) $EOS_OUTBASE
transfer_input_files    = $SUBDIR/filelists/\$(chunk_name)
should_transfer_files   = YES
when_to_transfer_output = ON_EXIT
transfer_output_files   = ""
use_x509userproxy       = true
x509userproxy           = $PROXY_DEST
output                  = $SUBDIR/logs/\$(short).\$(idx).resub.\$(ClusterId).\$(ProcId).out
error                   = $SUBDIR/logs/\$(short).\$(idx).resub.\$(ClusterId).\$(ProcId).err
log                     = $SUBDIR/logs/\$(short).\$(idx).resub.\$(ClusterId).\$(ProcId).log
request_cpus            = $REQUEST_CPUS
request_memory          = $REQUEST_MEMORY
request_disk            = $REQUEST_DISK
+JobFlavour             = "$JOB_FLAVOUR"
notification            = never
on_exit_hold            = (ExitBySignal == True) || (ExitCode != 0)
max_retries             = $MAX_RETRIES
queue chunk_name, short, idx from $BAD_INDEX
JDL

condor_submit "$RESUB_JDL"
echo "Resubmitted $NBAD chunk(s) under tag $TAG."

