#!/bin/bash
# Test script to verify all scenario reduction methods work correctly
# Usage: ./test_all_methods.sh [options]
#   Options:
#     -v, --verbose N    Set verbosity level (0=quiet, 1=normal, 2=detailed)
#     -f, --full N       Number of full scenarios (default: 40)
#     -r, --reduced N    Number of reduced scenarios (default: 4)
#     -h, --help         Show this help message
# Hurl your comments and insults at Benoît Tran in case of issues

# Default parameters
VERBOSE=1
FULL_SCENARIOS=40
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
METHODS=("baseline" "dupacova" "bestfit" "firstfit" "milp")

# Results storage
declare -A RESULTS
declare -A TIMES
declare -A STATUS
declare -A INDICES

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
for method in "${METHODS[@]}"; do
    echo -e "${BLUE}Testing method: $method${NC}"
    
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
        OUTPUT=$($TIMEOUT_CMD $EXECUTABLE -method=$method -n_scen=$FULL_SCENARIOS -n_reduced=$REDUCED_SCENARIOS -test $CACHE_OPTS --verbose=$VERBOSE 2>&1)
        echo "$OUTPUT"
    else
        # Capture output silently for processing
        OUTPUT=$($TIMEOUT_CMD $EXECUTABLE -method=$method -n_scen=$FULL_SCENARIOS -n_reduced=$REDUCED_SCENARIOS -test $CACHE_OPTS --verbose=$VERBOSE 2>&1)
    fi
    
    # Check exit status
    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 0 ]; then
        STATUS[$method]="PASS"
        
        # Extract objective value, time, and indices
        OBJ=$(extract_objective "$OUTPUT")
        TIME=$(extract_time "$OUTPUT")
        IND=$(extract_indices "$OUTPUT")
        
        if [ -n "$OBJ" ]; then
            RESULTS[$method]=$OBJ
        else
            RESULTS[$method]="N/A"
        fi
        
        if [ -n "$TIME" ]; then
            # Keep time in milliseconds as-is
            TIMES[$method]="${TIME}ms"
        else
            TIMES[$method]="N/A"
        fi
        
        
        if [ -n "$IND" ]; then
            INDICES[$method]=$IND
        else
            INDICES[$method]="N/A"
        fi
        
        if [ $VERBOSE -ge 1 ] && [ $VERBOSE -lt 2 ]; then
            echo -e "  ${GREEN}✓${NC} Status: PASS"
            echo "    Objective: ${RESULTS[$method]}"
            echo "    Reduction Time: ${TIMES[$method]}"
            echo "    Selected Indices: ${INDICES[$method]}"
        fi
    elif [ $EXIT_CODE -eq 124 ]; then
        # Timeout exit code
        STATUS[$method]="TIMEOUT"
        RESULTS[$method]="TIMEOUT"
        TIMES[$method]="TIMEOUT"
        INDICES[$method]="TIMEOUT"
        
        if [ $VERBOSE -ge 1 ]; then
            echo -e "  ${YELLOW}⚠${NC} Status: TIMEOUT"
        fi
    else
        STATUS[$method]="FAIL"
        RESULTS[$method]="ERROR"
        TIMES[$method]="N/A"
        INDICES[$method]="N/A"
        
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
    FULL_OUTPUT=$($EXECUTABLE -method=baseline -n_scen=$FULL_SCENARIOS -n_reduced=$REDUCED_SCENARIOS -test $CACHE_OPTS --verbose=0 2>&1)
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
printf "%-12s %-10s %-15s %-15s\n" "Method" "Status" "Objective" "Reduction Time"
printf "%-12s %-10s %-15s %-15s\n" "------" "------" "---------" "--------------"

TOTAL_PASS=0
TOTAL_FAIL=0
TOTAL_TIMEOUT=0

for method in "${METHODS[@]}"; do
    if [ "${STATUS[$method]}" == "PASS" ]; then
        STATUS_COLOR="${GREEN}PASS${NC}"
        ((TOTAL_PASS++))
    elif [ "${STATUS[$method]}" == "TIMEOUT" ]; then
        STATUS_COLOR="${YELLOW}TIMEOUT${NC}"
        ((TOTAL_TIMEOUT++))
    else
        STATUS_COLOR="${RED}FAIL${NC}"
        ((TOTAL_FAIL++))
    fi
    
    printf "%-14s " "$method"
    printf "%-20b " "$STATUS_COLOR"
    printf "%-25s %-15s\n" "${RESULTS[$method]}" "${TIMES[$method]}"
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
for method in "${METHODS[@]}"; do
    printf "%-12s: %s\n" "$method" "${INDICES[$method]}"
done

# Exit with appropriate status
if [ $TOTAL_FAIL -gt 0 ]; then
    exit 1
else
    exit 0
fi