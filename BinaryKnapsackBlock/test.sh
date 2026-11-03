#!/usr/bin/env bash

set -u

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cd "$script_dir" || exit 1

BinaryKnapsackBlock_test=${1:-$script_dir/BinaryKnapsackBlock_test}
solver_config=${2:-$script_dir/BSPar-times.txt}

log_dir=$script_dir/log
mkdir -p "$log_dir"

archive_logs()
{
  local suffix="N${N}_d${delta}_nW${nW}_nP${nP}_nI1_nM${nM}"
  local solver
  local solver_log_dir

  for solver in IncrementalBestFS IncrementalDFS GreedyBestFS GreedyDFS; do
    if [[ -e $solver ]]; then
      solver_log_dir="$log_dir/N${N}/${solver}"
      mkdir -p "$solver_log_dir"
      mv -- "$solver" "$solver_log_dir/${suffix}.log"
    fi
  done
}

seed=264
wchg=63
n_repeat=50

for N in 10 100 1000 10000 100000; do
  for delta in 0.005 0.05 1; do
    for nW in 0 0.5 1; do
      for nP in 0 0.5 1; do
        for nM in 0 0.5 1; do
          printf '[%s %s %s %s %s %s %s 1 %s]: ' \
            "$seed" "$wchg" "$N" "$n_repeat" "$delta" "$nW" "$nP" "$nM"

          "$BinaryKnapsackBlock_test" \
            -S "$solver_config" \
            -e "$seed" \
            -k "$wchg" \
            -N "$N" \
            -n "$n_repeat" \
            -d "$delta" \
            -W "$nW" \
            -P "$nP" \
            -i 1 \
            -M "$nM"

          retVal=$?
          archive_logs

          if [[ $retVal -ne 0 ]]; then
            exit "$retVal"
          fi
        done
      done
    done
  done
done
