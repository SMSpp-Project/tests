# CFL Scenario Reduction

This folder handles the Capacitated Facility Location (CFL) problem. The
uncertainty here is customer demand. It can change from one scenario to
another.

Work is split into two steps, as explained in the main README. First
generate scenarios and a TSSB file. Then solve, using the shared generic
program.

## Files in this folder

- `CFLScenarioGenerator.cpp` : reads a CFL instance, creates demand
  scenarios, and writes a TSSB file. This is the only file here that knows
  anything about CFL.
- `run_cfl_tests` : a batch script that runs the generate and solve
  steps for many combinations of instance, number of scenarios, number of
  representatives, and method. It writes a CSV of results.

The generic solve program itself is not in this folder: it is provided by the
`ScenarioReductionSolver` module (its `ScenarioReductionSolver_test` binary). It is
shared between CFL and UC and works on CFL files exactly the same way.

## Step 1: build

```bash
cd <build-dir>
cmake --build . --target CFLScenarioGenerator -j1
cmake --build . --target ScenarioReductionSolver_test -j1
```

## Step 2: generate scenarios and a TSSB file

```bash
cd tests/ScenarioReduction/CFLBlock
./CFLScenarioGenerator -i <instance.nc4> -o <scenarios.nc4> \
    --tssb-output <tssb.nc4> -n 50 -v 0.5 -s 1 --no-validate
```

Main options.

- `-i` : path to the base CFL instance. Required.
- `-o` : where to save the plain scenario file.
- `--tssb-output` : where to save the TSSB file. This is the file the solve
  program reads. If you skip this flag, no TSSB file is written.
- `-n` : number of scenarios to generate.
- `-v` : how much demand varies between scenarios.
- `-s` : random seed. Same seed gives the same scenarios.
- `--no-validate` : skip a slow feasibility check. Recommended, it also
  makes generation fully repeatable for a given seed.

Note that the number of scenarios `N` is fixed at generation time. If you
want to test several values of `N`, you need to generate a separate TSSB
file for each one.

## Step 3: solve

```bash
./ScenarioReductionSolver_test -i <tssb.nc4> -m cssc -r 5 -c ../BSCfg.txt
```

Main options.

- `-i` : the TSSB file produced in step 2.
- `-m` : reduction method. One of `baseline`, `dupacova`, `bestfit`,
  `firstfit`, `cssc`.
- `-r` : how many representative scenarios to keep.
- `-c` : solver configuration file. The configs live one folder up, for
  example `BSCfg.txt` or `BSCfg.txt`.

Example output.

```
Selected scenarios: 7 22 23 27 28
Full  (N=50): 7.886e+05
Reduced (K=5): 7.891e+05  (28150.2 ms)
Gap: 0.0653%
```

The gap is the relative difference between the reduced objective and the
full objective. A smaller gap means the reduced set represents the full
problem better.

## Batch runs

`run_cfl_tests` automates steps 2 and 3 over many combinations. It
generates one TSSB file per value of `N`, since the solve program cannot
subselect a smaller N from a bigger scenario pool.

The two executables are built in the build tree, so the batch takes them
from the outside and defaults to the current directory, as every other SMS++
batch: either run it from the directory holding them, or say where they are.

```bash
bash <src-dir>/run_cfl_tests
bash <src-dir>/run_cfl_tests --generator <build-dir>/CFLScenarioGenerator \
    --solve <build-dir>/ScenarioReductionSolver_test \
    --instances "cap102 cap121" --n "20 50" --k "5 10" \
    --methods "dupacova cssc" --solver BSCfg.txt --seed 1
```

Flags: `--generator --solve --instances --n --k --methods --solver --seed
--variation --instance-dir --scenario-dir --config-dir --output`; `--help`
prints them all. Every directory defaults to a path relative to the script,
so the batch runs from any working directory, and each of them can also be
set through the environment (`GEN`, `SOLVE`, `IDIR`, `SDIR`, `CFGDIR`).
