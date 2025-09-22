#!/bin/bash

# Batch script to run scenario reduction tests on ORLIB CFL instances
# Processes all instances and generates a summary report

# Configuration
CFL_DATA_DIR="../../../CapacitatedFacilityLocationBlock/data/nc4/ORLib"
OUTPUT_DIR="results_orlib"
LOG_FILE="$OUTPUT_DIR/batch_results.log"
SUMMARY_FILE="$OUTPUT_DIR/summary.csv"

# Test parameters
NUM_SCENARIOS=20
REDUCED_SCENARIOS=5
TIME_LIMIT=30
VARIATION_FACTOR=0.2

# Reduction methods to test
METHODS=("baseline" "dupacova" "bestfit" "firstfit")

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Initialize log and summary files
echo "ORLIB CFL Scenario Reduction Batch Test" > "$LOG_FILE"
echo "Started at: $(date)" >> "$LOG_FILE"
echo "" >> "$LOG_FILE"

# CSV header
echo "Instance,Method,Facilities,Customers,Full_Scenarios,Reduced_Scenarios,Full_Obj,Reduced_Obj,Gap_%,Reduction_Time_ms,Full_Time_ms,Reduced_Time_ms" > "$SUMMARY_FILE"

# Function to extract results from output
extract_results() {
    local output="$1"
    local instance="$2"
    local method="$3"

    # Extract values using grep and awk
    local facilities=$(echo "$output" | grep "First-stage dimension:" | awk '{print $3}')
    local customers=$(echo "$output" | grep "Scenario dimension:" | awk '{print $3}')
    local full_obj=$(echo "$output" | grep "Full:" | awk '{print $2}')
    local reduced_obj=$(echo "$output" | grep "Reduced:" | grep -v "scenarios" | awk '{print $2}')
    local gap=$(echo "$output" | grep "Difference:" | awk -F'[(%]' '{print $(NF-1)}')
    local red_time=$(echo "$output" | grep "Reduction time:" | awk '{print $3}')
    local full_time=$(echo "$output" | grep "Full solve time:" | awk '{print $4}')
    local reduced_time=$(echo "$output" | grep "Reduced solve time:" | awk '{print $4}')

    # Write to CSV
    echo "$instance,$method,$facilities,$customers,$NUM_SCENARIOS,$REDUCED_SCENARIOS,$full_obj,$reduced_obj,$gap,$red_time,$full_time,$reduced_time" >> "$SUMMARY_FILE"
}

# Count total instances
TOTAL_INSTANCES=$(ls "$CFL_DATA_DIR"/*.nc4 2>/dev/null | wc -l)
if [ "$TOTAL_INSTANCES" -eq 0 ]; then
    echo "Error: No .nc4 files found in $CFL_DATA_DIR"
    exit 1
fi

echo "Found $TOTAL_INSTANCES ORLIB instances to process"
echo ""

# Process each instance
INSTANCE_COUNT=0
for INSTANCE_FILE in "$CFL_DATA_DIR"/*.nc4; do
    INSTANCE_NAME=$(basename "$INSTANCE_FILE" .nc4)
    INSTANCE_COUNT=$((INSTANCE_COUNT + 1))

    echo "[$INSTANCE_COUNT/$TOTAL_INSTANCES] Processing $INSTANCE_NAME..."
    echo "========================================" >> "$LOG_FILE"
    echo "Instance: $INSTANCE_NAME" >> "$LOG_FILE"
    echo "========================================" >> "$LOG_FILE"

    # Test each reduction method
    for METHOD in "${METHODS[@]}"; do
        echo "  Testing $METHOD method..."

        # Run the test
        OUTPUT=$(./cfl_scenario_reduction_test \
            -i "$INSTANCE_FILE" \
            -n "$NUM_SCENARIOS" \
            -r "$REDUCED_SCENARIOS" \
            -m "$METHOD" \
            -t "$TIME_LIMIT" \
            -v 0 2>&1)

        # Check if successful
        if echo "$OUTPUT" | grep -q "Test completed successfully"; then
            echo "    ✓ Success"
            extract_results "$OUTPUT" "$INSTANCE_NAME" "$METHOD"
        else
            echo "    ✗ Failed"
            echo "$INSTANCE_NAME,$METHOD,ERROR,ERROR,ERROR,ERROR,ERROR,ERROR,ERROR,ERROR,ERROR,ERROR" >> "$SUMMARY_FILE"
        fi

        # Log detailed output
        echo "" >> "$LOG_FILE"
        echo "Method: $METHOD" >> "$LOG_FILE"
        echo "----------------------------------------" >> "$LOG_FILE"
        echo "$OUTPUT" >> "$LOG_FILE"
    done

    echo ""
done

# Generate summary statistics
echo ""
echo "Generating summary statistics..."

# Create summary report
cat > "$OUTPUT_DIR/report.txt" <<EOF
================================================================================
ORLIB CFL SCENARIO REDUCTION TEST SUMMARY
================================================================================

Test Configuration:
- Full scenarios: $NUM_SCENARIOS
- Reduced scenarios: $REDUCED_SCENARIOS
- Variation factor: $VARIATION_FACTOR
- Time limit: $TIME_LIMIT seconds
- Methods tested: ${METHODS[*]}

Instances Processed: $TOTAL_INSTANCES

Results Summary:
EOF

# Calculate average gap for each method
for METHOD in "${METHODS[@]}"; do
    AVG_GAP=$(awk -F',' -v method="$METHOD" '
        $2 == method && $8 != "ERROR" {sum += $8; count++}
        END {if(count>0) printf "%.2f", sum/count; else print "N/A"}
    ' "$SUMMARY_FILE")

    AVG_TIME=$(awk -F',' -v method="$METHOD" '
        $2 == method && $10 != "ERROR" {sum += $10; count++}
        END {if(count>0) printf "%.0f", sum/count; else print "N/A"}
    ' "$SUMMARY_FILE")

    SUCCESS_COUNT=$(awk -F',' -v method="$METHOD" '
        $2 == method && $8 != "ERROR" {count++}
        END {print count}
    ' "$SUMMARY_FILE")

    echo "- $METHOD: Avg Gap = $AVG_GAP%, Avg Reduction Time = ${AVG_TIME}ms, Success = $SUCCESS_COUNT/$TOTAL_INSTANCES" >> "$OUTPUT_DIR/report.txt"
done

echo "" >> "$OUTPUT_DIR/report.txt"
echo "Detailed results saved in: $SUMMARY_FILE" >> "$OUTPUT_DIR/report.txt"
echo "Full logs saved in: $LOG_FILE" >> "$OUTPUT_DIR/report.txt"
echo "" >> "$OUTPUT_DIR/report.txt"
echo "Completed at: $(date)" >> "$OUTPUT_DIR/report.txt"
echo "================================================================================" >> "$OUTPUT_DIR/report.txt"

# Display report
cat "$OUTPUT_DIR/report.txt"

echo ""
echo "Batch test completed. Results saved in $OUTPUT_DIR/"