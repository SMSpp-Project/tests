# UC Scenario Reduction

This folder handles the Unit Commitment (UC) problem. The uncertainty here
is a time series. It can be electricity demand over time, renewable power
available over time, or both. This is different from CFL, where the
uncertainty is a single static demand vector.

Work is split into two steps, the same as CFL. First generate scenarios and
a TSSB file. Then solve, using the shared generic program.

## Files in this folder

- `UCScenarioGenerator.cpp` : reads a UC instance, creates demand and or
  renewable scenarios, and writes a TSSB file. This is the only file here
  that knows anything about UC.
- `run_uc_test.sh` : a batch script that runs the generate and solve steps
  for many combinations of instance, seed, number of scenarios, number of
  representatives, and method. It writes a CSV of results.

The generic solve program itself is not in this folder: it is provided by the
`ScenarioReductionSolver` module (its `ScenarioReductionSolver_test` binary). It
also works on CFL files without any change.

## Types of uncertainty

A UC scenario is a vector. Its length depends on what varies. With `T` time
periods, `nd` demand nodes and `ni` intermittent (renewable) units.

- Demand only: length `nd * T`. Generated with `--no-maxpower`.
- Renewable only: length `ni * T`. Generated with `--no-demand`.
- Both: length `nd*T + ni*T`. This is the default, no flag needed.

## Step 1: build

```bash
cd <build-dir>
cmake --build . --target uc_scenario_generator -j1
cmake --build . --target ScenarioReductionSolver_test -j1
```

## Step 2: generate scenarios and a TSSB file

```bash
cd tests/ScenarioReduction/UCBlock
./UCScenarioGenerator -i <instance.nc4> -o <scenarios.nc4> \
    --tssb-output <tssb.nc4> -n 20 -v 0.3 -s 42 --no-maxpower --no-validate
```

Main options.

- `-i` : path to the base UC instance. Required.
- `-o` : where to save the plain scenario file.
- `--tssb-output` : where to save the TSSB file. This is the file the solve
  program reads. If you skip this flag, no TSSB file is written.
- `-n` : number of scenarios to generate.
- `-v` : how much demand or renewable power varies between scenarios.
- `-s` : random seed.
- `--no-demand` / `--no-maxpower` : disable one of the two uncertainty
  types, see the section above.
- `--no-validate` : always use this. The validation step is slow, can crash
  with demand uncertainty, and makes generation non repeatable.

Note that the number of scenarios `N` is fixed at generation time. If you
want to test several values of `N`, generate a separate TSSB file for each
one.

## Step 3: solve

```bash
./ScenarioReductionSolver_test -i <tssb.nc4> -m cssc -r 5 -c ../BSCfg.txt
```

Main options.

- `-i` : the TSSB file produced in step 2.
- `-m` : reduction method. One of `baseline`, `dupacova`, `bestfit`,
  `firstfit`, `cssc`.
- `-r` : how many representative scenarios to keep.
- `-c` : solver configuration file, in the parent folder, for example
  `BSCfg.txt`.

`cssc` needs an instance that has a ThermalUnitBlock (a `_TUB` instance).
Without it, only the heuristic methods work.

## Batch runs

`run_uc_test.sh` automates steps 2 and 3 over many combinations, for one
chosen uncertainty type at a time.

```bash
bash run_uc_test.sh --uncertainty demand
bash run_uc_test.sh --instances "EC_CO_Test_TUB EC_NC_Test_TUB" \
    --n "20 30" --k "5 10" --seeds "42 7" \
    --methods "dupacova cssc" --uncertainty renewable
```

Flags: `--instances --n --k --seeds --methods --solver --variation
--uncertainty --output`.

## Instances

UC instances with renewables live in `UCBlock/data/nc4/EC_Data`. Only the
`_TUB` instances have a ThermalUnitBlock, so only they support `cssc`.

- Support cssc (`_TUB`): `EC_CO_Test_TUB`, `EC_CO_Test_TUB_NB`,
  `EC_NC_Test_TUB`, `EC_NC_Test_TUB_NB`.
- Heuristics only (no `_TUB`): `EC_CO_Test`, `EC_CO_Test_NB`, `EC_NA_Test`,
  `EC_NA_Test_NB`, `EC_NC_Test`, `EC_NC_Test_NB`.

Name decoding: `EC` energy community, `CO`/`NA`/`NC` are network variants,
`_TUB` has a thermal unit block, `_NB` means no battery. The `NA` family has
no `_TUB` version, so `cssc` cannot run on it.

