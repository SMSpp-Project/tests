#!/usr/bin/env bash
#
# mmcf_battery.sh: what the batteries of MMCFBlock_test have in common,
# sourced by each of them with its argument, < exe file >; the battery then
# calls the functions below on its instances.

# shared CLI/run helpers - - - - - - - - - - - - - - - - - - - - - - - - - -
source "$(dirname "${BASH_SOURCE[0]}")/../../batch_common.sh"

DATADIR=data

MMCFBlock_test=${1:-./MMCFBlock_test}

# mmcf_canad < BlockConfig > < BlockSolverConfig > ...: every Canad instance,
# with that BlockConfig and with each of the BlockSolverConfig in turn
mmcf_canad() {
  local bpar=$1 what spar
  shift
  for what in ${DATADIR}/Canad/*.dat; do
    for spar in "$@"; do
      run_test "$MMCFBlock_test" "$what" -B "$bpar" -S "$spar" -t s
    done
  done
}

# mmcf_mnetgen: the Mnetgen instances of 64 and 128 nodes, 12 of each number
# of commodities
mmcf_mnetgen() {
  local i j h
  for i in 64 128; do
    for j in 4 8 16 32 64 128; do
      [ "$j" -gt "$i" ] && continue
      for (( h = 1 ; h < 13 ; h++ )); do
        run_test "$MMCFBlock_test" "${DATADIR}/Mnetgen/$i-$j-$h" \
                 -B BPar.txt -S BSPar.txt -t m
      done
    done
  done
}
