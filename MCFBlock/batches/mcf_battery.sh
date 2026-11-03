#!/usr/bin/env bash
#
# mcf_battery.sh: what batch and batch-dense of MCFBlock_test have in
# common, sourced by each of them with its arguments, < exe file > < fw exe >:
# the sweep of the changes over the net and goto instances of some sizes
# [see the header of batch for the two masks of the changes].

# shared CLI/run helpers - - - - - - - - - - - - - - - - - - - - - - - - - -
source "$(dirname "${BASH_SOURCE[0]}")/../../batch_common.sh"

DATADIR=../../MCFBlock/data

# "what to change" masks: 255 = all (with arc deletions), 223 = all but
# deletions (255 & ~32); see the header of batch for the rationale
wchg_list="255 223"

MCFBlock_test=${1:-./MCFBlock_test}
fwexe=${2:-}
# the Solver of BSPar.txt but the two LEMON algorithms that are slow on
# these instances, see BSPar-large.txt
par=BSPar-large.txt

# in CI one seed of the three is run: the other two are the same sweep with
# another random stream, and the whole of it takes more than an hour there
if [ -n "$CI" ]; then seeds=1; else seeds=3; fi

# one instance, of the type, size, number, seed and changes the sweep sets
mcf_run() {
  run_test "$MCFBlock_test" "${DATADIR}/nc4/${type}${sz}_${h}.nc4" \
           -S "$par" -e "$seed" -k "$wchg"
}

# mcf_sweep "< sizes >": the instances of those sizes, 5 of each, every one
# with each seed and each mask
mcf_sweep() {
  sweep type "net goto" seed "$( seq 0 $(( seeds - 1 )) )" sz "$1" \
        h "1 2 3 4 5" wchg "${wchg_list}" -- mcf_run
}
