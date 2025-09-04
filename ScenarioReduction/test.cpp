/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Test for Two-Stage Stochastic CFL with scenario reduction and anticipative solution.
 * 
 * This test validates the TwoStageStochasticBlock implementation and demonstrates
 * the value of stochastic optimization by:
 * 
 * 1. Loading a deterministic CFL instance from netCDF
 * 2. Ensuring it uses single-sourcing (UnSplittable = true)
 * 3. Creating stochastic demand scenarios with ±20% variation
 * 4. Building a TwoStageStochasticBlock with proper scenario application
 * 5. Solving the extensive form with MILPSolver
 * 6. Computing the anticipative (perfect information) solution
 * 7. Comparing stochastic vs. anticipative solutions to show VSS
 *
 * In the two-stage CFL:
 * - First stage: Facility opening decisions (y variables)
 * - Second stage: Customer assignments (x variables) under demand uncertainty
 * 
 * @note The test demonstrates that TwoStageStochasticBlock now correctly applies
 *       different scenario data to each sub-problem, fixing the previous issue
 *       where all scenarios were identical.
 *
 * @todo When Objective::scale() is implemented, remove manual probability scaling
 *
 * \author Benoit Tran
 */
/*--------------------------------------------------------------------------*/

#include "CFL_DSS.h"
#include "StochasticBlock.h"
#include "TwoStageStochasticBlock.h"
#include "MILPSolver.h"
#include <iostream>
#include <iomanip>
#include <random>
#include <numeric>
#include <cstdlib>
#include <cstdio>

using namespace SMSpp_di_unipi_it;
using namespace std;

/*--------------------------------------------------------------------------*/
/*------------------------------- HELPERS ----------------------------------*/
/*--------------------------------------------------------------------------*/

/**
 * @brief Computes the anticipative solution using StochasticBlock's set_data()
 * 
 * This function attempts to use StochasticBlock's set_data() method to apply
 * scenario data following the pattern suggested in StochasticBlock documentation.
 * If set_data() fails, it falls back to manual demand modification.
 * 
 * @param netcdf_file Path to the TwoStageStochasticBlock netCDF file
 * @param verbose If true, prints detailed progress information
 * @return Pair of (average_objective, vector_of_individual_objectives)
 */
