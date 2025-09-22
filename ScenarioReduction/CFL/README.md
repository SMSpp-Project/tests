# CFL Scenario Reduction Test

This directory contains the CFL-specific implementation of the scenario reduction test framework, along with tools for generating and managing scenarios for Capacitated Facility Location (CFL) problems.

## Components

### Test Framework
- **cfl_scenario_reduction_test**: Main test executable for CFL scenario reduction
- **CFLScenarioReductionTest.{h,cpp}**: CFL-specific test implementation
- **cfl_test_main.cpp**: Entry point for the test

### Scenario Generation Tools
- **CFLScenarioGenerator**: Standalone scenario generator for CFL problems (also validates instances)
- **batch_orlib.sh**: Batch script for running tests on ORLIB instances

### Configuration
- **makefile**: Build configuration for all components
- **BSPar_HiGHS.txt**: HiGHS solver parameters
- **BSConfig_SR.txt**: Scenario reduction solver configuration

## Important: Scenario Management

**The test framework requires pre-generated scenarios.** Scenarios must be generated using the CFLScenarioGenerator tool and stored in the `scenarios/CFL/` directory before running tests.

### Directory Structure
```
CFL/
├── scenarios/                  # Pre-generated scenario files
│   └── CFL/
│       ├── cap41_scenarios.nc4
│       ├── cap42_scenarios.nc4
│       └── ...
├── cfl_scenario_reduction_test # Main test executable
└── CFLScenarioGenerator        # Scenario generator and validator tool
```

## Building

```bash
# Build the test executable
make

# Build all components (test and generator)
make all

# Or build in debug mode
make debug

# Clean build artifacts
make clean
```

## Workflow

### Step 1: Generate Scenarios

Before running any tests, generate scenarios for your instances:

```bash
# Generate scenarios for a single instance
./CFLScenarioGenerator -i ../../../CapacitatedFacilityLocationBlock/data/nc4/ORLib/cap41.nc4

# The generator automatically saves to: scenarios/CFL/cap41_scenarios.nc4
```

### Step 2: Run Scenario Reduction Test

```bash
# Run test with default settings
./cfl_scenario_reduction_test -i ../../../CapacitatedFacilityLocationBlock/data/nc4/ORLib/cap41.nc4 -n 20 -r 5

# The test will automatically load scenarios from: scenarios/CFL/cap41_scenarios.nc4
```

## Usage Details

### Instance Validation

```bash
# Validate a single instance (using CFLScenarioGenerator in validate-only mode)
./CFLScenarioGenerator --instance instance.nc4 --validate-only

# Set time limit for validation
./CFLScenarioGenerator --instance instance.nc4 --validate-only --timeout 60 --verbose 2
```

The validator checks if CFL instances are feasible with single-sourcing constraints.

### Single Instance Generation

```bash
./CFLScenarioGenerator -i instance.nc4 -n 100 -v 0.2 -o output.nc4
```

Options:
- `-i, --instance`: Path to base CFL instance (required)
- `-n, --scenarios`: Number of scenarios to generate
- `-v, --variation`: Variation factor for demands
- `-o, --output`: Output file path
- `-s, --seed`: Random seed for reproducibility
- `--no-validate`: Skip feasibility validation
- `--validate-only`: Only validate instance, don't generate scenarios
- `--verbose`: Set verbosity level (0-2)
- `--timeout`: Timeout for validation (seconds)

### Batch Processing

```bash
# Process all ORLIB instances
./generate_orlib_scenarios.sh -n 50 -v 0.25

# Run in parallel
./generate_orlib_scenarios.sh -p -n 100
```

## Output Format

Generated scenarios are saved as DiscreteScenarioSet in netCDF format:
- **Scenarios**: 2D array of demand values
- **Probabilities**: Uniform probability distribution
- **Metadata**: Generator info, variation factor, timestamp

## Scenario Characteristics

The generator creates four types of demand clusters:
1. **Normal**: Small variations around original demands (±10%)
2. **High**: Increased demand scenarios (1.3x-1.5x)
3. **Low**: Decreased demand scenarios (0.5x-0.7x)
4. **Mixed**: Regional variations (half high, half low)

## Integration

Generated scenarios can be used with:
- ScenarioReductionTest for scenario reduction experiments
- TwoStageStochasticBlock for stochastic optimization
- Any tool that accepts DiscreteScenarioSet format