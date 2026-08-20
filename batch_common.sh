#!/usr/bin/env bash
# tests/batch_common.sh
#
# Shared helpers for SMS++ test batch scripts. Each batch should:
#   1. Source this file: source "$(dirname "$0")/../../batch_common.sh"
#      (adjust the relative path to the batch's depth)
#   2. Set: DEFAULT_EXE, DEFAULT_PAR (and optionally DEFAULT_SLV, DEFAULT_PAR2)
#   3. Call parse_batch_args "$@" to populate $exe / $par / $slv / $mlf
#      from the positional args (with fall-back to defaults)
#   4. Loop over instances, calling run_test "$exe" args... for each. The
#      function appends to $mlf if set, exits on non-zero retVal, and
#      preserves the printed "[<instance> <par> <slv> <ref>]: " prefix.
#
# The argument layout standardised here is:
#   < exe file >  = path of the executable, default: $DEFAULT_EXE
#   < par file >  = BlockSolverConfig file, default: $DEFAULT_PAR
#   < solver >    = 0 or 1, default: $DEFAULT_SLV -- ONLY for the batches that
#                   set DEFAULT_SLV, i.e. that select the Solver from the
#                   command line; where the BlockSolverConfig attaches every
#                   Solver to cross-check, there is nothing to select and the
#                   slot does not exist
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
# Reads the positional args from the caller. Sets globals:
#   exe : argv[1] or $DEFAULT_EXE
#   par : argv[2] or $DEFAULT_PAR
#   slv : argv[3] or $DEFAULT_SLV, only if the batch sets DEFAULT_SLV
#   mlf : the argument past the last one used above, or unset (no log)
#
# The < solver > slot is there only for the batches that declare DEFAULT_SLV;
# for the others the log file is argv[3], so that a batch never carries an
# argument it has no use for.

parse_batch_args() {
    exe="${1:-${DEFAULT_EXE}}"
    par="${2:-${DEFAULT_PAR}}"
    if [ -n "${DEFAULT_SLV+set}" ]; then
        slv="${3:-${DEFAULT_SLV}}"
        mlf="${4:-}"
    else
        slv=""
        mlf="${3:-}"
    fi

    if [ -z "${exe}" ]; then
        echo "batch_common: DEFAULT_EXE not set and no exe argument given" >&2
        exit 1
    fi
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# run a single test invocation and check its return value
#
# Usage: print_header <label>
# Prints the "[<label>]:" header on its own line (to $mlf if set, else stdout),
# so the per-round output of the test below it starts on a fresh line. Batches
# whose displayed label differs from the actual executable arguments, or that
# do not exit on error, can reuse this directly.
print_header() {
    if [ -z "${mlf}" ]; then
        printf "[%s]:\n" "$1"
    else
        printf "[%s]:\n" "$1" >> "${mlf}"
    fi
}

# Usage: run_test <exe> <args...>
# Effects:
#   - prints the "[<args>]:" header on its own line (via print_header)
#   - tees stdout/stderr to $mlf if set (else stdout only)
#   - exits 1 if the invocation returns non-zero
#
# The extended per-round log is enabled uniformly, for every test, via the
# `verbose` environment variable (e.g. `verbose=1 ./batch ...` or
# `verbose=1 ctest ...`): the test binaries read it from the inherited
# environment, so it works regardless of whether a test understands the -v
# option. Do NOT append -v here: tests that parse positional arguments by hand
# would mis-read it.

run_test() {
    local _exe=$1
    shift
    print_header "$*"
    if [ -z "${mlf}" ]; then
        "${_exe}" "$@"
    else
        "${_exe}" "$@" >> "${mlf}"
    fi
    local _rv=$?
    if [ ${_rv} -ne 0 ]; then
        exit 1
    fi
}
