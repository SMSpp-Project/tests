#!/bin/bash
# Test script to verify all scenario reduction methods work correctly
# Usage: ./test_all_methods.sh [options]
#   Options:
#     -v, --verbose N    Set verbosity level (0=quiet, 1=normal, 2=detailed)
#     -f, --full N       Number of full scenarios (default: 20)
#     -r, --reduced N    Number of reduced scenarios (default: 5)
#     -h, --help         Show this help message
# Hurl your comments and insults at Benoît Tran in case of issues

# Default parameters
VERBOSE=1
FULL_SCENARIOS=20
REDUCED_SCENARIOS=5
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
            echo "    -f, --full N       Number of full scenarios (default: 10)"
            echo "    -r, --reduced N    Number of reduced scenarios (default: 5)"
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
METHODS=("baseline" "dupacova" "bestfit" "firstfit")

# Results storage
declare -A RESULTS
declare -A TIMES
declare -A STATUS

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

# Function to extract time from output (returns time in milliseconds)
extract_time() {
    # First try to extract from cached output format (after "Reduced problem", any number of scenarios)
    TIME=$(echo "$1" | grep -E "Reduced problem \([0-9]+ scenarios\):" -A 3 | grep "Solution time:" | sed 's/.*Solution time: //' | awk '{print $1}')
    if [ -z "$TIME" ]; then
        # Fall back to non-cached output format
        TIME=$(echo "$1" | grep -E "Solution time:|Total time:|Computation time:" | tail -1 | sed 's/.*time[: ]*//' | awk '{print $1}')
    fi
    echo "$TIME"
}

# Test each method
echo "Testing methods..."
echo "------------------------------------------"

FIRST_METHOD=true
for method in "${METHODS[@]}"; do
    echo -e "${BLUE}Testing method: $method${NC}"
    
    # Build command with caching options
    if [ "$FIRST_METHOD" = true ]; then
        # First method: use longer timeout (5 minutes)
        TIMEOUT_CMD="timeout 300"
        FIRST_METHOD=false
        echo "  (First run - saving cache, timeout: 5 minutes)"
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
        
        # Extract objective value and time
        OBJ=$(extract_objective "$OUTPUT")
        TIME=$(extract_time "$OUTPUT")
        
        if [ -n "$OBJ" ]; then
            RESULTS[$method]=$OBJ
        else
            RESULTS[$method]="N/A"
        fi
        
        if [ -n "$TIME" ]; then
            # Convert milliseconds to seconds with 2 decimal places
            if [[ "$TIME" =~ ^[0-9]+$ ]]; then
                TIME_SEC=$(echo "scale=2; $TIME / 1000" | bc)
                TIMES[$method]="${TIME_SEC}s"
            else
                TIMES[$method]=$TIME
            fi
        else
            TIMES[$method]="N/A"
        fi
        
        if [ $VERBOSE -ge 1 ] && [ $VERBOSE -lt 2 ]; then
            echo -e "  ${GREEN}✓${NC} Status: PASS"
            echo "    Objective: ${RESULTS[$method]}"
            echo "    Time: ${TIMES[$method]}"
        fi
    elif [ $EXIT_CODE -eq 124 ]; then
        # Timeout exit code
        STATUS[$method]="TIMEOUT"
        RESULTS[$method]="TIMEOUT"
        TIMES[$method]="TIMEOUT"
        
        if [ $VERBOSE -ge 1 ]; then
            echo -e "  ${YELLOW}⚠${NC} Status: TIMEOUT"
        fi
    else
        STATUS[$method]="FAIL"
        RESULTS[$method]="ERROR"
        TIMES[$method]="N/A"
        
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

# First, run just to get the full problem objective if we haven't already
if [ -z "$FULL_OBJECTIVE" ]; then
    echo "Getting full problem objective ($FULL_SCENARIOS scenarios)..."
    FULL_OUTPUT=$($EXECUTABLE -method=baseline -n_scen=$FULL_SCENARIOS -n_reduced=$FULL_SCENARIOS -test $CACHE_OPTS --verbose=0 2>&1)
    # Try to extract from cached output format first
    FULL_OBJECTIVE=$(echo "$FULL_OUTPUT" | grep -E "Full problem \($FULL_SCENARIOS scenarios\):" -A 2 | grep "Objective:" | sed 's/.*Objective: //' | awk '{print $1}')
    if [ -z "$FULL_OBJECTIVE" ]; then
        # Fall back to non-cached output format
        FULL_OBJECTIVE=$(echo "$FULL_OUTPUT" | grep -E "Stochastic objective value \(expected\):" | tail -1 | sed 's/.*: //' | awk '{print $1}')
    fi
    if [ -n "$FULL_OBJECTIVE" ]; then
        echo "Full stochastic problem objective: $FULL_OBJECTIVE"
    fi
fi

echo ""
printf "%-12s %-10s %-15s %-10s\n" "Method" "Status" "Objective" "Time"
printf "%-12s %-10s %-15s %-10s\n" "------" "------" "---------" "----"

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
    printf "%-12s %-10s\n" "${RESULTS[$method]}" "${TIMES[$method]}"
done

echo "------------------------------------------"
if [ $TOTAL_TIMEOUT -gt 0 ]; then
    echo -e "Total: ${GREEN}$TOTAL_PASS passed${NC}, ${RED}$TOTAL_FAIL failed${NC}, ${YELLOW}$TOTAL_TIMEOUT timeout${NC}"
else
    echo -e "Total: ${GREEN}$TOTAL_PASS passed${NC}, ${RED}$TOTAL_FAIL failed${NC}"
fi

# Exit with appropriate status
if [ $TOTAL_FAIL -gt 0 ]; then
    exit 1
else
    exit 0
fi