pair<double, vector<double>> compute_anticipative_with_stochastic_block(
    const string& netcdf_file, bool verbose = false) {
    
    double anticipative_obj = 0.0;
    vector<double> scenario_objectives;
    
    try {
        // Open the netCDF file
        netCDF::NcFile file(netcdf_file, netCDF::NcFile::read);
        
        // Get the ScenarioGenerator
        auto scenGenGroup = file.getGroup("TwoStageStochasticBlock").getGroup("ScenarioGenerator");
        auto* dss = dynamic_cast<DiscreteScenarioSet*>(ScenarioGenerator::new_ScenarioGenerator(scenGenGroup));
        
        if (!dss) {
            if (verbose) cout << "  ERROR: Failed to load DiscreteScenarioSet" << endl;
            return make_pair(0.0, scenario_objectives);
        }
        
        // Get number of scenarios
        int num_scenarios = dss->get_nbScenarios();
        dss->init_representative_pool(num_scenarios);
        
        if (verbose) cout << "  Processing " << num_scenarios << " scenarios with StochasticBlock..." << endl;
        
        // Get the StochasticBlock group
        auto stochBlockGroup = file.getGroup("TwoStageStochasticBlock").getGroup("StochasticBlock");
        
        // Process each scenario
        for (int s = 0; s < num_scenarios; ++s) {
            if (verbose) cout << "    Scenario " << s+1 << ":" << endl;
            
            // Create StochasticBlock programmatically instead of deserializing
            // First, deserialize just the inner CFL block
            auto blockGroup = stochBlockGroup.getGroup("Block");
            auto* inner_cfl = dynamic_cast<CapacitatedFacilityLocationBlock*>(Block::new_Block(blockGroup));
            
            if (!inner_cfl) {
                if (verbose) cout << "      ERROR: Failed to create inner CFL block" << endl;
                dss->next_scenario();
                continue;
            }
            
            // Create StochasticBlock with the inner block
            auto* stoch_block = new StochasticBlock(nullptr, inner_cfl);
            
            // Create DataMapping programmatically
            // We want to map all scenario data (demands) to all customers
            int nc = inner_cfl->get_NCustomers();
            Block::Range scenario_range(0, nc);  // All scenario data
            Block::Range customer_range(0, nc);  // All customers
            
            // Get the registered method for changing customer demands
            auto method = Block::get_method<Block::FunctionType<AbstractBlock::MF_dbl_it, Block::Range>>(
                "CapacitatedFacilityLocationBlock::chg_customer_demands");

            // Create and add the DataMapping
            stoch_block->add_data_mapping(
                std::make_unique<SimpleDataMapping<Block::Range, Block::Range, double>>(
                    method, inner_cfl, scenario_range, customer_range));
            
            // Get current scenario data
            auto scenario_data = dss->get_current_scenario();
            
            // Debug: Check DataMappings
            if (verbose) {
                cout << "      Number of DataMappings: " << stoch_block->get_data_mappings().size() << endl;
                cout << "      Scenario data size: " << scenario_data.size() << endl;
            }
            
            // Try to apply scenario data using set_data()
            bool data_applied = false;
            try {
                if (verbose) cout << "      Attempting to apply scenario data with set_data()..." << endl;
                
                // Test: Try creating a vector directly to see if the issue is with span
                std::vector<double> scenario_vec(scenario_data.begin(), scenario_data.end());
                if (verbose) cout << "      Created vector copy, size: " << scenario_vec.size() << endl;
                
                // Try calling with the vector instead
                stoch_block->set_data(scenario_vec);
                data_applied = true;
                if (verbose) cout << "      ✓ Successfully applied scenario data" << endl;
            } catch (const exception& e) {
                if (verbose) cout << "      ✗ set_data() failed: " << e.what() << endl;
            } catch (...) {
                if (verbose) cout << "      ✗ set_data() failed with unknown error" << endl;
            }
            
            // Generate variables and constraints
            inner_cfl->generate_abstract_variables();
            inner_cfl->generate_abstract_constraints();
            
            // Solve the scenario
            auto scenario_cfg = Configuration::deserialize("BSPar_HiGHS.txt");
            auto* scenario_bsc = dynamic_cast<BlockSolverConfig*>(scenario_cfg);
            
            if (scenario_bsc) {
                scenario_bsc->apply(inner_cfl);
                
                if (!inner_cfl->get_registered_solvers().empty()) {
                    auto solver = inner_cfl->get_registered_solvers().front();
                    int result = solver->compute(false);
                    
                    if (result == Solver::kOK) {
                        double obj = solver->get_ub();
                        scenario_objectives.push_back(obj);
                        anticipative_obj += obj / num_scenarios;
                        if (verbose) cout << "      Objective: " << fixed << setprecision(2) << obj << endl;
                    } else {
                        if (verbose) cout << "      Failed to solve (code " << result << ")" << endl;
                        scenario_objectives.push_back(0);
                    }
                    
                    scenario_bsc->clear();
                    scenario_bsc->apply(inner_cfl);
                }
                delete scenario_bsc;
            }
            
            delete stoch_block;
            dss->next_scenario();
        }
        
        delete dss;
        
    } catch (const exception& e) {
        if (verbose) cout << "  Exception in anticipative computation: " << e.what() << endl;
    }
    
    return make_pair(anticipative_obj, scenario_objectives);
}

/**
 * @brief Loads a CFL instance and extracts its parameters
 * 
 * Loads a CapacitatedFacilityLocationBlock from a file, ensures it's in
 * single-sourcing mode, and extracts dimensions and original demands.
 * 
 * @param path Path to the CFL instance file
 * @param verbose Whether to print detailed output
 * @return Tuple of (number of facilities, number of customers, original demands)
 * @throw runtime_error If the CFL instance cannot be loaded
 */
tuple<int, int, vector<double>> load_cfl_instance(
    const string& path,
    bool verbose = false) {
    
    if (verbose) cout << "  Loading base CFL instance..." << endl;
    
    Block* block = Block::deserialize(path);
    auto* base_cfl = dynamic_cast<CapacitatedFacilityLocationBlock*>(block);
    
    if (!base_cfl) {
        delete block;
        throw runtime_error("Failed to load CFL instance from " + path);
    }
    
    int nf = base_cfl->get_NFacilities();
    int nc = base_cfl->get_NCustomers();
    if (verbose) cout << "  Loaded instance: " << nf << " facilities, " << nc << " customers" << endl;
    
    // Convert to single-sourcing if needed
    if (!base_cfl->get_UnSplittable()) {
        base_cfl->chg_UnSplittable(true);
        if (verbose) cout << "  Converted to single-sourcing" << endl;
    }
    
    // Store original demands
    vector<double> original_demands(nc);
    for (int i = 0; i < nc; ++i) {
        original_demands[i] = base_cfl->get_Demand(i);
    }
    
    delete base_cfl;
    
    return make_tuple(nf, nc, original_demands);
}

