#!/usr/bin/env bash
set -u

GENDIR=~/smspp-project/build/tests/ScenarioReduction/CFL
SOLVEDIR=~/smspp-project/build/tests/ScenarioReduction/UC
IDIR=~/smspp-project/CapacitatedFacilityLocationBlock/data/nc4/ORLib
SDIR=/tmp/cfl_scenarios
CFGDIR=~/smspp-project/tests/ScenarioReduction   # where the BSPar_*.txt configs live

INSTANCES="cap102"
N_VALUES="200"
K_VALUES="5 10"
METHODS="baseline dupacova bestfit firstfit cssc"
SOLVER="BSPar_CPLEX.txt"
SEED=1
VARIATION="0.5"
OUTPUT_CSV="insample_results_cap102_seed_1_200.csv"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --instances) INSTANCES="$2"; shift 2 ;;
        --n)         N_VALUES="$2";  shift 2 ;;
        --k)         K_VALUES="$2";  shift 2 ;;
        --methods)   METHODS="$2";   shift 2 ;;
        --solver)    SOLVER="$2";    shift 2 ;;
        --seed)      SEED="$2";      shift 2 ;;
        --variation) VARIATION="$2"; shift 2 ;;
        --output)    OUTPUT_CSV="$2";shift 2 ;;
        *) shift ;;
    esac
done

# allow a bare filename for solver (e.g. BSPar_CPLEX.txt): resolve under CFGDIR.
# A value containing a slash is treated as a path and used as is.
[[ "$SOLVER" != */* ]] && SOLVER="$CFGDIR/$SOLVER"
[ ! -f "$SOLVER" ] && { echo "ERROR: solver config not found: $SOLVER"; exit 1; }

mkdir -p "$SDIR"

echo "Instance,N,K,Method,Full_Obj,Reduced_Obj,Gap_Pct,RedTime_s,AlgoTime_s" > "$OUTPUT_CSV"
printf "\n%-8s %-5s %-4s %-10s %16s %16s %10s %15s %15s\n" \
    "Instance" "N" "K" "Method" "Full_Obj" "Reduced_Obj" "Gap(%)" "RedTime(s)" "AlgoTime(s)"
printf '%.0s=' {1..106}; printf "\n"

for INST in $INSTANCES; do
    INSTANCE="$IDIR/${INST}.nc4"

    for N in $N_VALUES; do
        TSSB="$SDIR/${INST}_n${N}_s${SEED}_v${VARIATION}_tssb.nc4"
        [ ! -f "$TSSB" ] && (
            cd "$GENDIR" || exit 1
            ./CFLScenarioGenerator -i "$INSTANCE" \
                -o "$SDIR/${INST}_n${N}_s${SEED}_v${VARIATION}.nc4" \
                --tssb-output "$TSSB" \
                -n "$N" -s "$SEED" -v "$VARIATION" \
                --no-validate --verbose 0 >/dev/null 2>&1
        )

        for K in $K_VALUES; do
            [ "$K" -ge "$N" ] && continue
            for M in $METHODS; do
                OUT=$(cd "$SOLVEDIR" && ./scenario_reduction_solve -i "$TSSB" -r "$K" -m "$M" -c "$SOLVER" 2>&1)

                FULL=$(echo "$OUT" | grep -oP 'Full\s+\(N=[0-9]+\):\s*\K[0-9.eE+-]+' | head -1)
                RED=$(echo "$OUT"  | grep -oP 'Reduced \(K=[0-9]+\):\s*\K[0-9.eE+-]+' | head -1)
                GAP=$(echo "$OUT"  | grep -oP 'Gap:\s*\K[0-9.eE+-]+' | head -1)
                RTIME_MS=$(echo "$OUT" | grep -oP 'Reduced \(K=[0-9]+\):[^(]*\(\K[0-9.]+(?=\s*ms\))' | head -1)
                : ${FULL:=NA}; : ${RED:=NA}; : ${GAP:=NA}; : ${RTIME_MS:=NA}

                # RedTime_s/AlgoTime_s: the generic solver only reports one
                # timing (the reduction step itself, in ms); kept in both
                # columns for CSV-format compatibility with old runs.
                if [ "$RTIME_MS" != "NA" ]; then
                    RTIME=$(awk -v ms="$RTIME_MS" 'BEGIN{printf "%.4f", ms/1000}')
                else
                    RTIME=NA
                fi
                ALGOTIME="$RTIME"

                printf "%-8s %-5s %-4s %-10s %16s %16s %10s %15s %15s\n" \
                    "$INST" "$N" "$K" "$M" "$FULL" "$RED" "$GAP" "$RTIME" "$ALGOTIME"
                echo "$INST,$N,$K,$M,$FULL,$RED,$GAP,$RTIME,$ALGOTIME" >> "$OUTPUT_CSV"
            done
        done
    done
done
