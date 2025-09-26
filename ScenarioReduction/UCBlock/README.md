# UCBlock Scenario Generation

This directory contains tools for generating and managing scenarios for Unit Commitment (UC) problems.

## Files

- **UCBlockScenarioGenerator.cpp**: Main scenario generator for UC problems
- **makefile**: Build configuration for the UC generator

## Building

```bash
# Build the generator
make

# Or build in debug mode
make debug

# Clean build artifacts
make clean
```

## Usage

```bash
./UCBlockScenarioGenerator -i uc_instance.nc4 -t demand -n 50 -d 0.1
```

Options:
- `-i, --instance`: Path to base UC instance (required)
- `-t, --type`: Scenario type (demand/renewable/both)
- `-n, --scenarios`: Number of scenarios to generate
- `-d, --demand-var`: Demand variation factor
- `-r, --renewable-var`: Renewable variation factor
- `-o, --output`: Output file path
- `-s, --seed`: Random seed for reproducibility
- `--no-validate`: Skip feasibility validation
- `--verbose`: Set verbosity level (0-2)

## Scenario Types

### Demand Scenarios
- Peak hour variations (evening/morning peaks)
- Overall high/low demand patterns
- Random variations per time period
- Time-correlated patterns

### Renewable Scenarios
- Very low generation (calm/cloudy conditions)
- High generation (windy/sunny conditions)
- Intermittent patterns (passing clouds/gusty wind)
- Correlated variations with noise

### Combined Scenarios
- Simultaneous demand and renewable uncertainty
- Multi-dimensional scenario representation

## Output Format

Generated scenarios are saved as DiscreteScenarioSet in netCDF format:
- **Scenarios**: 2D array of demand/renewable values
- **Probabilities**: Uniform probability distribution
- **Metadata**: Generator info, scenario type, variation factors, timestamp

## Implementation Status

**Note**: This is currently a placeholder implementation that demonstrates the structure for UCBlock scenario generation. Full implementation requires:

1. UCBlock instance loading and data extraction
2. Access to UCBlock-specific methods for:
   - Extracting base demand profiles
   - Extracting renewable generation profiles
   - Applying scenario modifications
3. Integration with UCBlock solver configuration
4. Validation using UCBlock-specific feasibility checks

## Future Work

- Complete integration with UCBlock data structures
- Support for multi-area UC problems
- Reserve requirement scenarios
- Transmission capacity scenarios
- Generator outage scenarios
- Fuel price uncertainty