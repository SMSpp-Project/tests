#!/bin/bash
# Test script to verify all scenario reduction methods work correctly
# Usage: ./test_all_methods.sh [options]
#   Options:
#     -v, --verbose N    Set verbosity level (0=quiet, 1=normal, 2=detailed)
#     -f, --full N       Number of full scenarios (default: 20)
#     -r, --reduced N    Number of reduced scenarios (default: 4)
#     -h, --help         Show this help message
# Hurl your comments and insults at Benoît Tran in case of issues

# Default parameters
VERBOSE=1
FULL_SCENARIOS=20
REDUCED_SCENARIOS=4
EXECUTABLE="./ScenarioReduction_test"
CACHE_OPTS="--save-cache --load-cache"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--verbose)
            VERBOSE="$2"
            shift 2
            ;;
        -f|--full)
            FULL_SCENARIOS="$2"
            shift 2
            ;;
        -r|--reduced)
            REDUCED_SCENARIOS="$2"
            shift 2
            ;;
        -h|--help)
            echo "Test all scenario reduction methods"
            echo "Usage: $0 [options]"
            echo "  Options:"
            echo "    -v, --verbose N    Set verbosity level (0=quiet, 1=normal, 2=detailed)"
            echo "    -f, --full N       Number of full scenarios (default: 40)"
            echo "    -r, --reduced N    Number of reduced scenarios (default: 3)"
            echo "    -h, --help         Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

# Colors for output (if terminal supports it)
if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    BLUE='\033[0;34m'
    YELLOW='\033[1;33m'
    NC='\033[0m' # No Color
else
    RED=''
    GREEN=''
    BLUE=''
    YELLOW=''
    NC=''
fi

echo "=========================================="
echo "Scenario Reduction Methods Test"
echo "=========================================="
echo "Configuration:"
echo "  Full scenarios: $FULL_SCENARIOS"
echo "  Reduced scenarios: $REDUCED_SCENARIOS"
echo "  Verbosity level: $VERBOSE"
echo ""

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
    echo -e "${RED}Error: $EXECUTABLE not found${NC}"
    echo "Please build the test first with 'make'"
    exit 1
fi

# Array of methods to test
# Format: "method:label" (warmstart and shuffle are set via CLI now)
METHODS=(
    "baseline:baseline"
    "dupacova:dupacova"
    "bestfit:bestfit"
    "firstfit:firstfit"
    "milp:milp"
)

# Results storage
declare -A RESULTS
declare -A TIMES
declare -A STATUS
declare -A INDICES
declare -A WASSERSTEIN


# Function to extract objective value from output
extract_objective() {
    # First try to extract from cached output format (any number of scenarios)
    OBJ=$(echo "$1" | grep -E "Reduced problem \([0-9]+ scenarios\):" -A 2 | grep "Objective:" | sed 's/.*Objective: //' | awk '{print $1}')
    if [ -z "$OBJ" ]; then
        # Fall back to non-cached output format
        OBJ=$(echo "$1" | grep -E "Stochastic objective value \(expected\):" | tail -1 | sed 's/.*: //' | awk '{print $1}')
    fi
    echo "$OBJ"
}

# Function to extract solution time from output for full problem
extract_full_solution_time() {
    # Extract full problem solution time
    TIME=$(echo "$1" | grep -E "Full problem \([0-9]+ scenarios\):" -A 3 | grep "Solution time:" | sed 's/.*Solution time: //' | awk '{print $1}')
    echo "$TIME"
}


# Function to extract scenario reduction time from output (returns time in milliseconds)
extract_time() {
    # Extract the scenario reduction time (the important metric!)
    TIME=$(echo "$1" | grep -E "Scenario reduction time:" | tail -1 | sed 's/.*Scenario reduction time: //' | awk '{print $1}')
    echo "$TIME"
}

# Function to extract Wasserstein distance from output
extract_wasserstein() {
    # Extract the Wasserstein-ell distance value
    WASS=$(echo "$1" | grep -E "Wasserstein-[0-9.]+ distance:" | tail -1 | sed 's/.*distance: //' | awk '{print $1}')
    echo "$WASS"
}

# Function to extract selected scenario indices from output
extract_indices() {
    # Extract the selected scenarios line
    INDICES_LINE=$(echo "$1" | grep -E "Selected scenarios \(first 5, sorted\):" | tail -1 | sed 's/.*Selected scenarios (first 5, sorted): //')
    echo "$INDICES_LINE"
}

