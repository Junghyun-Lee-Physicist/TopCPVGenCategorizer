#!/bin/bash
# =============================================================================
#  runJob.sh — Condor worker wrapper for TopCPVGenCategorizer
#  -----------------------------------------------------------------------------
#  Arguments (positional, all required):
#      $1 = chunk_filelist     (e.g. TTToSemiLeptonic_2017_chunk000.txt)
#      $2 = dataset_short      (e.g. TTToSemiLeptonic_2017)
#      $3 = chunk_index        (e.g. 0)
#      $4 = eos_output_dir     (e.g. /eos/user/j/junghyun/ttbar_gencat/2017)
#
#  Environment:
#      Worker node OS is pinned to EL8 via 'MY.WantOS = "el8"' in submit.jdl,
#      so no cmssw-el8 container wrapper is needed here. The job sets up the
#      user's CMSSW_14_2_1 release area on AFS via cmsenv, which provides
#      ROOT + g++ consistent with the pre-built binary.
# =============================================================================

set -e
set -o pipefail
echo "============================================================"
echo " TopCPVGenCategorizer Condor job"
echo "   started:  $(date)"
echo "   host:     $(hostname)"
echo "   OS:       $(cat /etc/redhat-release 2>/dev/null || echo unknown)"
echo "   workdir:  $(pwd)"
echo "============================================================"

# -----------------------------------------------------------------------
# Parse args
# -----------------------------------------------------------------------
if [ $# -lt 4 ]; then
    echo "[FATAL] usage: $0 <filelist> <dataset_short> <chunk_index> <eos_outdir>"
    exit 64
fi
CHUNK_FILELIST="$1"
DATASET_SHORT="$2"
CHUNK_INDEX="$3"
EOS_OUTDIR="$4"

echo "  CHUNK_FILELIST  = $CHUNK_FILELIST"
echo "  DATASET_SHORT   = $DATASET_SHORT"
echo "  CHUNK_INDEX     = $CHUNK_INDEX"
echo "  EOS_OUTDIR      = $EOS_OUTDIR"
echo

# -----------------------------------------------------------------------
# Environment setup — CMSSW release area on AFS (requires EL8 worker)
# -----------------------------------------------------------------------
CMSSW_AREA=/afs/cern.ch/user/j/junghyun/CMSSW_14_2_1/src/TopCPVGenCategorizer

echo "[setup] sourcing CMSSW environment from $CMSSW_AREA"
source /cvmfs/cms.cern.ch/cmsset_default.sh
cd "$CMSSW_AREA"
eval `scramv1 runtime -sh`     # = cmsenv
echo "[setup] CMSSW_BASE   = $CMSSW_BASE"
echo "[setup] ROOT version = $(root-config --version)"
echo "[setup] g++ version  = $(g++ --version | head -1)"

# Return to the Condor scratch directory for actual work
cd - > /dev/null

# Use the proxy from Condor's x509userproxy
if [ -n "$X509_USER_PROXY" ]; then
    echo "[setup] using X509_USER_PROXY = $X509_USER_PROXY"
else
    echo "[WARN] X509_USER_PROXY not set — xrootd reads may fail"
fi

# -----------------------------------------------------------------------
# Locate binary — prefer the one in the CMSSW area (built with cmsenv ROOT)
# -----------------------------------------------------------------------
if [ -x "$CMSSW_AREA/TopCPVGenCategorizer" ]; then
    BINARY="$CMSSW_AREA/TopCPVGenCategorizer"
    echo "[input] using binary from CMSSW area: $BINARY"
elif [ -f TopCPVGenCategorizer ]; then
    BINARY="./TopCPVGenCategorizer"
    chmod +x TopCPVGenCategorizer
    echo "[input] using transferred binary: $BINARY"
else
    echo "[FATAL] TopCPVGenCategorizer binary not found (neither in $CMSSW_AREA nor transferred)"
    exit 65
fi

[ -f "$CHUNK_FILELIST" ] || { echo "[FATAL] chunk filelist not transferred"; exit 66; }

mkdir -p input
cp "$CHUNK_FILELIST" "input/$(basename $CHUNK_FILELIST)"

NFILES=$(wc -l < "input/$(basename $CHUNK_FILELIST)")
echo "[input] $NFILES files in chunk"

# -----------------------------------------------------------------------
# Run categorizer
# -----------------------------------------------------------------------
OUT_NAME="${DATASET_SHORT}_chunk$(printf '%03d' $CHUNK_INDEX).root"
echo
echo "[run] starting categorizer  → $OUT_NAME"
echo "------------------------------------------------------------"
time "$BINARY" "$(basename $CHUNK_FILELIST)" "$OUT_NAME"
RC=$?
echo "------------------------------------------------------------"
echo "[run] exit code: $RC"

if [ $RC -ne 0 ] || [ ! -f "$OUT_NAME" ]; then
    echo "[FATAL] categorizer failed or no output produced"
    exit 67
fi

ls -la "$OUT_NAME"

# -----------------------------------------------------------------------
# Stage output to EOS via xrdcp
# -----------------------------------------------------------------------
EOS_DEST="${EOS_OUTDIR}/${DATASET_SHORT}"
EOS_URL="root://eosuser.cern.ch/${EOS_DEST}"
echo
echo "[stage] creating EOS destination ${EOS_DEST}"
xrdfs root://eosuser.cern.ch mkdir -p "${EOS_DEST}" || true

echo "[stage] copying ${OUT_NAME} → ${EOS_URL}/"
xrdcp -f "$OUT_NAME" "${EOS_URL}/${OUT_NAME}"
RC=$?
if [ $RC -ne 0 ]; then
    echo "[FATAL] xrdcp failed (RC=$RC)"
    exit 68
fi

xrdfs root://eosuser.cern.ch stat "${EOS_DEST}/${OUT_NAME}" \
    && echo "[stage] verified on EOS" \
    || { echo "[FATAL] post-stage stat failed"; exit 69; }

# -----------------------------------------------------------------------
# Cleanup
# -----------------------------------------------------------------------
rm -f "$OUT_NAME"
rm -rf input

echo
echo "============================================================"
echo " Job done.  $(date)"
echo "============================================================"
exit 0