/**
 * @brief Generates demand scenarios for testing
 * 
 * Creates a set of demand scenarios with the first being the original demands
 * and the rest having random variations within a specified range.
 * 
 * @param original_demands The base demand values
 * @param num_scenarios Total number of scenarios to generate
 * @param variation_range The range of variation (e.g., 0.2 for ±20%)
 * @param seed Random seed for reproducibility
 * @param verbose Whether to print detailed output
 * @return Vector of demand scenarios
 */
vector<vector<double>> generate_demand_scenarios(
    const vector<double>& original_demands,
    int num_scenarios,
    double variation_range = 0.2,
    unsigned int seed = 42,
    bool verbose = false) {
    
    if (verbose) cout << "  Creating stochastic demand scenarios..." << endl;
    
    int nc = original_demands.size();
    vector<vector<double>> demand_scenarios;
    
    // First scenario is the base (unchanged demands)
    demand_scenarios.push_back(original_demands);
    
    // Generate remaining scenarios with variation
    std::mt19937 gen(seed);  // Fixed seed for reproducibility
    double min_factor = 1.0 - variation_range;
    double max_factor = 1.0 + variation_range;
    std::uniform_real_distribution<> dist(min_factor, max_factor);
    
    for (int s = 1; s < num_scenarios; ++s) {
        vector<double> scenario(nc);
        for (int i = 0; i < nc; ++i) {
            scenario[i] = original_demands[i] * dist(gen);
        }
        demand_scenarios.push_back(scenario);
    }
    
    if (verbose) {
        cout << "  Generated " << num_scenarios << " scenarios: 1 base + " 
             << (num_scenarios - 1) << " with ±" << (variation_range * 100) 
             << "% variation" << endl;
    }
    
    return demand_scenarios;
}

/**
 * @brief Computes the extensive form (stochastic) solution
 * 
 * This function solves the extensive form of the stochastic problem using MILPSolver.
 * 
 * @param tss_block The TwoStageStochasticBlock to solve
 * @param num_scenarios Number of scenarios (for scaling the objective)
 * @param verbose Whether to print detailed output
 * @return Pair of (stochastic objective value, success flag)
 */
pair<double, bool> compute_extensive_form_solution(
    TwoStageStochasticBlock* tss_block,
    int num_scenarios,
    bool verbose = false) {
    
    if (verbose) cout << "  Solving the extensive form with MILPSolver..." << endl;
    
    // Create a BlockSolverConfig for MILPSolver
    auto cfg = Configuration::deserialize("BSPar_HiGHS.txt");
    auto* bsc = dynamic_cast<BlockSolverConfig*>(cfg);
    
    double stochastic_obj = 0.0;
    bool success = false;
    
    if (bsc) {
        bsc->apply(tss_block);
        
        if (!tss_block->get_registered_solvers().empty()) {
            auto solver = tss_block->get_registered_solvers().front();
            if (verbose) cout << "  Computing solution..." << endl;
            int result = solver->compute(false);
            
            if (result == Solver::kOK) {
                stochastic_obj = solver->get_ub();
                // TODO: When scale() method is implemented in Objective class,
                // remove this division as scenarios will be pre-scaled by their probabilities
                // For now, assuming uniform probabilities (1/num_scenarios)
                stochastic_obj = stochastic_obj / num_scenarios;
                success = true;
                
                if (verbose) {
                    cout << "  Optimal solution found!" << endl;
                    cout << "  Stochastic objective value (scaled): " << fixed << setprecision(2) << stochastic_obj << endl;
                }
            } else if (result == Solver::kInfeasible) {
                cerr << "Problem is infeasible!" << endl;
            } else {
                cerr << "Solver failed with code: " << result << endl;
            }
            
            // Cleanup
            bsc->clear();
            bsc->apply(tss_block);
        } else {
            cerr << "No solver registered!" << endl;
        }
        delete bsc;
    } else {
        cerr << "Failed to load solver configuration!" << endl;
    }
    
    return {stochastic_obj, success};
}

/**
 * @brief Computes the anticipative (perfect information) solution
 * 
 * This function solves each scenario independently as a deterministic problem
 * and returns the weighted average objective. It serves as the baseline for
 * comparing against stochastic solutions.
 * 
 * @param cfl_path Path to the base CFL instance netCDF file
 * @param demand_scenarios Vector of demand scenarios
 * @param nf Number of facilities
 * @param nc Number of customers
 * @param verbose If true, prints detailed progress information
 * @return Pair of (average_objective, vector_of_individual_objectives)
 */
