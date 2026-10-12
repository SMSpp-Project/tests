#!/usr/bin/env bash
#
# qf_battery.sh: what the batteries of MILPSolver_QuadFunction_test have in
# common, sourced by each of them with its argument, < exe file >; the
# battery then calls qf_run on its family of instances.

# shared CLI/run helpers - - - - - - - - - - - - - - - - - - - - - - - - - -
source "$(dirname "${BASH_SOURCE[0]}")/../../../batch_common.sh"

MILPSolver_QuadFunction_test=${1:-./MILPSolver_QuadFunction_test}

# instances whose dense quadratic term exhausts the CI runners' memory
CI_OOM_SKIP="QPLib_4270.lp QPLib_8845.lp QPLib_8906.lp QPLib_8991.lp"

# qf_run < directory > < type >: the test on every instance of the directory,
# C (continuous) or I (integer)
qf_run() {
  local what
  for what in "$1"/*; do

    # a few dense-Q instances need more memory than the CI runners provide
    # (the solve gets OOM-killed there): skip them in CI only, keeping local
    # coverage. Difficulty tracks the density of Q, not the file size.
    case " $CI_OOM_SKIP " in
      *" $(basename "$what") "*) [ -n "$CI" ] && continue ;;
    esac

    run_test "$MILPSolver_QuadFunction_test" -t "$2" "$what"

  done
}
