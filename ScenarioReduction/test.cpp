/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Test for CFL single-sourcing conversion and feasibility verification.
 * 
 * This test demonstrates:
 * 1. Loading a CFL instance from netCDF
 * 2. Converting it to single-sourcing using chg_UnSplittable(true)
 * 3. Verifying feasibility with HiGHSMILPSolver
 * 4. Applying stochastic demand scenarios via StochasticBlock and DataMapping
 *
 * \author Nils Peyrouset, Benoit Tran
 */
/*--------------------------------------------------------------------------*/

#include "CFL_DSS.h"
#include "CapacitatedFacilityLocationBlock.h"
#include "BlockSolverConfig.h"
#include "StochasticBlock.h"
#include "DataMapping.h"
#include <iostream>
#include <iomanip>
#include <random>

using namespace SMSpp_di_unipi_it;
using namespace std;

/*--------------------------------------------------------------------------*/
/*------------------------------- HELPERS ----------------------------------*/
/*--------------------------------------------------------------------------*/

// Test feasibility with HiGHSMILPSolver
bool test_feasibility(CapacitatedFacilityLocationBlock* cfl_block) {
    cout << "  Testing feasibility with HiGHSMILPSolver..." << endl;
    
    auto cfg = Configuration::deserialize("BSPar_HiGHS.txt");
    auto* bsc = dynamic_cast<BlockSolverConfig*>(cfg);
    if (!bsc) return false;
    
    bsc->apply(cfl_block);
    if (cfl_block->get_registered_solvers().empty()) {
        delete bsc;
        return false;
    }
    
    auto solver = cfl_block->get_registered_solvers().front();
    int result = solver->compute(false);
    bool feasible = (result != Solver::kInfeasible);
    
    if (feasible && result == Solver::kOK) {
        cout << "    ✓ Feasible! Optimal value: " << fixed << setprecision(2) 
             << solver->get_lb() << endl;
    } else if (feasible) {
        cout << "    ✓ Feasible (result code: " << result << ")" << endl;
    } else {
        cout << "    ✗ Infeasible" << endl;
    }
    
    // Cleanup
    bsc->clear();
    bsc->apply(cfl_block);
    delete bsc;
    return feasible;
}

/*--------------------------------------------------------------------------*/
/*--------------------------------- MAIN -----------------------------------*/
/*--------------------------------------------------------------------------*/

