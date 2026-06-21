# UC Scenario Reduction Test

This directory contains the Unit Commitment (UC) test for scenario reduction.
A single self contained executable solves the full two stage stochastic
problem, runs a chosen scenario reduction method, solves the reduced problem,
and reports the in sample gap between them. A scenario generator and a batch
sweep script complete the workflow.

## Components

### Executables

- `uc_scenario_reduction_test`: the main test. In ONE run it loads a UC
  instance, loads a scenario set, solves the full two stage problem (N
  scenarios), applies a reduction method to pick K representatives, solves the
  reduced problem, and prints the objectives plus the in sample gap and the
  timings. All methods (including cssc) go through this one binary.
- `uc_scenario_generator`: standalone tool that generates a
  `DiscreteScenarioSet` of demand and/or renewable scenarios for a UC instance.

Both are produced under `<build-dir>/tests/ScenarioReduction/UC`.

### Source files

- `UCScenarioReductionTest.cpp`, `uc_test_main.cpp`: implementation and entry
  point of `uc_scenario_reduction_test`.
- `UCScenarioGenerator.cpp`: implementation of `uc_scenario_generator`.

Note: the generic, problem agnostic solvers live one level up in
`tests/ScenarioReduction/src` (`ScenarioReductionSolver` for the heuristics and
`CSSCScenarioReductionSolver` for CSSC). They are compiled into this test and
shared with the CFL test.

### Configuration

Solver configuration files live in the parent directory
`tests/ScenarioReduction` (`BSPar_CPLEX.txt`, `BSPar_HiGHS.txt`,
`BSPar_GRB.txt`, `BSPar_SCIP.txt`).

## Uncertainty types

A UC scenario is a flat vector whose layout depends on what varies. With T time
periods, nd demand nodes and ni intermittent units:

- demand only: length `nd * T`, generated with `--no-maxpower`
- renewable only: length `ni * T`, generated with `--no-demand`
- both: length `nd*T + ni*T`, generated with neither flag (the default)

The test infers the type automatically from the scenario vector length.

## What the test measures

The extensive form is a genuine two stage stochastic program: the unit
commitment (on and off schedule of the generating units) is the first stage
(here and now) decision shared by all scenarios through non anticipativity
constraints, while the dispatch is the second stage (recourse) that adapts to
each scenario demand and renewable availability.

The test reports:

- `Full_Obj`: optimum of the full problem over all N scenarios.
- `Reduced_Obj`: optimum of the reduced problem over the K selected scenarios.
- `Gap`: the in sample gap, `|Reduced_Obj - Full_Obj| / |Full_Obj|`.

## Reduction methods

Selected with `-m`: `baseline`, `dupacova`, `bestfit`, `firstfit`, `cssc`.


## Building

```bash
cd <build-dir>
cmake ..
cmake --build . --target uc_scenario_reduction_test --target uc_scenario_generator -j$(nproc)
```

## Workflow

### Step 1: generate scenarios

```bash
# demand only
./uc_scenario_generator -i <instance.nc4> -o <scen.nc4> -n 20 -v 0.3 -s 42 --no-maxpower --no-validate

# renewable only
./uc_scenario_generator -i <instance.nc4> -o <scen.nc4> -n 20 -v 0.3 -s 42 --no-demand --no-validate

# both (default)
./uc_scenario_generator -i <instance.nc4> -o <scen.nc4> -n 20 -v 0.3 -s 42 --no-validate
```

Always pass `--no-validate`: the validation step solves each scenario and is
known to crash on demand uncertainty, and it also makes generation non
deterministic.

### Step 2: run one test

```bash
./uc_scenario_reduction_test \
    -i <instance.nc4> -f <scen.nc4> \
    -n 20 -r 5 -m cssc -c ../../BSPar_CPLEX.txt --verbose 1
```

Options:

- `-i`: UC instance netCDF file (required)
- `-f`: scenario netCDF file
- `-n`: number of full scenarios to use (default: all in the file)
- `-r`: number of reduced scenarios K (default 3)
- `-m`: reduction method (default `dupacova`)
- `-c`: solver config file (default `BSPar_HiGHS.txt`)
- `--verbose`: verbosity 0 to 2
- `--skip-full`: skip solving the full problem (no gap reported)

## Batch experiments

`run_uc_test.sh` sweeps over instances, seeds, N, K and methods for one chosen
uncertainty type, then prints an aligned table and writes a CSV. It generates
the scenario file once per `(instance, uncertainty, N, seed)` and parses the gap
and timings straight from the test output.

```bash
bash run_uc_test.sh --uncertainty demand
bash run_uc_test.sh --instances "EC_CO_Test_TUB EC_NC_Test_TUB" \
     --n "20 30" --k "5 10" --seeds "42 7" \
     --methods "dupacova cssc" --uncertainty renewable
```

Flags: `--instances --n --k --seeds --methods --solver --variation
--uncertainty --output`.

## Output format

The table and the CSV have one row per `(instance, uncertainty, seed, N, K,
method)` with columns:

- `Full_Obj`: full problem objective (N scenarios)
- `Reduced_Obj`: reduced problem objective (K scenarios)
- `Gap_Pct`: in sample gap percentage
- `RedTime_s`: time to solve the reduced problem (seconds)
- `AlgoTime_s`: time of the reduction algorithm itself (seconds)

The raw test prints the timings in microseconds; the script converts them to
seconds. `AlgoTime_s` is the genuine computational cost of the reduction
method, tiny for the heuristics and large for cssc.

## Instances

UC instances with renewables live in `UCBlock/data/nc4/EC_Data`. Only the
`_TUB` instances (with a ThermalUnitBlock, hence commitment variables) support
cssc:

- cssc capable (`_TUB`): `EC_CO_Test_TUB`, `EC_CO_Test_TUB_NB`,
  `EC_NC_Test_TUB`, `EC_NC_Test_TUB_NB`
- heuristics only (no `_TUB`): `EC_CO_Test`, `EC_CO_Test_NB`, `EC_NA_Test`,
  `EC_NA_Test_NB`, `EC_NC_Test`, `EC_NC_Test_NB`

Name decoding: `EC` energy community, `CO`/`NA`/`NC` network variants, `_TUB`
has a thermal unit block, `_NB` no battery. The `NA` family has no `_TUB`
version, so cssc cannot run on it.

The `UCBlock/data/nc4/1UC_Data` directory holds the standard UC benchmark
library (no renewables); it is not used for the demand versus renewable
comparison here.

## Notes

- cssc is slow on UC: roughly a few minutes at N=20, around ten minutes at
  N=50, much longer at N=100. Use a modest N when including it, or drop it from
  `--methods` for a quick heuristic sweep.
- The in sample gap is not monotone in K and does not by itself rank methods by
  decision quality; it only measures the objective value approximation.