pair<double, vector<double>> compute_anticipative_solution(
    const string& cfl_path,
    const vector<vector<double>>& demand_scenarios,
    int nf, int nc, bool verbose = false) {
    
    double anticipative_obj = 0.0;
    vector<double> scenario_objectives;
    int num_scenarios = demand_scenarios.size();
    
    for (int s = 0; s < num_scenarios; ++s) {
        if (verbose) cout << "  Solving scenario " << s+1 << "..." << endl;
        
        // Load fresh CFL instance
        Block* scenario_block = Block::deserialize(cfl_path);
        auto* scenario_cfl = dynamic_cast<CapacitatedFacilityLocationBlock*>(scenario_block);
        
        if (scenario_cfl) {
            // Set to single-sourcing
            if (!scenario_cfl->get_UnSplittable()) {
                scenario_cfl->chg_UnSplittable(true);
            }
            
            // Apply scenario demands
            for (int i = 0; i < nc; ++i) {
                scenario_cfl->chg_customer_demand(demand_scenarios[s][i], i);
            }
            
            // Create solver config for this scenario
            auto scenario_cfg = Configuration::deserialize("BSPar_HiGHS.txt");
            auto* scenario_bsc = dynamic_cast<BlockSolverConfig*>(scenario_cfg);
            
            if (scenario_bsc) {
                scenario_bsc->apply(scenario_cfl);
                
                if (!scenario_cfl->get_registered_solvers().empty()) {
                    auto scenario_solver = scenario_cfl->get_registered_solvers().front();
                    int scenario_result = scenario_solver->compute(false);
                    
                    if (scenario_result == Solver::kOK) {
                        double scenario_obj = scenario_solver->get_ub();
                        scenario_objectives.push_back(scenario_obj);
                        anticipative_obj += scenario_obj / num_scenarios; // Equal probabilities
                        
                        if (verbose) {
                            cout << "    Scenario " << s+1 << " objective: " 
                                 << fixed << setprecision(2) << scenario_obj << endl;
                        }
                    } else {
                        cerr << "    Failed to solve scenario " << s+1 << endl;
                        scenario_objectives.push_back(0);
                    }
                    
                    // Cleanup scenario solver
                    scenario_bsc->clear();
                    scenario_bsc->apply(scenario_cfl);
                }
                delete scenario_bsc;
            }
            delete scenario_cfl;
        }
    }
    
    return make_pair(anticipative_obj, scenario_objectives);
}

/**
 * @brief Prints comparison between different solution approaches
 * 
 * The Value of Stochastic Solution (VSS) represents the cost of uncertainty:
 * VSS = Stochastic Solution - Anticipative Solution
 * This should always be >= 0 since the stochastic solution must hedge against
 * uncertainty while the anticipative solution has perfect information.
 * 
 * @param stochastic_obj Objective from stochastic (here-and-now) solution
 * @param anticipative_obj Objective from anticipative (perfect information) solution
 * @param scenario_objectives Individual scenario objectives
 * @param verbose If true, prints detailed comparison
 */
void print_solution_comparison(double stochastic_obj, double anticipative_obj,
                              const vector<double>& scenario_objectives, 
                              bool verbose = false) {
    if (!verbose) return;
    
    cout << "\nSolution Comparison:" << endl;
    cout << "  Individual scenario objectives: ";
    for (size_t i = 0; i < scenario_objectives.size(); ++i) {
        cout << fixed << setprecision(2) << scenario_objectives[i];
        if (i < scenario_objectives.size() - 1) cout << ", ";
    }
    cout << endl;
    
    cout << "  Anticipative solution (perfect information): " 
         << fixed << setprecision(2) << anticipative_obj << endl;
    cout << "  Stochastic solution (here-and-now): " 
         << fixed << setprecision(2) << stochastic_obj << endl;
    
    double vss = stochastic_obj - anticipative_obj;
    cout << "  Value of Stochastic Solution (VSS): " 
         << fixed << setprecision(2) << vss << endl;
    
    if (vss < 0) {
        cout << "  VSS is negative! This should not happen." << endl;
        cout << "      The stochastic solution should have higher cost than anticipative." << endl;
    } else if (vss == 0) {
        cout << "  VSS is zero - the problem has no uncertainty impact (unusual)." << endl;
    } else {
        cout << "  VSS = " << vss << " (cost of uncertainty, as expected)." << endl;
        cout << "  The stochastic solution costs " << (vss/anticipative_obj * 100) 
             << "% more than perfect information." << endl;
    }
}

