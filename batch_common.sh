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
# would mis-read it. A number asks for that level, `verbose=2 ./batch ...`
# being what gives the log of each Solver, which is the only way to have it
# from inside a battery.

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# set an algorithmic parameter in a ComputeConfig file, in place
#
#   cfg_set_par < file > < name > < value >
#
# adds the parameter to the file if it is not there, replaces its value if it
# is, and keeps the count of the parameters of that section right. Which
# section is used is read off the name, i.e. "int..." goes among the integer
# parameters and "dbl..." among the double ones; any other prefix is an error.
# The file is changed in place, so whoever calls this is expected to keep a
# copy of the original and to put it back at the end [see batch-aggr].

cfg_set_par() {
    local _file=$1 _name=$2 _value=$3 _kind

    case "${_name}" in
        int*) _kind="integer" ;;
        dbl*) _kind="double" ;;
        *) echo "cfg_set_par: ${_name} is neither int... nor dbl..." >&2
           return 1 ;;
    esac

    if grep -q "^${_name}[[:space:]]" "${_file}"; then
        sed -i "s/^${_name}[[:space:]].*/${_name} ${_value}/" "${_file}"
        return 0
    fi

    # not there: add it right after the line declaring how many there are,
    # and increase that number by one
    awk -v name="${_name}" -v value="${_value}" -v kind="${_kind}" '
        $0 ~ ("^[0-9]+ # number of " kind " parameters") && ! done {
            print $1 + 1 " # number of " kind " parameters"
            print ""
            print name " " value
            done = 1
            next
        }
        { print }
    ' "${_file}" > "${_file}.tmp" && mv "${_file}.tmp" "${_file}"
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# run a test and report how long it took
#
#   run_timed < tag > < exe file > args...
#
# does what run_test does, and prints one line "<tag> time <seconds>" after
# it, so that a batch sweeping a parameter leaves a log out of which the
# times can be read without the solver having to print anything.

run_timed() {
    local _tag=$1
    shift
    local _t0 _t1
    _t0=$( date +%s.%N )
    run_test "$@"
    _t1=$( date +%s.%N )
    echo "${_tag} time $( awk -v a="${_t0}" -v b="${_t1}" 'BEGIN{ printf "%.2f" , b - a }' )"
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# the Frank-Wolfe decomposition of the same instance
#
#   fw_run < instance file > [ further arguments ]
#
# A battery that walks the instances of a Block can hand each of them to the
# generic Frank-Wolfe tester as well, which it receives as its second
# executable ($fwexe): the instance is read K times into a father Block whose
# Objective couples the copies, and the value FrankWolfeSolver computes by
# decomposing it is cross-checked against the monolithic :MILPSolver of the
# same configuration. The configurations are those of the FW directory of the
# suite, which -c makes every nested name resolve into while the working
# directory stays the one of the suite, where the instances are; $fwpar is the
# BlockSolverConfig of the father inside it and $fwargs whatever else the
# Block asks for (the BlockConfig of the formulation, the variable groups, the
# father objective). Nothing is run if the battery was given no such
# executable, i.e., if FrankWolfeSolver is not in the build.

fw_run() {
    [ -n "${fwexe:-}" ] && [ -x "${fwexe}" ] || return 0
    run_test "${fwexe}" -c "${fwdir:-FW}" -S "${fwpar:-BSPar.txt}" \
             ${fwargs:-} "$@"
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

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