# Test each method
echo "Testing methods..."
echo "------------------------------------------"

FIRST_METHOD=true
for method_config in "${METHODS[@]}"; do
    # Parse the method configuration (method:label format)
    IFS=':' read -r method label <<< "$method_config"
    
    echo -e "${BLUE}Testing: $label${NC}"
    
    # Build command with caching options
    if [ "$FIRST_METHOD" = true ]; then
        # First method: use longer timeout (10 minutes for 40 scenarios)
        TIMEOUT_CMD="timeout 600"
        FIRST_METHOD=false
        echo "  (First run - saving cache, timeout: 10 minutes)"
    else
        # Subsequent methods: use shorter timeout (2 minutes)
        TIMEOUT_CMD="timeout 120"
        echo "  (Using cached data, timeout: 2 minutes)"
    fi
    
    # Run the test and capture output
    if [ $VERBOSE -ge 2 ]; then
        # Show full output for verbose mode 2
        OUTPUT=$($TIMEOUT_CMD $EXECUTABLE -m $method -n $FULL_SCENARIOS -r $REDUCED_SCENARIOS -w 0 -S 0 $CACHE_OPTS -v $VERBOSE 2>&1)
        echo "$OUTPUT"
    else
        # Capture output silently for processing
        OUTPUT=$($TIMEOUT_CMD $EXECUTABLE -m $method -n $FULL_SCENARIOS -r $REDUCED_SCENARIOS -w 0 -S 0 $CACHE_OPTS -v $VERBOSE 2>&1)
    fi
    
    # Check exit status
    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 0 ]; then
        STATUS[$label]="PASS"
        
        # Extract objective value, time, indices, and Wasserstein distance
        OBJ=$(extract_objective "$OUTPUT")
        TIME=$(extract_time "$OUTPUT")
        IND=$(extract_indices "$OUTPUT")
        WASS=$(extract_wasserstein "$OUTPUT")
        
        if [ -n "$OBJ" ]; then
            RESULTS[$label]=$OBJ
        else
            RESULTS[$label]="N/A"
        fi
        
        if [ -n "$TIME" ]; then
            # Keep time in milliseconds as-is
            TIMES[$label]="${TIME}ms"
        else
            TIMES[$label]="N/A"
        fi
        
        if [ -n "$IND" ]; then
            INDICES[$label]=$IND
        else
            INDICES[$label]="N/A"
        fi
        
        if [ -n "$WASS" ]; then
            WASSERSTEIN[$label]=$WASS
        else
            WASSERSTEIN[$label]="N/A"
        fi
        
        if [ $VERBOSE -ge 1 ] && [ $VERBOSE -lt 2 ]; then
            echo -e "  ${GREEN}✓${NC} Status: PASS"
            echo "    Objective: ${RESULTS[$label]}"
            echo "    Reduction Time: ${TIMES[$label]}"
            echo "    Wasserstein Distance: ${WASSERSTEIN[$label]}"
            echo "    Selected Indices: ${INDICES[$label]}"
        fi
    elif [ $EXIT_CODE -eq 124 ]; then
        # Timeout exit code
        STATUS[$label]="TIMEOUT"
        RESULTS[$label]="TIMEOUT"
        TIMES[$label]="TIMEOUT"
        INDICES[$label]="TIMEOUT"
        WASSERSTEIN[$label]="TIMEOUT"
        
        if [ $VERBOSE -ge 1 ]; then
            echo -e "  ${YELLOW}⚠${NC} Status: TIMEOUT"
        fi
    else
        STATUS[$label]="FAIL"
        RESULTS[$label]="ERROR"
        TIMES[$label]="N/A"
        INDICES[$label]="N/A"
        WASSERSTEIN[$label]="N/A"
        
        if [ $VERBOSE -ge 1 ]; then
            echo -e "  ${RED}✗${NC} Status: FAIL (exit code: $EXIT_CODE)"
            if [ $VERBOSE -ge 2 ]; then
                echo "Error output:"
                echo "$OUTPUT" | head -20
            fi
        fi
    fi
    
    echo ""
done

# Summary
echo "=========================================="
echo "Summary of Results"
echo "=========================================="