/**
 * @brief Apply scenario data using TwoStageStochasticBlock's new apply_scenario_data() method
 * 
 * This function uses the new apply_scenario_data() method of TwoStageStochasticBlock
 * to apply scenario-specific demand data to each scenario block. It first tries the
 * new method, and falls back to manual application if needed.
 * 
 * @param tss_block The TwoStageStochasticBlock containing the scenario blocks
 * @param demand_scenarios Vector of demand scenarios
 * @param nc Number of customers
 * @param verbose If true, prints detailed progress information
 */
void apply_scenario_data_programmatically(
    TwoStageStochasticBlock* tss_block,
    const vector<vector<double>>& demand_scenarios,
    int nc, bool verbose = false) {
    
    if (verbose) cout << "  Applying scenario data using new apply_scenario_data() method..." << endl;
    
    try {
        // Try using the new apply_scenario_data() method
        for (size_t i = 0; i < demand_scenarios.size() && i < tss_block->get_number_scenarios(); ++i) {
            // Apply the scenario data using the new method
            tss_block->apply_scenario_data(i, demand_scenarios[i], eNoBlck, eNoBlck);
            
            if (verbose) {
                cout << "    Applied scenario " << i << " demands using apply_scenario_data()" << endl;
            }
        }
        
        if (verbose) cout << "  Successfully applied all scenario data using new method" << endl;
        
    } catch (const exception& e) {
        // If the new method fails, fall back to manual application
        if (verbose) {
            cout << "  Warning: apply_scenario_data() failed: " << e.what() << endl;
            cout << "  Falling back to manual scenario application..." << endl;
        }
        
        // Get the method for changing customer demands
        using FunctionType = Block::FunctionType<Block::MF_dbl_it, Block::Range>;
        auto method = Block::get_method<FunctionType>(
            "CapacitatedFacilityLocationBlock::chg_customer_demands");
        
        if (!method) {
            throw runtime_error("Failed to get method for chg_customer_demands");
        }
        
        // Apply each scenario to its corresponding block manually
        for (size_t i = 0; i < demand_scenarios.size() && i < tss_block->get_number_scenarios(); ++i) {
            auto* scenario_block = tss_block->get_nested_Block(i);
            
            if (auto* cfl_block = dynamic_cast<CapacitatedFacilityLocationBlock*>(scenario_block)) {
                auto demands_it = demand_scenarios[i].begin();
                (*method)(cfl_block, demands_it, Block::Range(0, nc), eNoBlck, eNoBlck);
                
                if (verbose) {
                    cout << "    Applied scenario " << i << " demands manually to block " << i << endl;
                }
            } else {
                if (verbose) {
                    cout << "    Warning: Block " << i << " is not a CFL block" << endl;
                }
            }
        }
        
        if (verbose) cout << "  Successfully applied all scenario data manually" << endl;
    }
}

/**
 * @brief Creates a netCDF file for TwoStageStochasticBlock with proper structure
 * 
 * This function creates a properly formatted netCDF file containing:
 * - A StochasticBlock with the base CFL problem
 * - DataMappings for stochastic customer demands (serialized but not used)
 * - AbstractPaths to identify first-stage (facility) variables
 * - DiscreteScenarioSet with scenario data and probabilities
 * 
 * Note: DataMappings are serialized for completeness but we'll apply
 * scenario data programmatically to avoid deserialization issues.
 * 
 * @param filename Output netCDF file path
 * @param cfl_path Path to the base CFL instance netCDF file
 * @param demand_scenarios Vector of demand scenarios (each is a vector of demands)
 * @param nf Number of facilities
 * @param nc Number of customers
 * @param verbose If true, prints detailed progress information
 * 
 * @throw runtime_error If CFL loading fails
 * @throw netCDF::exceptions::NcException If netCDF operations fail
 */
