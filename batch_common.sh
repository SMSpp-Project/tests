#!/usr/bin/env bash
# tests/batch_common.sh
#
# Shared helpers for SMS++ test batch scripts. Each batch should:
#   1. Source this file: source "$(dirname "$0")/../../batch_common.sh"
#      (adjust the relative path to the batch's depth)
#   2. Set: DEFAULT_EXE, DEFAULT_PAR (and optionally DEFAULT_SLV, DEFAULT_PAR2)
#   3. Call parse_batch_args "$@" to populate $exe / $par / $slv / $mlf
#      from positional args 1..4 (with fall-back to defaults)
#   4. Loop over instances, calling run_test "$exe" args... for each. The
#      function appends to $mlf if set, exits on non-zero retVal, and
#      preserves the printed "[<instance> <par> <slv> <ref>]: " prefix.
#
# The argument layout standardised here is:
#   < exe file >  = path of the executable, default: $DEFAULT_EXE
#   < par file >  = BlockSolverConfig file, default: $DEFAULT_PAR
#   < solver >    = 0 or 1, default: $DEFAULT_SLV (or 0)
#   < log file >  = output log file (no log if absent)
#
# Some batches use two configs (a BlockConfig + a BlockSolverConfig) instead
# of (par, slv); they should set DEFAULT_PAR2 and ignore $slv at run-time.

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# colors

if [ -t 1 ]; then
    RED='\033[31m'
    GREEN='\033[32m'
    NC='\033[0m'
else
    RED=''
    GREEN=''
    NC=''
fi

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# parse standard batch arguments
#
# Reads positional args $1..$4 from the caller. Sets globals:
#   exe : argv[1] or $DEFAULT_EXE
#   par : argv[2] or $DEFAULT_PAR
#   slv : argv[3] or $DEFAULT_SLV or "0"
#   mlf : argv[4] or unset (no log)

parse_batch_args() {
    exe="${1:-${DEFAULT_EXE}}"
    par="${2:-${DEFAULT_PAR}}"
    slv="${3:-${DEFAULT_SLV:-0}}"
    mlf="${4:-}"

    if [ -z "${exe}" ]; then
        echo "batch_common: DEFAULT_EXE not set and no exe argument given" >&2
        exit 1
    fi
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# run a single test invocation and check its return value
#
# Usage: run_test <exe> <args...>
# Effects:
#   - prints "[<args>]: " prefix (matches pre-existing batch convention)
#   - tees stdout/stderr to $mlf if set (else stdout only)
#   - exits 1 if the invocation returns non-zero

run_test() {
    local _exe=$1
    shift
    if [ -z "${mlf}" ]; then
        printf "[%s]: " "$*"
        "${_exe}" "$@"
    else
        printf "[%s]: " "$*" >> "${mlf}"
        "${_exe}" "$@" >> "${mlf}"
    fi
    local _rv=$?
    if [ ${_rv} -ne 0 ]; then
        exit 1
    fi
}
