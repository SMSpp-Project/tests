# Scenario Reduction Test Framework

Modular test framework for scenario reduction algorithms on various stochastic optimization problems. Currently supports Capacitated Facility Location (CFL) problems with extensible architecture for adding other problem types.

## Architecture

### Core Components (Problem-Independent)

- **`include/`** - Common headers
  - `AbstractScenarioReductionTest.h` - Abstract base class defining the test interface
  - `ScenarioReductionCommon.h` - Shared data structures and utilities

- **`src/`** - Base implementation
  - `AbstractScenarioReductionTest.cpp` - Problem-independent workflow implementation

### Problem-Specific Implementations

- **`CFL/`** - Capacitated Facility Location
  - `CFLScenarioReductionTest.{h,cpp}` - CFL-specific test implementation
  - `cfl_test_main.cpp` - Entry point
  - `CFLScenarioGenerator.cpp` - Standalone scenario generator
  - `batch_orlib.sh` - Batch script for ORLIB instances
  - `makefile` - Build configuration

- **`UCBlock`** - WIP

### Scenario Storage

- **`scenarios/`** - Centralized scenario storage
  - `CFL/` - CFL-specific scenarios (naming: `<instance>_scenarios.nc4`)
  - Future problem types will have their own subdirectories
### Supporting Files

- `BSPar_HiGHS.txt` - HiGHS solver configuration
- `BSConfig_SR.txt` - Scenario reduction solver configuration


## Building

Each problem type is built independently in its own directory:

```bash
# Build CFL test
cd CFL
make release

# Build with debug symbols
make debug
```

## Important: Pre-Generated Scenarios Required

**The test framework does NOT generate scenarios.** Scenarios must be pre-generated using problem-specific generators before running any tests.

### Generating Scenarios

Each problem type has its own scenario generator:

**For CFL problems:**
```bash
cd CFL
./CFLScenarioGenerator -i <instance_path> -n <num_scenarios> [-v <verbosity>]
# Scenarios saved to: ../scenarios/CFL/<instance_name>_scenarios.nc4
```

The test will automatically look for scenarios in the expected location based on the instance name.

## Running Tests

### CFL Test
```bash
cd CFL
./cfl_scenario_reduction_test [options]
```

### Command-Line Options

**Instance Selection:**
- `-i, --instance <path>` - Path to problem instance file (required)
- `-f, --scenario-file <path>` - Load pre-generated scenarios from file (optional)

**Scenario Configuration:**
- `-r, --reduced <N>` - Number of reduced scenarios (default: 3)
- `-m, --method <name>` - Reduction method: baseline, dupacova, bestfit, firstfit, milp
- **Note:** Total number of scenarios is determined from the loaded scenario file

**Algorithm Options:**
- `-w, --warmstart <0|1>` - Use warm start for local search methods
- `-S, --shuffle <0|1>` - Enable shuffling for FirstFit method

**Solver Settings:**
- `-t, --time <seconds>` - Solver time limit in seconds
- `-v, --verbose <level>` - Verbosity level (0=silent, 1=normal, 2=detailed)

**Results Caching:**
- `-s, --save-results` - Save solution results to cache
- `-L, --load-results` - Load pre-computed results from cache
- `-d, --cache-dir <path>` - Specify cache directory (default: ./cache/)

**Analysis:**
- `-c, --compute-vpi` - Compute Value of Perfect Information (VPI)
- `-h, --help` - Show help message

### Examples

**Basic CFL test:**
```bash
# First, ensure scenarios are generated:
./CFLScenarioGenerator -i ../../../CapacitatedFacilityLocationBlock/data/nc4/ORLib/cap41.nc4 -n 10

# Then run the test:
./cfl_scenario_reduction_test -i ../../../CapacitatedFacilityLocationBlock/data/nc4/ORLib/cap41.nc4 -r 5 -v 1
```

**Test with custom scenario file:**
```bash
./cfl_scenario_reduction_test -i ../../../CapacitatedFacilityLocationBlock/data/nc4/ORLib/cap71.nc4 -f /path/to/custom_scenarios.nc4 -r 5
```

**Full analysis with VPI:**
```bash
# Ensure scenarios exist, then run:
./cfl_scenario_reduction_test -i ../../../CapacitatedFacilityLocationBlock/data/nc4/ORLib/cap41.nc4 -r 10 -p -v 2 -t 60
```

**Generate scenarios for an instance:**
```bash
./CFLScenarioGenerator -i ../../../CapacitatedFacilityLocationBlock/data/nc4/ORLib/cap101.nc4 -n 100 -v 2
# Scenarios will be saved to ../scenarios/CFL/cap101_scenarios.nc4
```

### Batch Testing

**Test all ORLib instances:**
```bash
./batch_orlib.sh [--small]  # --small tests only cap4* instances
```

The batch script:
1. Tests multiple CFL instances from the ORLib dataset
2. Handles infeasible scenarios with automatic regeneration
3. Outputs CSV results with status tracking (SUCCESS/PARTIAL/FAILED)
4. Supports timeout handling and partial result detection

## How It Works