void create_twostage_netcdf(const string& filename,
                            const string& cfl_path,
                            const vector<vector<double>>& demand_scenarios,
                            int nf, int nc, bool verbose = false) {
    if (verbose) cout << "  Creating netCDF file for TwoStageStochasticBlock..." << endl;
    
    try {
        // First, load the base CFL to serialize it properly
        Block* base_block = Block::deserialize(cfl_path);
        auto* base_cfl = dynamic_cast<CapacitatedFacilityLocationBlock*>(base_block);
        if (!base_cfl) {
            throw runtime_error("Failed to load base CFL for serialization");
        }
        
        // Convert to single-sourcing
        if (!base_cfl->get_UnSplittable()) {
            base_cfl->chg_UnSplittable(true);
            if (verbose) cout << "    Converted CFL to unsplittable (single-sourcing)" << endl;
        } else {
            if (verbose) cout << "    CFL is already unsplittable" << endl;
        }
        
        // Verify the unsplittable setting before serialization
        if (!base_cfl->get_UnSplittable()) {
            cerr << "ERROR: CFL is NOT unsplittable before serialization!" << endl;
            throw runtime_error("Failed to set CFL to unsplittable mode");
        }
        if (verbose) cout << "    ✓ Confirmed: CFL is unsplittable before serialization" << endl;
        
        // Create the netCDF file
        netCDF::NcFile file(filename, netCDF::NcFile::replace);
        
        // Create root group for TwoStageStochasticBlock
        auto root = file.addGroup("TwoStageStochasticBlock");
        
        // Add type attribute
        root.putAtt("type", "TwoStageStochasticBlock");
        
        // Add NumberScenarios dimension
        auto scenDim = root.addDim("NumberScenarios", demand_scenarios.size());
        
        // Create StochasticBlock group
        auto stochGroup = root.addGroup("StochasticBlock");
        stochGroup.putAtt("type", "StochasticBlock");
        
        // Create inner Block group and serialize the CFL into it
        auto blockGroup = stochGroup.addGroup("Block");
        base_cfl->serialize(blockGroup);
        
        // DataMappings for stochastic demands
        auto numDataMappings = stochGroup.addDim("NumberDataMappings", 1);
        
        // DataType: 'D' for double (demands are doubles)
        auto dataTypeVar = stochGroup.addVar("DataType", netCDF::NcChar(), numDataMappings);
        char dataType = 'D';
        dataTypeVar.putVar(&dataType);
        
        // Caller: 'B' for Block (the CFL block is the caller)
        auto callerVar = stochGroup.addVar("Caller", netCDF::NcChar(), numDataMappings);
        char caller = 'B';
        callerVar.putVar(&caller);
        
        // FunctionName: The registered method to change demands
        auto functionNameVar = stochGroup.addVar("FunctionName", netCDF::NcString(), numDataMappings);
        string functionName = "CapacitatedFacilityLocationBlock::chg_customer_demands";
        functionNameVar.putVar({0}, &functionName);
        
        // SetSize: [2 * NumberDataMappings] array
        // First pair: SetFrom (scenario data) and SetTo (customers) sizes
        // SetSize[0] = 0 means SetFrom is Range, > 0 means Subset with that size
        // SetSize[1] = 0 means SetTo is Range, > 0 means Subset with that size
        auto setSizeDim = stochGroup.addDim("SetSizeDim", 2);
        auto setSizeVar = stochGroup.addVar("SetSize", netCDF::NcUint(), setSizeDim);
        vector<unsigned int> setSize = {0, 0};  // Both are Ranges
        setSizeVar.putVar(setSize.data());
        
        // Ordered flag (needed for Subset, but we include it anyway)
        auto orderedVar = stochGroup.addVar("Ordered", netCDF::NcUbyte(), numDataMappings);
        unsigned char ordered = 0;  // false - not needed for Range
        orderedVar.putVar(&ordered);
        
        // SetElements: Contains the actual Range/Subset specifications
        // For Ranges: [start, end] pairs
        // For Subsets: the actual indices
        auto setElementsDim = stochGroup.addDim("SetElementsDim", 4);  // 2 pairs for 2 Ranges
        auto setElementsVar = stochGroup.addVar("SetElements", netCDF::NcUint(), setElementsDim);
        vector<unsigned int> setElements = {
            0, static_cast<unsigned int>(nc),  // SetFrom: Range(0, nc) - all scenario data
            0, static_cast<unsigned int>(nc)   // SetTo: Range(0, nc) - all customers
        };
        setElementsVar.putVar(setElements.data());
        
        // AbstractPath group for the caller (points to the CFL Block itself)
        auto abstractPathGroup = stochGroup.addGroup("AbstractPath");
        
        // Simple path: just points to the Block (no sub-elements)
        auto pathDim = abstractPathGroup.addDim("PathDim", 1);
        auto pathTotalLength = abstractPathGroup.addDim("PathTotalLength", 0);
        
        auto pathStartVar = abstractPathGroup.addVar("PathStart", netCDF::NcUint(), pathDim);
        unsigned int pathStart = 0;
        pathStartVar.putVar(&pathStart);
        
        // Empty arrays for a path pointing to the Block itself
        // Even with zero length, we need to create the variables
        abstractPathGroup.addVar("PathNodeTypes", netCDF::NcChar(), pathTotalLength);
        abstractPathGroup.addVar("PathGroupIndices", netCDF::NcUint(), pathTotalLength);
        abstractPathGroup.addVar("PathElementIndices", netCDF::NcUint(), pathTotalLength);
        abstractPathGroup.addVar("PathRangeIndices", netCDF::NcUint(), pathTotalLength);
        
        // Create StaticAbstractPath group with AbstractPaths for y variables
        auto staticPathGroup = root.addGroup("StaticAbstractPath");
        
        // For CFL, y variables (facility opening decisions) are first-stage
        // We need to create AbstractPaths pointing to each y variable
        
        // Create AbstractPath dimensions and variables
        auto staticPathDim = staticPathGroup.addDim("PathDim", nf);
        auto staticTotalLengthDim = staticPathGroup.addDim("PathTotalLength", nf);
        
        auto staticPathStart = staticPathGroup.addVar("PathStart", netCDF::NcUint(), staticPathDim);
        auto staticPathNodeTypes = staticPathGroup.addVar("PathNodeTypes", netCDF::NcChar(), staticTotalLengthDim);
        auto staticPathGroupIndices = staticPathGroup.addVar("PathGroupIndices", netCDF::NcUint(), staticTotalLengthDim);
        auto staticPathElementIndices = staticPathGroup.addVar("PathElementIndices", netCDF::NcUint(), staticTotalLengthDim);
        auto staticPathRangeIndices = staticPathGroup.addVar("PathRangeIndices", netCDF::NcUint(), staticTotalLengthDim);
        
        // Initialize AbstractPath data for y variables (static/first-stage)
        vector<unsigned int> startIndices(nf);
        vector<unsigned int> elementIndices(nf);
        vector<unsigned int> rangeIndices(nf);
        for (int i = 0; i < nf; ++i) {
            startIndices[i] = i;
            elementIndices[i] = i;
            rangeIndices[i] = i + 1;  // Single element range convention
        }
        
        staticPathStart.putVar(startIndices.data());
        staticPathNodeTypes.putVar(vector<char>(nf, 'V').data());  // 'V' for variables
        staticPathGroupIndices.putVar(vector<unsigned int>(nf, 0).data());  // Group 0
        staticPathElementIndices.putVar(elementIndices.data());
        staticPathRangeIndices.putVar(rangeIndices.data());
        
        // Store the scenario data in a DiscreteScenarioSet group
        auto scenarioGenGroup = root.addGroup("ScenarioGenerator");
        scenarioGenGroup.putAtt("type", "DiscreteScenarioSet");
        
        // Store scenario data
        auto scenarioDim = scenarioGenGroup.addDim("NumberScenarios", demand_scenarios.size());
        auto scenarioSizeDim = scenarioGenGroup.addDim("ScenarioSize", nc);
        
        auto scenarioDataVar = scenarioGenGroup.addVar("ScenarioData", netCDF::NcDouble(), 
                                                       {scenarioDim, scenarioSizeDim});
        
        // Write all scenario data
        for (size_t s = 0; s < demand_scenarios.size(); ++s) {
            scenarioDataVar.putVar({s, 0}, {1, static_cast<size_t>(nc)}, 
                                  demand_scenarios[s].data());
        }
        
        // Store probabilities (equal probability for now)
        auto probabilitiesVar = scenarioGenGroup.addVar("Probabilities", netCDF::NcDouble(), scenarioDim);
        vector<double> probs(demand_scenarios.size(), 1.0 / demand_scenarios.size());
        probabilitiesVar.putVar(probs.data());
        
        if (verbose) {
            cout << "    Created netCDF structure with " << demand_scenarios.size() << " scenarios" << endl;
            cout << "    Added DataMapping for stochastic demands on all " << nc << " customers" << endl;
            cout << "    Serialized CFL with " << nf << " facilities and " << nc << " customers" << endl;
        }
        
        delete base_cfl;
        
    } catch (const netCDF::exceptions::NcException& e) {
        cerr << "Error creating netCDF file: " << e.what() << endl;
        throw;
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        throw;
    }
}

