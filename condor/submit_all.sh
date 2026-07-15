#!/bin/bash
# =============================================================================
#  submit_all.sh — One-shot submission driver (config-driven, per-submission dir)
#  -----------------------------------------------------------------------------
#  Reads all settings from config.sh. Each submission is isolated in its own
#  directory under $SUBMIT_ROOT/<TAG>/ containing the proxy, rendered JDL,
#  index, filelists, and logs — so concurrent / repeated submissions never
#  clobber each other.
#
#  Usage:
#      ./submit_all.sh [-t TAG] [-n] [-c CONFIG]
#        -t TAG     submission tag (default: UTC timestamp YYYYmmdd-HHMMSS)
#        -n         dry-run: show DAS results, output paths, job count, and the
#                   rendered JDL, but DO NOT submit
#        -c CONFIG  alternate config file (default: config.sh)
#
#  Build note: CMSSW_14_2_1 is EL8. Build the binary once inside cmssw-el8:
#      cmssw-el8
#      cd $CMSSW_AREA && cmsenv && make clean && make
#      exit
# =============================================================================

set -e
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd "$SCRIPT_DIR"

# ----------------------------------------------------------------------
# Parse options
# ----------------------------------------------------------------------
TAG=""
DRYRUN=0
CONFIG="config.sh"
while getopts "t:nc:h" opt; do
    case $opt in
        t) TAG="$OPTARG" ;;
        n) DRYRUN=1 ;;
        c) CONFIG="$OPTARG" ;;
        h) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "invalid option; use -h for help"; exit 64 ;;
    esac
done

# ----------------------------------------------------------------------
# Load config
# ----------------------------------------------------------------------
[ -f "$CONFIG" ] || { echo "[FATAL] config file '$CONFIG' not found"; exit 1; }
# shellcheck disable=SC1090
source "$CONFIG"

[ -z "$TAG" ] && TAG=$(date -u +%Y%m%d-%H%M%S)
SUBDIR="$SUBMIT_ROOT/$TAG"

echo "=================================================================="
echo "  TopCPVGenCategorizer condor submission"
echo "  tag      : $TAG"
echo "  dry-run  : $([ $DRYRUN -eq 1 ] && echo YES || echo no)"
echo "  config   : $CONFIG"
echo "  subdir   : $SUBDIR"
echo "  $(date)"
echo "=================================================================="

# ----------------------------------------------------------------------
# Echo resolved configuration (so dry-run shows everything)
# ----------------------------------------------------------------------
echo
echo "[config] resolved settings"
echo "  CMSSW_AREA      = $CMSSW_AREA"
echo "  BINARY          = $CMSSW_AREA/$BINARY_NAME"
echo "  EOS output base = $EOS_REDIRECTOR/$EOS_OUTBASE"
echo "  datasets file   = $DATASETS_FILE"
echo "  xrootd prefix   = $XROOTD_PREFIX"
echo "  files/chunk     = $FILES_PER_CHUNK"
echo "  worker OS       = $WORKER_OS"
echo "  job flavour     = $JOB_FLAVOUR"
echo "  resources       = ${REQUEST_CPUS} cpu, ${REQUEST_MEMORY}, ${REQUEST_DISK}"

# ----------------------------------------------------------------------
# 1. Binary check
# ----------------------------------------------------------------------
echo
echo "[1/6] Binary check"
if [ -x "$CMSSW_AREA/$BINARY_NAME" ]; then
    echo "  found: $CMSSW_AREA/$BINARY_NAME ($(stat -c%s "$CMSSW_AREA/$BINARY_NAME" 2>/dev/null || echo '?') bytes)"
else
    echo "  [WARN] binary not found at $CMSSW_AREA/$BINARY_NAME"
    echo "         build it inside cmssw-el8:  cd \$CMSSW_AREA && cmsenv && make"
    [ $DRYRUN -eq 0 ] && { echo "  [FATAL] cannot submit without binary"; exit 1; }
    echo "  (dry-run: continuing anyway)"
fi

# ----------------------------------------------------------------------
# 2. Create per-submission workspace
# ----------------------------------------------------------------------
echo
echo "[2/6] Creating submission workspace: $SUBDIR"
if [ $DRYRUN -eq 0 ]; then
    mkdir -p "$SUBDIR/filelists" "$SUBDIR/logs"
else
    echo "  (dry-run) would create $SUBDIR/{filelists,logs}"
fi

