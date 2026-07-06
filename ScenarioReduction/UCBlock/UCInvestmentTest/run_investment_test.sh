#!/usr/bin/env bash
# run_investment_test.sh -- runs uc_investment_test over TSSB instances.
# Usage: ./run_investment_test.sh [-s solver] [-f format] [instance ...]
set -u

# --- defaults ---------------------------------------------------------------
SOLVER="cplex" # we could choose grb, highs, scip
OUTPUT_FORMAT="table" # we could choose csv, both

# --- paths (edit BUILD_DIR if your build tree is elsewhere) ----------------
SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"          # UCBlock/UCInvestmentTest
SR_ROOT="$(cd "$SELF_DIR/../.." && pwd)"                          # tests/ScenarioReduction
BUILD_DIR="${BUILD_DIR:-$HOME/smspp-project/build}"
BIN="$BUILD_DIR/tests/ScenarioReduction/UC/uc_investment_test"
INSTANCE_DIR="$SELF_DIR/tests_instances"
RESULTS_DIR="$SELF_DIR/results"

# --- available solver configs -----------------------------------------------
# detect all BSPar_*.txt files
declare -A SOLVER_CONFIGS=()
for cfg_file in "$SR_ROOT"/BSPar_*.txt; do
  if [[ -f "$cfg_file" ]]; then
    solver_name=$(basename "$cfg_file" .txt | sed 's/BSPar_//' | tr '[:upper:]' '[:lower:]')
    SOLVER_CONFIGS[$solver_name]="$cfg_file"
  fi
done
# default if no configs found
if [[ ${#SOLVER_CONFIGS[@]} -eq 0 ]]; then
  SOLVER_CONFIGS[cplex]="$SR_ROOT/BSPar_CPLEX.txt"
fi

# --- helper: extract comparison table from .out file and save as CSV -------
extract_table_to_csv() {
  local out_file="$1"
  local csv_file="${out_file%.out}.csv"
  local instance_name=$(basename "$out_file" .out)

  # extract reference value and label from "=== Comparison" header
  local ref_line=$(grep "=== Comparison" "$out_file" | head -1)
  local ref_label=$(echo "$ref_line" | grep -oE 'v\*|WS bound' | head -1)
  local ref_val=$(echo "$ref_line" | grep -oE '[0-9.e+-]+' | tail -1)

  # write CSV header
  {
    echo "Instance,Method,Pick,Reduced_Obj,InSample_Gap_Abs,InSample_Gap_Pct,Impl_Cost,ImplErr_Abs,ImplErr_Pct,Reference,RefLabel"

    # parse the comparison table via regex
    # Line format: method pick red_obj gap_abs (gap_pct%) impl_cost err_abs (err_pct%)
    grep -E '^\s*(baseline|dupacova|bestfit|firstfit|cssc)' "$out_file" | \
    awk -v inst="$instance_name" -v ref="$ref_val" -v reflab="$ref_label" '
      match($0, /^\s*([a-z]+)\s+([0-9]+)\s+([0-9.e+-]+)\s+([0-9.e+-]+)\s+\(([^)%]+)%\)\s+([0-9.e+-]+)\s+([0-9.e+-]+)\s+\(([^)%]+)%\)/, arr) {
        method=arr[1]; pick=arr[2]; red=arr[3]; gap_abs=arr[4]; gap_pct=arr[5]
        impl=arr[6]; err_abs=arr[7]; err_pct=arr[8]
        printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n", \
          inst, method, pick, red, gap_abs, gap_pct, impl, err_abs, err_pct, ref, reflab
      }
    '
  } > "$csv_file"

  echo "  (CSV saved: $csv_file)"
}

# --- usage help -----
show_help() {
  local solvers=$(printf '%s, ' "${!SOLVER_CONFIGS[@]}" | sed 's/, $//')
  echo "Usage: ./run_investment_test.sh [-s solver] [-f table|csv|both] [instance ...]"
  echo "  -s SOLVER   $solvers (default: cplex)"
  echo "  -f FORMAT   table (default), csv, both"
  exit 0
}

# --- parse options ----------------------------------------------------------
INSTANCES_ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -s|--solver)
      SOLVER="${2,,}"  # convert to lowercase
      if [[ ! -v SOLVER_CONFIGS[$SOLVER] ]]; then
        echo "ERROR: unknown solver '$SOLVER'. Available: ${!SOLVER_CONFIGS[@]}" >&2
        exit 1
      fi
      shift 2
      ;;
    -f|--format)
      OUTPUT_FORMAT="${2,,}"
      if [[ ! "$OUTPUT_FORMAT" =~ ^(table|csv|both)$ ]]; then
        echo "ERROR: format must be table, csv, or both" >&2
        exit 1
      fi
      shift 2
      ;;
    -h|--help)
      show_help
      ;;
    -*)
      echo "ERROR: unknown option '$1'" >&2
      exit 1
      ;;
    *)
      INSTANCES_ARGS+=("$1")
      shift
      ;;
  esac