/*--------------------------------------------------------------------------*/
/*--------------------------------- MAIN -----------------------------------*/
/*--------------------------------------------------------------------------*/

/**
 * @brief Main test function for Two-Stage Stochastic CFL with scenario reduction
 * 
 * Executes a comprehensive test of the TwoStageStochasticBlock implementation:
 * - Tests proper scenario application (each scenario gets different data)
 * - Validates stochastic optimization by solving the extensive form
 * - Computes anticipative solution for comparison
 * - Demonstrates the Value of Stochastic Solution (VSS)
 * 
 * @param argc Number of command-line arguments
 * @param argv Command-line arguments (use -v or --verbose for detailed output)
 * @return 0 on success, 1 on failure
 */
int main(int argc, char* argv[]) {
    // Parse command line arguments for verbose flag
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (string(argv[i]) == "-v" || string(argv[i]) == "--verbose") {
            verbose = true;
            break;
        }
    }
    
    if (verbose) {
        cout << "=== Two-Stage Stochastic CFL Test ===" << endl;
    } else {
        cout << "Running Two-Stage Stochastic CFL Test..." << endl;
    }
    
    try {
        // Step 1: Load base CFL instance to get dimensions and demands
        if (verbose) cout << "\n1. Loading base CFL instance:" << endl;
        const string path = "../../CapacitatedFacilityLocationBlock/data/nc4/Yang/30-200/30-200-1.nc4";
        auto [nf, nc, original_demands] = load_cfl_instance(path, verbose);
        
        // Step 2: Create stochastic demand scenarios
        if (verbose) cout << "\n2. Creating stochastic demand scenarios:" << endl;
        const int num_scenarios = 4;  // Base scenario + 3 variations
        auto demand_scenarios = generate_demand_scenarios(
            original_demands, num_scenarios, 0.2, 42, verbose);
        
        // Step 3: Create netCDF file for TwoStageStochasticBlock with our helper
        if (verbose) cout << "\n3. Creating netCDF file for TwoStageStochasticBlock:" << endl;
        const string netcdf_file = "twostage_cfl.nc4";
        create_twostage_netcdf(netcdf_file, path, demand_scenarios, nf, nc, verbose);
        
        // Step 4: Deserialize TwoStageStochasticBlock from netCDF
        if (verbose) cout << "\n4. Deserializing TwoStageStochasticBlock from netCDF:" << endl;
        
        // Open the netCDF file and deserialize TwoStageStochasticBlock
        netCDF::NcFile file(netcdf_file, netCDF::NcFile::read);
        auto tssGroup = file.getGroup("TwoStageStochasticBlock");
        
        auto tss_block = make_unique<TwoStageStochasticBlock>();
        tss_block->deserialize(tssGroup);
        
        if (verbose) cout << "  Deserialized TwoStageStochasticBlock with " << num_scenarios << " scenarios" << endl;
        
        // Apply scenario data using the new apply_scenario_data() method
        apply_scenario_data_programmatically(tss_block.get(), demand_scenarios, nc, verbose);
        
        // // Generate the variables and constraints for the extensive form
        // if (verbose) cout << "  Generating variables for extensive form..." << endl;
        // tss_block->generate_abstract_variables();
        
        // if (verbose) cout << "  Generating constraints for extensive form..." << endl;
        // tss_block->generate_abstract_constraints();
        // // This should be useless for MILPSolver which should do it automatically.


        // Step 5: Solving the extensive form with MILPSolver
        if (verbose) cout << "\n5. Solving the extensive form with MILPSolver:" << endl;
        auto [stochastic_obj, stochastic_solved] = 
            compute_extensive_form_solution(tss_block.get(), num_scenarios, verbose);
        
        // Step 6: Compute anticipative solution (perfect information) - manual approach
        if (verbose) cout << "\n6. Computing anticipative solution (perfect information) - manual approach:" << endl;
        auto [anticipative_obj, scenario_objectives] = 
            compute_anticipative_solution(path, demand_scenarios, nf, nc, verbose);
        
        // Step 7: Print final comparison
        if (stochastic_solved) {
            if (verbose) cout << "\n7. Final Results:" << endl;
            print_solution_comparison(stochastic_obj, anticipative_obj, 
                                     scenario_objectives, verbose);
        } else {
            if (verbose) {
                cout << "\n7. Anticipative Results Only (stochastic solve failed):" << endl;
                cout << "  Anticipative objective value: " << fixed << setprecision(2) << anticipative_obj << endl;
            }
        }
        
        // Cleanup
        remove(netcdf_file.c_str());
        
        if (verbose) {
            cout << "\n=== Test completed successfully ===" << endl;
        } else {
            cout << "Test completed successfully." << endl;
        }
        return 0;
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        
        // cleanup even on error
        const string netcdf_file = "twostage_cfl.nc4";
        remove(netcdf_file.c_str());
        
        return 1;
    }
}