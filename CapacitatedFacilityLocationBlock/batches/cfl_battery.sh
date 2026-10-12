#!/usr/bin/env bash
#
# cfl_battery.sh: what the batteries of CFL_test have in common, sourced by
# each of them with its arguments, < exe file > < log file >; the battery
# then sets
#   vars     the list of its variants [see CFLBVar*.txt]
#   wchg     what is changed at each round [see CFL_test --help]
#   maxseed  the last seed of the changes, from 0
# and calls the functions below on its instances. Every run is CFL_test with
# no Slope Scaling and 40 rounds of changes, once per seed.

# shared CLI/run helpers - - - - - - - - - - - - - - - - - - - - - - - - - -
source "$(dirname "${BASH_SOURCE[0]}")/../../batch_common.sh"

# run from the test root (one level up), where the config files are
cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 1

DATADIR=../../CapacitatedFacilityLocationBlock/data

CFL_test=${1:-./CFL_test}
mlf=${2:-}

niter=0
rounds=40

# cfl_run < instance > < type > [ option ... ]: the runs of an instance, one
# per seed, with the options of the battery and those given
cfl_run() {
  local what=$1 type=$2 seed
  shift 2
  for (( seed = 0 ; seed <= maxseed ; seed++ )); do
    run_test "$CFL_test" "$what" -F "$vars" -t "$type" -s "$niter" \
             -e "$seed" -k "$wchg" -n "$rounds" "$@"
  done
}

# cfl_orlib [ number ... ]: the ORLib instances capNNN of the numbers given,
# or all of them
cfl_orlib() {
  local f
  if [ $# -eq 0 ]; then
    for f in ${DATADIR}/txt/ORLib/*.txt; do cfl_run "$f" c; done
  else
    for f in "$@"; do cfl_run "${DATADIR}/txt/ORLib/cap${f}.txt" c; done
  fi
}

# the small ORLib instances, those of the batteries that also solve the
# integer problem or a Lagrangian dual
cfl_orlib_small=( 102 113 124 42 62 73 91 103 114 131 43 63 74 92 104 121
                  132 44 64 81 93 111 122 134 51 71 82 94 112 123 41 61 72
                  84 )

# cfl_yang: the Yang instances, 5 of each size
cfl_yang() {
  local i h
  for i in 30-200 60-200 60-300 80-400; do
    for (( h = 1 ; h <= 5 ; h++ )); do
      cfl_run "${DATADIR}/txt/Yang/${i}/${i}-${h}.txt" l
    done
  done
}

# cfl_tbed < h ... >: the TBED instances of size 300 with the numbers given
cfl_tbed() {
  local h
  for h in "$@"; do
    cfl_run "${DATADIR}/txt/TBED/300/i300_${h}.txt" f
  done
}