done

CFG="${SOLVER_CONFIGS[$SOLVER]}"
if [[ ! -f "$CFG" ]]; then
  echo "ERROR: solver config not found: $CFG" >&2
  echo "Available solvers: ${!SOLVER_CONFIGS[@]}" >&2
  exit 1
fi

if [[ ! -x "$BIN" ]]; then
  echo "ERROR: binary not found/executable: $BIN" >&2
  echo "Build it first:  cmake --build \"$BUILD_DIR\" --target uc_investment_test -j1" >&2
  exit 1
fi
mkdir -p "$RESULTS_DIR"

# --- resolve the list of instances ----------------------------------------
declare -a INSTANCES=()
if [[ ${#INSTANCES_ARGS[@]} -eq 0 ]]; then
  while IFS= read -r f; do INSTANCES+=("$f"); done \
    < <(ls "$INSTANCE_DIR"/*2050*snap24*.nc 2>/dev/null | sort)
else
  for arg in "${INSTANCES_ARGS[@]}"; do
    if [[ -f "$arg" ]]; then
      INSTANCES+=("$arg")                                   # explicit path
    else
      while IFS= read -r f; do INSTANCES+=("$f"); done \
        < <(ls "$INSTANCE_DIR"/*"$arg"*.nc 2>/dev/null | sort)   # name fragment
    fi
  done
fi

if [[ ${#INSTANCES[@]} -eq 0 ]]; then
  echo "No matching instances found." >&2
  exit 1
fi

# --- run each instance -----------------------------------------------------
echo "Binary : $BIN"
echo "Solver : $SOLVER"
echo "Config : $CFG"
echo "Format : $OUTPUT_FORMAT"
echo "Results: $RESULTS_DIR"
echo "Instances: ${#INSTANCES[@]}"
echo

for F in "${INSTANCES[@]}"; do
  name="$(basename "$F" .nc)"
  out="$RESULTS_DIR/$name.out"
  echo "=============================================================="
  echo ">>> $name"
  echo "=============================================================="
  # keep the comparison table on screen
  "$BIN" -i "$F" -c "$CFG" >"$out" 2>&1
  status=$?

  # display table + time
  if [[ "$OUTPUT_FORMAT" != "csv" ]]; then
    grep -vE "Unexpected netCDF Vars" "$out" | sed -n '/=== Comparison/,/Total.*ms/p'
  fi

  # extract and save CSV
  if [[ "$OUTPUT_FORMAT" =~ ^(csv|both)$ ]]; then
    extract_table_to_csv "$out"
  fi

  if [[ $status -ne 0 ]]; then
    echo "  [FAILED] exit=$status  (see $out)"
    grep -vE "Unexpected netCDF Vars" "$out" | grep -iE "error" | tail -3
  else
    echo "  [OK]"
  fi
  echo "  (full log: $out)"
  echo
done

echo "=============================================================="
echo "All instances completed."
echo "Results in: $RESULTS_DIR"
echo "=============================================================="