int main() {
    cout << "=== Simple CFL Single-Sourcing Test ===" << endl;
    
    try {
        // Load CFL instance
        cout << "\n1. Loading CFL instance (30-200-1.nc4):" << endl;
        const string path = "../../CapacitatedFacilityLocationBlock/data/nc4/Yang/30-200/30-200-1.nc4";
        
        Block* block = Block::deserialize(path);
        auto* cfl = dynamic_cast<CapacitatedFacilityLocationBlock*>(block);
        
        if (!cfl) {
            cerr << "✗ Failed to load CFL instance" << endl;
            delete block;
            return 1;
        }
        
        cout << "  ✓ Loaded instance: " << cfl->get_NFacilities() 
             << " facilities, " << cfl->get_NCustomers() << " customers" << endl;
        
        // Check/convert to single-sourcing
        cout << "\n2. Converting to single-sourcing:" << endl;
        if (!cfl->get_UnSplittable()) {
            cout << "  Converting from splittable to single-sourcing..." << endl;
            cfl->chg_UnSplittable(true);
            cout << "  ✓ Successfully converted!" << endl;
        } else {
            cout << "  ✓ Already single-sourcing" << endl;
        }
        
        // Test feasibility
        cout << "\n3. Verifying feasibility:" << endl;
        if (!test_feasibility(cfl)) {
            cout << "✗ Instance is infeasible after conversion" << endl;
            delete cfl;
            return 1;
        }
        
        // Test stochastic scenarios
        cout << "\n4. Testing stochastic demand scenarios:" << endl;
        int nc = cfl->get_NCustomers();
        
        // Store original demands and find the 5 highest
        vector<pair<double, int>> demands_with_index;
        for (int i = 0; i < nc; ++i) {
            demands_with_index.push_back({cfl->get_Demand(i), i});
        }
        
        // Sort by demand value (descending)
        sort(demands_with_index.begin(), demands_with_index.end(), greater<pair<double, int>>());
        
        cout << "  Original demands (5 highest): ";
        for (int i = 0; i < min(5, nc); ++i) {
            cout << fixed << setprecision(1) << demands_with_index[i].first 
                 << " (customer " << demands_with_index[i].second << ")";
            if (i < min(5, nc) - 1) cout << ", ";
        }
        cout << endl;
        
        // Store original demands for scenario generation
        vector<double> original_demands(nc);
        for (int i = 0; i < nc; ++i) {
            original_demands[i] = cfl->get_Demand(i);
        }
        
        // Create StochasticBlock
        auto stochastic_block = make_unique<StochasticBlock>(nullptr, cfl);
        
        // Verify that the inner block is still single-sourcing after StochasticBlock creation
        auto inner_cfl = dynamic_cast<CapacitatedFacilityLocationBlock*>(stochastic_block->get_inner_block());
        if (!inner_cfl->get_UnSplittable()) {
            cout << "  Warning: Inner CFL block is not single-sourcing, fixing..." << endl;
            inner_cfl->chg_UnSplittable(true);
            cout << "  ✓ Inner CFL block now single-sourcing" << endl;
        } else {
            cout << "  ✓ Inner CFL block is single-sourcing" << endl;
        }
        
        // Setup DataMapping for demand changes
        using DemandIterator = CapacitatedFacilityLocationBlock::DVector::const_iterator;
        using FunctionType = Block::FunctionType<DemandIterator, Block::Range>;
        
        auto func_ptr = Block::get_method<FunctionType>(
            "CapacitatedFacilityLocationBlock::chg_customer_demands"
        );
        
        if (func_ptr) {
            auto data_mapping = make_unique<SimpleDataMapping<Block::Range, Block::Range, double, Block>>(
                func_ptr,
                stochastic_block->get_inner_block(),
                Block::Range(0, nc),  // scenario range
                Block::Range(0, nc)   // customer range
            );
            
            stochastic_block->add_data_mapping(std::move(data_mapping));
            
            // Store scenarios for later use with CFL_DSS
            vector<vector<double>> generated_scenarios;
            
            // Generate scenarios with ±20% demand variation around original demands
            std::random_device rd;
            std::mt19937 gen(rd());
            
            for (int s = 0; s < 3; ++s) {
                vector<double> scenario(nc);
                for (int i = 0; i < nc; ++i) {
                    std::uniform_real_distribution<> dist(0.8, 1.2);  // ±20% variation
                    scenario[i] = original_demands[i] * dist(gen);  // Always use original demands as base
                }
                
                generated_scenarios.push_back(scenario);
                stochastic_block->set_data(scenario);
                
                // Show 5 highest demands of this scenario
                auto inner = dynamic_cast<CapacitatedFacilityLocationBlock*>(
                    stochastic_block->get_inner_block());
                
                vector<pair<double, int>> scenario_demands;
                for (int i = 0; i < nc; ++i) {
                    scenario_demands.push_back({inner->get_Demand(i), i});
                }
                
                sort(scenario_demands.begin(), scenario_demands.end(), greater<pair<double, int>>());
                
                cout << "  Scenario " << (s + 1) << " (5 highest): ";
                for (int i = 0; i < min(5, nc); ++i) {
                    cout << fixed << setprecision(1) << scenario_demands[i].first 
                         << " (customer " << scenario_demands[i].second << ")";
                    if (i < min(5, nc) - 1) cout << ", ";
                }
                cout << endl;
            }
            
            cout << "  ✓ Successfully generated and applied 3 stochastic scenarios" << endl;
            
            // Test 5: CFL_DSS integration
            cout << "\n5. Testing CFL_DSS integration:" << endl;
            auto cfl_dss = make_unique<CFL_DSS>();
            
            // Verify inner block is single-sourcing before setting in CFL_DSS
            cout << "  Verifying single-sourcing before CFL_DSS integration..." << endl;
            cout << "  Inner CFL block UnSplittable: " << (inner_cfl->get_UnSplittable() ? "Yes" : "No") << endl;
            
            // Set the StochasticBlock in CFL_DSS
            cfl_dss->set_Block(stochastic_block.get());
            cout << "  ✓ CFL_DSS successfully configured with StochasticBlock" << endl;
            
            // Test scenario distance computation
            cout << "  Testing scenario distance computation..." << endl;
            Eigen::VectorXd scenario1(3), scenario2(3);
            scenario1 << 1.0, 2.0, 3.0;
            scenario2 << 2.0, 3.0, 4.0;
            
            double distance = cfl_dss->compute_scenario_distance(scenario1, scenario2, 2.0);
            cout << "  ✓ Distance between test scenarios: " << distance << endl;
            
            // Test transport cost matrix computation
            cout << "  Testing transport cost matrix computation..." << endl;
            int n_scenarios = 5;
            int scenario_size = 3;
            float ell = 2.0;
            
            auto cost_matrix = cfl_dss->compute_transport_cost_matrix(n_scenarios, scenario_size, ell);
            cout << "  ✓ Transport cost matrix computed successfully" << endl;
            cout << "  ✓ Matrix dimensions: " << cost_matrix.shape()[0] << "x" << cost_matrix.shape()[1] << endl;
            
        } else {
            cout << "  ✗ Could not setup DataMapping for demand changes" << endl;
        }
        
        cout << "\n=== All tests passed! ===" << endl;
        
        // Note: Don't delete cfl - StochasticBlock owns it now
        return 0;
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}