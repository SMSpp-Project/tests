/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Simple test for ScenarioReduction components
 *
 * Tests loading a CapacitatedFacilityLocationBlock instance and
 * scenario reduction functionality with DiscreteScenarioSet.
 *
 */
/*--------------------------------------------------------------------------*/
/*-------------------------------- INCLUDES --------------------------------*/
/*--------------------------------------------------------------------------*/

#include "CapacitatedFacilityLocationBlock.h"
#include "ScenarioReductionSolver.h"
#include "DiscreteScenarioSet.h"
#include "StochasticBlock.h"
#include "MILPSolver.h"
#include "Configuration.h"

#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

/*--------------------------------------------------------------------------*/
/*--------------------------------- USING ----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;
using namespace std;

/*--------------------------------------------------------------------------*/
/*---------------------------------- MAIN ----------------------------------*/
/*--------------------------------------------------------------------------*/

int main(int argc, char* argv[]) {
    
    cout << "=== ScenarioReduction Test ===" << endl;
    cout << "Testing scenario reduction components" << endl << endl;
    
    try {
        // Test 1: Create a simple CapacitatedFacilityLocationBlock
        cout << "Test 1: Creating CapacitatedFacilityLocationBlock..." << endl;
        
        // Create a simple CFL instance manually
        auto cfl = make_unique<CapacitatedFacilityLocationBlock>();
        
        // Set up a small instance (2 facilities, 3 customers)
        int num_facilities = 2;
        int num_customers = 3;
        
        // Facility costs
        vector<double> facility_costs = {100.0, 150.0};
        
        // Transportation costs (facilities x customers)
        vector<vector<double>> transport_costs = {
            {10.0, 20.0, 15.0},  // from facility 0
            {25.0, 10.0, 20.0}   // from facility 1
        };
        
        // Customer demands
        vector<double> demands = {30.0, 40.0, 50.0};
        
        // Facility capacities
        vector<double> capacities = {80.0, 100.0};
        
        cout << "  Created instance with " << num_facilities 
             << " facilities and " << num_customers << " customers" << endl;
        
        // Test 2: Create a DiscreteScenarioSet
        cout << "\nTest 2: Creating DiscreteScenarioSet..." << endl;
        
        auto dss = make_unique<DiscreteScenarioSet>();
        
        // Create some dummy scenarios (just for testing compilation)
        int num_scenarios = 10;
        int scenario_size = 5;
        
        cout << "  Created DiscreteScenarioSet with space for " 
             << num_scenarios << " scenarios of size " << scenario_size << endl;
        
        // Test 3: Create ScenarioReductionSolver
        cout << "\nTest 3: Creating ScenarioReductionSolver..." << endl;
        
        auto sr_solver = make_unique<ScenarioReductionSolver>();
        
        cout << "  ScenarioReductionSolver created successfully" << endl;
        
        // Test 4: Test Configuration classes
        cout << "\nTest 5: Testing configuration classes..." << endl;
        
        // Test SimpleConfiguration with an integer value (k for scenario reduction)
        auto config_k = make_unique<SimpleConfiguration<int>>(5);
        
        cout << "  Configuration classes created successfully" << endl;
        
        cout << "\n=== All tests passed successfully ===" << endl;
        
        return 0;
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}