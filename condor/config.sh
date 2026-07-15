# =============================================================================
#  config.sh — central configuration for TopCPVGenCategorizer condor submission
#  -----------------------------------------------------------------------------
#  All submission-level settings live here. submit_all.sh and the helper
#  scripts source this file, so you only edit ONE place.
#
#  Edit the values below to match your setup, then run ./submit_all.sh.
# =============================================================================

# ----------------------------------------------------------------------
# CMSSW release area where the TopCPVGenCategorizer binary is built.
# The Condor worker does `cmsenv` here and runs $CMSSW_AREA/TopCPVGenCategorizer.
# ----------------------------------------------------------------------
CMSSW_AREA="/afs/cern.ch/user/j/junghyun/CMSSW_14_2_1/src/TopCPVGenCategorizer"

# Binary name (= Makefile PROGRAM)
BINARY_NAME="TopCPVGenCategorizer"

# ----------------------------------------------------------------------
# EOS output base. Per-dataset subdirectories are created underneath:
#     $EOS_OUTBASE/<dataset_short>/<dataset_short>_chunkNNN.root
# ----------------------------------------------------------------------
EOS_OUTBASE="/eos/user/j/junghyun/ttbar_gencat/2017"

# EOS xrootd redirector (eosuser for /eos/user, eoscms for /eos/cms)
EOS_REDIRECTOR="root://eosuser.cern.ch"

# ----------------------------------------------------------------------
# Dataset list — text file with  <short_name>  <DAS_path>  per line.
# ----------------------------------------------------------------------
DATASETS_FILE="datasets.txt"

# xrootd redirector prepended to /store/... input file paths
XROOTD_PREFIX="root://cms-xrd-global.cern.ch/"

# Number of input NanoAOD files per Condor job
FILES_PER_CHUNK=8

# ----------------------------------------------------------------------
# Condor resource requests
# ----------------------------------------------------------------------
REQUEST_CPUS=1
REQUEST_MEMORY="2 GB"
REQUEST_DISK="4 GB"
JOB_FLAVOUR="workday"          # espresso/microcentury/longlunch/workday/tomorrow/testmatch
WORKER_OS="el8"                # MY.WantOS — matches CMSSW_14_2_1 (el8_amd64_gcc12)
MAX_RETRIES=3

# ----------------------------------------------------------------------
# Submission workspace root. Each submission gets its own subdirectory:
#     $SUBMIT_ROOT/<TAG>/
#         ├── x509up.proxy
#         ├── submit.jdl           (rendered)
#         ├── index_for_condor.txt
#         ├── filelists/
#         └── logs/
# TAG defaults to a UTC timestamp; override with:  ./submit_all.sh -t mytag
# ----------------------------------------------------------------------
SUBMIT_ROOT="submissions"

# Grid proxy validity to request if a fresh one is needed
PROXY_HOURS=192

# ----------------------------------------------------------------------
# Output verification (used by checkOutputs.sh and resubmit_failed.sh)
# ----------------------------------------------------------------------
# Minimum acceptable output .root size in bytes. A valid GenCatTree output
# for an 8-file chunk is typically tens of MB; anything below this is treated
# as a truncated / failed transfer. 100 KB is a safe floor.
MIN_OUTPUT_SIZE=102400

# Name of the output TTree (used for ROOT entry-count verification)
OUTPUT_TREE_NAME="GenCatTree"

# Verification depth for checkOutputs.sh:
#   exists  — file present on EOS
#   size    — exists AND size >= MIN_OUTPUT_SIZE          (fast)
#   root    — size AND opens in ROOT with >0 tree entries (thorough, slower)
VERIFY_LEVEL="root"

