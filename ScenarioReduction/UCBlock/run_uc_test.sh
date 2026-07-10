#!/usr/bin/env bash
set -u

# ---- defaults (override via flags) ----------------------------------------
WDIR=~/smspp-project/build/tests/ScenarioReduction/UC
IDIR=~/smspp-project/UCBlock/data/nc4
SDIR=/tmp/uc_scenarios
CFGDIR=~/smspp-project/tests/ScenarioReduction   # where the BSPar_*.txt configs live

INSTANCES="EC_CO_Test_TUB"
N_VALUES="25"
K_VALUES="5"
SEEDS="10"
METHODS="cssc"
SOLVER=BSPar_CPLEX.txt                 # bare name (looked up in CFGDIR) or a full path
VARIATION="0.5"
UNCERTAINTY="demand"                  # demand | renewable | both
OUTPUT_CSV="uc_results_seed_20.csv"

# ---- parse flags ----------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --instances)   INSTANCES="$2";   shift 2 ;;
        --n)           N_VALUES="$2";    shift 2 ;;
        --k)           K_VALUES="$2";    shift 2 ;;
        --seeds)       SEEDS="$2";       shift 2 ;;
        --methods)     METHODS="$2";     shift 2 ;;
        --solver)      SOLVER="$2";      shift 2 ;;
        --variation)   VARIATION="$2";   shift 2 ;;
        --uncertainty) UNCERTAINTY="$2"; shift 2 ;;
        --output)      OUTPUT_CSV="$2";  shift 2 ;;
        -h|--help)     grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

GEN="$WDIR/uc_scenario_generator"
TEST="$WDIR/uc_scenario_reduction_test"
[ ! -x "$GEN" ]    && { echo "ERROR: $GEN not found (build uc_scenario_generator)"; exit 1; }
[ ! -x "$TEST" ]   && { echo "ERROR: $TEST not found (build uc_scenario_reduction_test)"; exit 1; }

# allow a bare filename for solver (e.g. BSPar_CPLEX.txt): resolve under CFGDIR.
# A value containing a slash is treated as a path and used as is.
[[ "$SOLVER" != */* ]] && SOLVER="$CFGDIR/$SOLVER"
[ ! -f "$SOLVER" ] && { echo "ERROR: solver config not found: $SOLVER"; exit 1; }

# map uncertainty -> generator flag
case "$UNCERTAINTY" in
    demand)    GEN_FLAG="--no-maxpower" ;;   # only demand varies (renewable off)
    renewable) GEN_FLAG="--no-demand"   ;;   # only renewable varies (demand off)
    both)      GEN_FLAG=""              ;;
    *) echo "ERROR: --uncertainty must be demand|renewable|both"; exit 1 ;;
esac

cd "$WDIR" || exit 1
mkdir -p "$SDIR"

echo "Instance,Uncertainty,Seed,N,K,Method,Full_Obj,Reduced_Obj,Gap_Pct,RedTime_s,AlgoTime_s" > "$OUTPUT_CSV"
printf "\n%-16s %-10s %-5s %-4s %-3s %-9s %15s %15s %9s %11s %11s\n" \
    "Instance" "Uncert" "Seed" "N" "K" "Method" "Full_Obj" "Reduced_Obj" "Gap(%)" "RedTime(s)" "AlgoTime(s)"
printf '%.0s=' {1..118}; printf "\n"

for INST in $INSTANCES; do
    # resolve instance path (full path, or short name searched under IDIR)
    if [ -f "$INST" ]; then
        IPATH="$INST"
    else
        IPATH=$(find "$IDIR" -name "${INST}.nc4" 2>/dev/null | head -1)
    fi
    [ -z "$IPATH" ] && { echo "  skip: instance '$INST' not found under $IDIR"; continue; }

    for SEED in $SEEDS; do
        for N in $N_VALUES; do
            # one scenario file per (instance, uncertainty, N, seed, variation)
            SCEN="$SDIR/${INST}_${UNCERTAINTY}_n${N}_s${SEED}_v${VARIATION}.nc4"
            if [ ! -f "$SCEN" ]; then
                "$GEN" -i "$IPATH" -o "$SCEN" -n "$N" -v "$VARIATION" -s "$SEED" \
                       $GEN_FLAG --no-validate --verbose 0 >/dev/null 2>&1
            fi
            [ ! -f "$SCEN" ] && { echo "  skip: scenario gen failed ($INST N=$N seed=$SEED)"; continue; }

            for K in $K_VALUES; do
                [ "$K" -ge "$N" ] && continue
                for M in $METHODS; do
                    OUT=$("$TEST" -i "$IPATH" -f "$SCEN" -n "$N" -r "$K" -m "$M" \
                                  -c "$SOLVER" --verbose 1 2>&1)

                    FULL=$(echo "$OUT" | grep -oP 'Full\.\.\.:\s*\K[0-9.eE+-]+'        | head -1)
                    RED=$( echo "$OUT" | grep -oP 'Reduced:\s*\K[0-9.eE+-]+'           | head -1)
                    GAP=$( echo "$OUT" | grep -oP 'Gap \(absolute\):.*\(\K[0-9.]+'     | head -1)
                    AUS=$( echo "$OUT" | grep -oP 'Reduction time\.:\s*\K[0-9]+'       | head -1)  # us
                    RUS=$( echo "$OUT" | grep -oP 'Reduced solve t:\s*\K[0-9]+'        | head -1)  # us

                    # microseconds -> seconds
                    RTIME="NA";    [ -n "${RUS:-}" ] && RTIME=$(awk "BEGIN{printf \"%.3f\", $RUS/1e6}")
                    ALGOTIME="NA"; [ -n "${AUS:-}" ] && ALGOTIME=$(awk "BEGIN{printf \"%.3f\", $AUS/1e6}")
                    : ${FULL:=NA}; : ${RED:=NA}; : ${GAP:=NA}

                    printf "%-16s %-10s %-5s %-4s %-3s %-9s %15s %15s %9s %11s %11s\n" \
                        "$INST" "$UNCERTAINTY" "$SEED" "$N" "$K" "$M" "$FULL" "$RED" "$GAP" "$RTIME" "$ALGOTIME"
                    echo "$INST,$UNCERTAINTY,$SEED,$N,$K,$M,$FULL,$RED,$GAP,$RTIME,$ALGOTIME" >> "$OUTPUT_CSV"
                done
            done
        done
    done
done

printf '%.0s=' {1..118}; printf "\n"
echo "CSV saved to: $WDIR/$OUTPUT_CSV"