# First, run just to get the full problem objective and timing if we haven't already
if [ -z "$FULL_OBJECTIVE" ]; then
    echo "Getting full problem objective ($FULL_SCENARIOS scenarios)..."
    FULL_OUTPUT=$($EXECUTABLE -m baseline -n $FULL_SCENARIOS -r $REDUCED_SCENARIOS $CACHE_OPTS -v 0 2>&1)
    # Extract from cached output format first
    FULL_OBJECTIVE=$(echo "$FULL_OUTPUT" | grep -E "Full problem \($FULL_SCENARIOS scenarios\):" -A 2 | grep "Objective:" | sed 's/.*Objective: //' | awk '{print $1}')
    # Extract full problem solution time
    FULL_SOLUTION_TIME=$(extract_full_solution_time "$FULL_OUTPUT")
    if [ -n "$FULL_OBJECTIVE" ]; then
        echo "Full stochastic problem objective: $FULL_OBJECTIVE"
        if [ -n "$FULL_SOLUTION_TIME" ]; then
            echo "Full problem solution time: ${FULL_SOLUTION_TIME}ms"
        fi
    fi
fi

echo ""
echo "Results Table:"
printf "%-25s %-10s %15s %15s %15s\n" "Method" "Status" "Objective" "Wasserstein" "Time" 
printf "%-25s %-10s %15s %15s %15s\n" "------" "------" "---------" "-----------" "----" 

TOTAL_PASS=0
TOTAL_FAIL=0
TOTAL_TIMEOUT=0

for method_config in "${METHODS[@]}"; do
    IFS=':' read -r method label <<< "$method_config"
    
    if [ "${STATUS[$label]}" == "PASS" ]; then
        STATUS_COLOR="${GREEN}PASS${NC}"
        ((TOTAL_PASS++))
    elif [ "${STATUS[$label]}" == "TIMEOUT" ]; then
        STATUS_COLOR="${YELLOW}TIMEOUT${NC}"
        ((TOTAL_TIMEOUT++))
    else
        STATUS_COLOR="${RED}FAIL${NC}"
        ((TOTAL_FAIL++))
    fi
    
    printf "%-25s " "$label"
    printf "%-21b " "$STATUS_COLOR"
    printf "%15s %15s %15s\n" "${RESULTS[$label]}" "${WASSERSTEIN[$label]}" "${TIMES[$label]}" 
done

echo "------------------------------------------"
if [ $TOTAL_TIMEOUT -gt 0 ]; then
    echo -e "Total: ${GREEN}$TOTAL_PASS passed${NC}, ${RED}$TOTAL_FAIL failed${NC}, ${YELLOW}$TOTAL_TIMEOUT timeout${NC}"
else
    echo -e "Total: ${GREEN}$TOTAL_PASS passed${NC}, ${RED}$TOTAL_FAIL failed${NC}"
fi

echo ""
echo "Selected Scenario Indices (first 5, sorted):"
echo "------------------------------------------"
for method_config in "${METHODS[@]}"; do
    IFS=':' read -r method label <<< "$method_config"
    # Format indices with fixed width for proper alignment
    if [ "${INDICES[$label]}" != "N/A" ] && [ "${INDICES[$label]}" != "TIMEOUT" ] && [ "${INDICES[$label]}" != "ERROR" ]; then
        # Parse the comma-separated indices and format each with width 3
        IFS=',' read -ra idx_array <<< "${INDICES[$label]}"
        formatted_indices=""
        for idx in "${idx_array[@]}"; do
            # Trim whitespace and format with width 3
            idx_trimmed=$(echo "$idx" | xargs)
            if [ -n "$formatted_indices" ]; then
                formatted_indices=$(printf "%s, %3s" "$formatted_indices" "$idx_trimmed")
            else
                formatted_indices=$(printf "%3s" "$idx_trimmed")
            fi
        done
        printf "%-25s: %s\n" "$label" "$formatted_indices"
    else
        printf "%-25s: %s\n" "$label" "${INDICES[$label]}"
    fi
done


echo ""
echo "Legend:"
echo "  _warm    = using warmstart from Dupacova's solution"
echo "  _shuf    = using shuffling for randomized order"
echo "  _warm_shuf = both warmstart and shuffling enabled"

# Exit with appropriate status
if [ $TOTAL_FAIL -gt 0 ]; then
    exit 1
else
    exit 0
fi