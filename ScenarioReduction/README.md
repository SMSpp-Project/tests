# Scenario Reduction Test Framework

This framework tests scenario reduction methods on stochastic optimization
problems. A scenario reduction method takes a large set of scenarios and
picks a small representative subset. The framework measures how close the
reduced problem is to the full problem.

It currently supports three problems. Capacitated Facility Location (CFL).
Unit Commitment with thermal units (UC-Thermal). A power system investment
problem (UC-Investment).

## The main idea: generate and solve are separate

The work is split into two steps.

The first step is called generate. It is specific to each problem. It reads
the original problem instance. It creates random scenarios. Then it writes
everything into one file. This file is called a TSSB file. It contains the
problem itself, the scenario data, and a description of where the first
stage decision variables are located inside the problem.

The second step is called solve. There is only one solve program for all
problems. It does not contain any code that knows about CFL or UC. It just
opens the TSSB file and reads what is inside. It relies on a mechanism
already built into SMS++. Every saved file stores the name of its own
problem type. The solve program uses this name to rebuild the right object
automatically, without needing to know in advance what kind of problem it
is looking at.

This means adding a new problem in the future only requires writing a new
generate program. The solve program never needs to change.

## Folder structure

- `CFL/` : the CFL generator, plus the batch script and configuration files
  for CFL experiments.
- `UCBlock/` : the UC-Thermal generator, plus its batch script.
- `UCBlock/UCInvestmentTest/` : a separate tool for the UC-Investment
  problem. See the note below about why this one is kept separate.
- `src/` : the shared code used by everything. This includes the CSSC
  algorithm, the heuristic reduction algorithms, and the one generic solve
  program.
- `include/` : headers for the shared code in `src/`.

Each of `CFL/` and `UCBlock/` has its own README with exact commands and
options.

## A note on the UC-Investment tool

The UC-Investment problem uses a different tool,
`UCInvestmentTest/UCInvestmentReductionTest.cpp`, instead of the generic
solve program. This tool computes an extra number called the
implementation error. It takes the decision chosen from the reduced
scenarios, and checks how well that same decision performs when applied to
the full scenario set. The generic solve program does not compute this
number. So this tool was kept as a separate, standalone program.

A few CFL variants are kept for the same reason. They measure things the
generic solve program was not built to measure, such as out of sample
performance.

## Solvers used

Scenario reduction can be done with a few different methods, selected by
the `-m` flag in the solve program.

- `baseline` : a simple default selection.
- `dupacova` : a greedy forward selection method.
- `bestfit` and `firstfit` : local search methods.
- `cssc` : Cost Space Scenario Clustering. This one is slower because
  it solves many optimization problems to build a cost aware clustering,
  but it can give a better reduction, especially for harder problems.

## Building

Everything is built with the normal SMS++ CMake system, from the main build
directory.

```bash
cd <build-dir>
cmake ..
cmake --build . --target CFLScenarioGenerator -j1
cmake --build . --target uc_scenario_generator -j1
cmake --build . --target scenario_reduction_solve -j1
```

Use `-j1` on machines with limited memory. Building with many parallel jobs
can run out of memory and corrupt the build.

See `CFL/README.md` and `UCBlock/README.md` for the exact generate and
solve commands for each problem.