### Test Workflow

1. **Load Base Instance**: Reads a deterministic problem instance from netCDF format
2. **Load Scenarios** (must be pre-generated):
   - If scenario file specified with `-f`: Load from specified file
   - Otherwise: Look for scenarios in `../scenarios/<problem_type>/<instance>_scenarios.nc4`
   - **If not found: Test will fail with instructions on how to generate scenarios**
3. **Perform Scenario Reduction**: Uses DiscreteScenarioSet with selected algorithm
4. **Solve Stochastic Problems**: Creates TwoStageStochasticBlock and solves:
   - Full problem with all scenarios
   - Reduced problem with representative scenarios
5. **Compute VPI** (optional): Calculates Value of Perfect Information by solving anticipative problems
6. **Report Results**: Shows objectives, solution times, speedup, and approximation error

### Scenario Generation (CFL)

For CFL problems, scenarios represent demand variations organized in 4 clusters:
- **Cluster 0**: Normal demand (±10% variation)
- **Cluster 1**: High demand (1.3x-1.5x)
- **Cluster 2**: Low demand (0.5x-0.7x)
- **Cluster 3**: Mixed regional variation

Use `CFLScenarioGenerator` to pre-generate scenarios for any CFL instance.

### Key Features

- **Modular Architecture**: Abstract base class with problem-specific implementations
- **Centralized Scenario Storage**: Pre-generated scenarios stored in `scenarios/` directory
- **Robust Error Handling**: Graceful handling of solver failures and infeasible scenarios
- **Performance Metrics**: Wasserstein distance, speedup, approximation error
- **Results Caching**: Save/load solution results to avoid recomputation
- **Comprehensive Output**: Detailed reporting with multiple verbosity levels

## Scenario Reduction Methods

### Available Algorithms

- **baseline** - Default DiscreteScenarioSet selection
- **dupacova** - Dupačová et al. (2003) greedy algorithm (default)
- **bestfit** - Best-fit local search heuristic
- **firstfit** - First-fit constructive heuristic
- **milp/optimal** - MILP-based optimal reduction using HiGHS

### Method Selection

The test automatically configures the appropriate solver:
- ScenarioReductionSolver for heuristic methods (dupacova, bestfit, firstfit)
- HiGHSMILPSolver for optimal reduction (milp)

## Extending the Framework

### Adding a New Problem Type

To add support for a new problem type:

1. **Create a new directory** (e.g., `UCBlock/`, `MCFBlock/`)

2. **Implement the abstract interface**:
   ```cpp
   class MyProblemScenarioReductionTest : public AbstractScenarioReductionTest {
   protected:
       // Required pure virtual methods
       void load_problem_instance(const std::string& path) override;
       void create_twostage_netcdf(const std::string& filename,
                                  const std::vector<std::vector<double>>& scenarios,
                                  const std::vector<double>& probabilities) override;
       std::string get_problem_type() const override;
       size_t get_first_stage_dimension() const override;
       std::string get_scenarios_directory() const override;

       // Optional
       void print_additional_help(const char* program_name) override;
   };
   ```

3. **Create the main entry point** (`main.cpp`):
   ```cpp
   int main(int argc, char** argv) {
       MyProblemScenarioReductionTest test;
       return test.run(argc, argv);
   }
   ```

4. **Set up the build** (create `makefile` based on CFL example)

5. **Create scenario storage**: `scenarios/MyProblem/`

6. **Optional: Create a scenario generator** for pre-generating scenarios

### Key Implementation Points

- **load_problem_instance**: Load your Block and set `base_block`, `stochastic_block`
- **create_twostage_netcdf**: Create TwoStageStochasticBlock with your problem structure
- **get_scenarios_directory**: Return `"../scenarios/MyProblem/"`
- **get_first_stage_dimension**: Return size of first-stage decision variables

The base class handles all the common workflow - you just provide the problem-specific parts!

## Output Interpretation

### Solution Quality Metrics

- **Objective Value**: Expected cost across all scenarios
- **Approximation Error**: Difference between full and reduced objectives
- **Relative Error**: Approximation error as percentage
- **Speedup**: Ratio of full to reduced solution times

### Value of Perfect Information (VPI)

- **Stochastic Solution**: Here-and-now decision with uncertainty
- **Anticipative Solution**: Perfect information (wait-and-see) solution
- **VPI**: Stochastic - Anticipative (cost of uncertainty)

A positive VPI indicates the value of having perfect information about future scenarios.

## Troubleshooting

### Common Issues

1. **"ScenarioReductionSolver not present"**: Normal warning - the config file is auto-generated
2. **"Failed to load scenarios"**:
   - Scenarios must be pre-generated before running tests
   - Use the problem-specific generator (e.g., `CFLScenarioGenerator` for CFL)
   - Place generated files in `scenarios/<problem_type>/` directory
   - Follow naming convention: `<instance_name>_scenarios.nc4`
   - Or specify custom scenario file with `-f` option
3. **Timeout issues**: Increase time limit with `-t` option
4. **Memory issues**: Reduce number of scenarios or use pre-generated scenarios