# ----------------------------------------------------------------------
# 3. Grid proxy → copy into submission dir
# ----------------------------------------------------------------------
echo
echo "[3/6] Grid proxy"
PROXY_DEST="$SUBDIR/x509up.proxy"
if [ $DRYRUN -eq 0 ]; then
    if ! voms-proxy-info -exists -valid 24:00 2>/dev/null; then
        echo "  proxy not valid for 24h — initialising"
        voms-proxy-init -voms cms -valid ${PROXY_HOURS}:00
    fi
    SRC_PROXY=$(voms-proxy-info -path)
    cp -f "$SRC_PROXY" "$PROXY_DEST"
    echo "  proxy copied → $PROXY_DEST"
    voms-proxy-info -file "$PROXY_DEST" -timeleft | sed 's/^/  timeleft(s): /'
else
    echo "  (dry-run) would copy current proxy → $PROXY_DEST"
fi

# ----------------------------------------------------------------------
# 4. Filelists from DAS
# ----------------------------------------------------------------------
echo
echo "[4/6] Generating filelists from DAS"
DAS_ARGS=(--datasets "$DATASETS_FILE"
          --outdir   "$SUBDIR/filelists"
          --files-per-chunk "$FILES_PER_CHUNK"
          --xrootd-prefix   "$XROOTD_PREFIX")
[ $DRYRUN -eq 1 ] && DAS_ARGS+=(--dry-run)
./makeFilelists.py "${DAS_ARGS[@]}"

# ----------------------------------------------------------------------
# 5. Condor index + rendered JDL
# ----------------------------------------------------------------------
echo
echo "[5/6] Building Condor index and rendering JDL"
INDEX_FILE="$SUBDIR/index_for_condor.txt"
RENDERED_JDL="$SUBDIR/submit.jdl"

if [ $DRYRUN -eq 0 ]; then
    ./makeCondorIndex.py --indir "$SUBDIR/filelists" --out "$INDEX_FILE"
else
    # In dry-run the filelists weren't written, so synthesise the index from DAS
    # counts is overkill; just note what would happen.
    echo "  (dry-run) would build $INDEX_FILE from $SUBDIR/filelists/_index.txt"
fi

# Render submit.jdl from the template, substituting config values + paths.
render_jdl() {
    cat > "$RENDERED_JDL" << JDL
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

output                  = $SUBDIR/logs/\$(short).\$(idx).\$(ClusterId).\$(ProcId).out
error                   = $SUBDIR/logs/\$(short).\$(idx).\$(ClusterId).\$(ProcId).err
log                     = $SUBDIR/logs/\$(short).\$(idx).\$(ClusterId).\$(ProcId).log

request_cpus            = $REQUEST_CPUS
request_memory          = $REQUEST_MEMORY
request_disk            = $REQUEST_DISK
+JobFlavour             = "$JOB_FLAVOUR"

notification            = never
on_exit_hold            = (ExitBySignal == True) || (ExitCode != 0)
periodic_release        = (NumJobStarts < $MAX_RETRIES) && ((CurrentTime - EnteredCurrentStatus) > 600)
max_retries             = $MAX_RETRIES

queue chunk_name, short, idx from $INDEX_FILE
JDL
}

if [ $DRYRUN -eq 0 ]; then
    render_jdl
    echo "  rendered → $RENDERED_JDL"
    NJOBS=$(wc -l < "$INDEX_FILE")
    echo "  total jobs queued: $NJOBS"
else
    # Render to a temp file for inspection (subdir not created in dry-run)
    RENDERED_JDL=$(mktemp /tmp/topcpvgencat_jdl.XXXXXX)
    render_jdl
    echo "  (dry-run) rendered JDL would be (paths shown for tag '$TAG'):"
    echo "  ----------------------------------------------------------------"
    sed 's/^/  | /' "$RENDERED_JDL"
    echo "  ----------------------------------------------------------------"
    rm -f "$RENDERED_JDL"
fi

# ----------------------------------------------------------------------
# 6. Submit (or stop for dry-run)
# ----------------------------------------------------------------------
echo
if [ $DRYRUN -eq 1 ]; then
    echo "[6/6] DRY-RUN complete — nothing submitted."
    echo
    echo "  To submit for real:   ./submit_all.sh -t $TAG"
    # Clean up the dry-run rendered jdl so it doesn't linger
    rm -f "$RENDERED_JDL"
    exit 0
fi

echo "[6/6] Submitting"
condor_submit "$RENDERED_JDL"

echo
echo "=================================================================="
echo "  Submitted under tag: $TAG"
echo "  workspace : $SUBDIR/"
echo "  proxy     : $PROXY_DEST"
echo "  logs      : $SUBDIR/logs/"
echo "  EOS output: $EOS_REDIRECTOR/$EOS_OUTBASE/<dataset>/"
echo "  Monitor   : condor_q $USER"
echo "=================================================================="

