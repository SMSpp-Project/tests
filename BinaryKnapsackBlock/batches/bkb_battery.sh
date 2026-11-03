#!/usr/bin/env bash
#
# bkb_battery.sh: the run of a generated instance of the batteries of
# BinaryKnapsackBlock_test that sweep its parameters, sourced by each of
# them after batch_common.sh [see sweep()]

# bkb_run: one generated instance, of the parameters the sweep sets and of
# the seed, changes, rounds and integer fraction of the battery
bkb_run() {
  run_test "$exe" -S "$par" -E "$EPS" -e "$seed" -k "$wchg" -N "$N" \
           -n "$n_repeat" -d "$delta" -W "$nW" -P "$nP" -i "$nI" -M "$nM"
}
