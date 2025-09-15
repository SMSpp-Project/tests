/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Test for Two-Stage Stochastic CFL with scenario reduction and Value of Perfect Information.
 * 
 * This test validates the TwoStageStochasticBlock implementation and demonstrates
 * scenario reduction techniques and the Value of Perfect Information (VPI) by:
 * 
 * 1. Loading a deterministic CFL instance from netCDF
 * 2. Ensuring it uses single-sourcing (UnSplittable = true)
 * 3. Creating stochastic demand scenarios with ±20% variation
 * 4. Performing scenario reduction using DiscreteScenarioSet's Wasserstein-based method
 * 5. Solving both full and reduced extensive forms with MILPSolver
 * 6. Computing anticipative (perfect information) solutions for comparison
 * 7. Comparing results to show reduction quality, speedup, and VPI
 *
 * In the two-stage CFL:
 * - First stage: Facility opening decisions (y variables)
 * - Second stage: Customer assignments (x variables) under demand uncertainty
 * 
 * Command-line options:
 * - --verbose=<level>: Set verbosity level (0=silent, 1=normal, 2=detailed)
 * - -v: Set verbose level to 1, -vv: Set verbose level to 2
 * - -time=<seconds>: Set solver time limit (e.g., -time=300 for 300 seconds)
 * - -n_scen=<number>: Set total number of scenarios (default: 20)
 * - -n_reduced=<number>: Set number of reduced scenarios (default: 3)
 * - -method=<name>: Scenario reduction method (baseline, dupacova (default), bestfit, firstfit, milp)
 * - --save-cache: Save scenarios and results to cache files
 * - --load-cache: Load scenarios from cache files
 * - --load-results: Load pre-computed results from cache
 * - --cache-dir=<path>: Specify cache directory (default: ./cache/)
 * - --compute-vpi: Compute Value of Perfect Information (anticipative solutions)
 * 
 * Example usage:
 * - ./ScenarioReduction_test -n_scen=50 -n_reduced=5 -time=60 --verbose=2
 *   Creates 50 scenarios, reduces to 5, with 60s time limit and detailed output
 * - ./ScenarioReduction_test --load-cache --load-results -v
 *   Load cached scenarios and results with normal verbosity
 * - ./ScenarioReduction_test -n_scen=20 -n_reduced=5 --compute-vpi -v
 *   Run with VPI computation enabled
 *
 * \author Benoît Tran \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Benoît Tran
 */
/*--------------------------------------------------------------------------*/

#include "CFL_DSS.h"
#include "StochasticBlock.h"
#include "TwoStageStochasticBlock.h"
#include "MILPSolver.h"
#include "DiscreteScenarioSet.h"
#include "BlockSolverConfig.h"
#include "ScenarioReductionSolver.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <random>
#include <numeric>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace SMSpp_di_unipi_it;
using namespace std;

/*--------------------------------------------------------------------------*/
/*--------------------------- RESULT STRUCTURES ----------------------------*/
/*--------------------------------------------------------------------------*/

/**
 * @brief Structure to hold solution results with timing information
 */
struct SolutionResult {
    double objective = 0.0;
    bool solved = false;
    long long time_ms = 0;
    vector<double> scenario_objectives;  // For anticipative solutions
};

/*--------------------------------------------------------------------------*/
/*---------------------------- CACHE FUNCTIONS -----------------------------*/
/*--------------------------------------------------------------------------*/

/**
 * @brief Structure to hold scenario reduction metrics
 */
struct ScenarioReductionMetrics {
    double wasserstein_distance = 0.0;
    double wasserstein_ell_power = 0.0;
    double ell = 2.0;
    long long reduction_time_ms = 0;
    vector<int> selected_indices;
};

/**
 * @brief Save scenarios to a netCDF cache file
 * 
 * @param filename Output cache file path
 * @param scenarios Vector of demand scenarios
 * @param probabilities Optional scenario probabilities (empty for uniform)
 * @param verbose Verbosity level
 * @param metrics Optional metrics to save (for reduced scenarios)
 */
void save_scenarios_to_file(const string& filename,
                           const vector<vector<double>>& scenarios,
                           const vector<double>& probabilities = {},
                           int verbose = 0,
                           const ScenarioReductionMetrics* metrics = nullptr) {
    if (verbose >= 2) cout << "  Saving scenarios to cache: " << filename << endl;
    
    // Create directory if it doesn't exist
    std::filesystem::path filepath(filename);
    std::filesystem::create_directories(filepath.parent_path());
    
    netCDF::NcFile file(filename, netCDF::NcFile::replace);
    
    // Add dimensions
    auto scenDim = file.addDim("NumberScenarios", scenarios.size());
    auto sizeDim = file.addDim("ScenarioSize", scenarios[0].size());
    
    // Add scenario data
    auto scenVar = file.addVar("Scenarios", netCDF::NcDouble(), {scenDim, sizeDim});
    for (size_t s = 0; s < scenarios.size(); ++s) {
        scenVar.putVar({s, 0}, {1, scenarios[0].size()}, scenarios[s].data());
    }
    
    // Add probabilities
    auto probVar = file.addVar("Probabilities", netCDF::NcDouble(), scenDim);
    if (!probabilities.empty() && probabilities.size() == scenarios.size()) {
        probVar.putVar(probabilities.data());
    } else {
        vector<double> uniform_probs(scenarios.size(), 1.0 / scenarios.size());
        probVar.putVar(uniform_probs.data());
    }
    
    // Add metrics if provided (for reduced scenarios)
    if (metrics != nullptr) {
        // Save scalar metrics as attributes
        file.putAtt("wasserstein_distance", netCDF::NcDouble(), metrics->wasserstein_distance);
        file.putAtt("wasserstein_ell_power", netCDF::NcDouble(), metrics->wasserstein_ell_power);
        file.putAtt("ell", netCDF::NcDouble(), metrics->ell);
        file.putAtt("reduction_time_ms", netCDF::NcInt64(), metrics->reduction_time_ms);
        
        // Save selected indices as a variable
        if (!metrics->selected_indices.empty()) {
            auto indDim = file.addDim("NumberSelectedIndices", metrics->selected_indices.size());
            auto indVar = file.addVar("SelectedIndices", netCDF::NcInt(), indDim);
            indVar.putVar(metrics->selected_indices.data());
        }
        
        if (verbose >= 2) {
            cout << "    Saved metrics: Wasserstein-" << metrics->ell << " distance = " 
                 << metrics->wasserstein_distance << endl;
        }
    }
    
    if (verbose >= 2) {
        cout << "    Saved " << scenarios.size() << " scenarios to cache" << endl;
    }
}

/**
 * @brief Load scenarios from a netCDF cache file
 * 
 * @param filename Input cache file path
 * @param verbose Verbosity level
 * @param metrics Optional pointer to store loaded metrics (if present)
 * @return Pair of (scenarios, probabilities)
 */
pair<vector<vector<double>>, vector<double>> load_scenarios_from_file(
    const string& filename,
    int verbose = 0,
    ScenarioReductionMetrics* metrics = nullptr) {
    
    if (verbose >= 2) cout << "  Loading scenarios from cache: " << filename << endl;
    
    if (!std::filesystem::exists(filename)) {
        throw runtime_error("Cache file not found: " + filename);
    }
    
    netCDF::NcFile file(filename, netCDF::NcFile::read);
    
    // Get dimensions
    auto scenDim = file.getDim("NumberScenarios");
    auto sizeDim = file.getDim("ScenarioSize");
    size_t num_scenarios = scenDim.getSize();
    size_t scenario_size = sizeDim.getSize();
    
    // Load scenarios
    auto scenVar = file.getVar("Scenarios");
    vector<vector<double>> scenarios(num_scenarios, vector<double>(scenario_size));
    for (size_t s = 0; s < num_scenarios; ++s) {
        scenVar.getVar({s, 0}, {1, scenario_size}, scenarios[s].data());
    }
    
    // Load probabilities
    auto probVar = file.getVar("Probabilities");
    vector<double> probabilities(num_scenarios);
    probVar.getVar(probabilities.data());
    
    // Load metrics if requested and available
    if (metrics != nullptr) {
        try {
            // Load scalar attributes
            file.getAtt("wasserstein_distance").getValues(&metrics->wasserstein_distance);
            file.getAtt("wasserstein_ell_power").getValues(&metrics->wasserstein_ell_power);
            file.getAtt("ell").getValues(&metrics->ell);
            file.getAtt("reduction_time_ms").getValues(&metrics->reduction_time_ms);
            
            // Load selected indices if present
            try {
                auto indVar = file.getVar("SelectedIndices");
                auto indDim = file.getDim("NumberSelectedIndices");
                size_t num_indices = indDim.getSize();
                metrics->selected_indices.resize(num_indices);
                indVar.getVar(metrics->selected_indices.data());
                
                if (verbose >= 2) {
                    cout << "    Loaded metrics: Wasserstein-" << metrics->ell << " distance = " 
                         << metrics->wasserstein_distance << endl;
                }
            } catch (...) {
                // Selected indices might not be present
            }
        } catch (...) {
            // Metrics not present in file (old cache format)
            if (verbose >= 2) {
                cout << "    No metrics found in cache (old format)" << endl;
            }
        }
    }
    
    if (verbose >= 2) {
        cout << "    Loaded " << num_scenarios << " scenarios from cache" << endl;
    }
    
    return {scenarios, probabilities};
}

/**
 * @brief Save SolutionResult to a netCDF cache file
 * 
 * @param filename Output cache file path
 * @param result SolutionResult to save
 * @param verbose Verbosity level
 */
void save_solution_result(const string& filename,
                         const SolutionResult& result,
                         int verbose = 0) {
    if (verbose >= 2) cout << "  Saving solution result to cache: " << filename << endl;
    
    // Create directory if it doesn't exist
    std::filesystem::path filepath(filename);
    std::filesystem::create_directories(filepath.parent_path());
    
    netCDF::NcFile file(filename, netCDF::NcFile::replace);
    
    // Save scalar values
    file.putAtt("objective", netCDF::NcDouble(), result.objective);
    file.putAtt("solved", netCDF::NcInt(), result.solved ? 1 : 0);
    file.putAtt("time_ms", netCDF::NcInt64(), result.time_ms);
    
    // Save scenario objectives if present
    if (!result.scenario_objectives.empty()) {
        auto dim = file.addDim("NumScenarios", result.scenario_objectives.size());
        auto var = file.addVar("ScenarioObjectives", netCDF::NcDouble(), dim);
        var.putVar(result.scenario_objectives.data());
    }
    
    if (verbose >= 2) {
        cout << "    Saved solution with objective=" << result.objective 
             << ", time=" << result.time_ms << "ms" << endl;
    }
}

/**
 * @brief Load SolutionResult from a netCDF cache file
 * 
 * @param filename Input cache file path
 * @param verbose Verbosity level
 * @return Loaded SolutionResult
 */
SolutionResult load_solution_result(const string& filename,
                                   int verbose = 0) {
    if (verbose >= 2) cout << "  Loading solution result from cache: " << filename << endl;
    
    if (!std::filesystem::exists(filename)) {
        throw runtime_error("Cache file not found: " + filename);
    }
    
    netCDF::NcFile file(filename, netCDF::NcFile::read);
    SolutionResult result;
    
    // Load scalar values
    file.getAtt("objective").getValues(&result.objective);
    int solved_int;
    file.getAtt("solved").getValues(&solved_int);
    result.solved = (solved_int != 0);
    file.getAtt("time_ms").getValues(&result.time_ms);
    
    // Load scenario objectives if present
    try {
        auto var = file.getVar("ScenarioObjectives");
        if (!var.isNull()) {
            auto dim = file.getDim("NumScenarios");
            size_t num_scenarios = dim.getSize();
            result.scenario_objectives.resize(num_scenarios);
            var.getVar(result.scenario_objectives.data());
        }
    } catch (...) {
        // No scenario objectives in file
    }
    
    if (verbose >= 2) {
        cout << "    Loaded solution with objective=" << result.objective 
             << ", time=" << result.time_ms << "ms" << endl;
    }
    
    return result;
}

/*--------------------------------------------------------------------------*/
/*------------------------------- HELPERS ----------------------------------*/
/*--------------------------------------------------------------------------*/

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
    int verbose = 0) {
    
    if (verbose >= 2) cout << "  Loading base CFL instance..." << endl;
    
    Block* block = Block::deserialize(path);
    auto* base_cfl = dynamic_cast<CapacitatedFacilityLocationBlock*>(block);
    
    if (!base_cfl) {
        delete block;
        throw runtime_error("Failed to load CFL instance from " + path);
    }
    
    int nf = base_cfl->get_NFacilities();
    int nc = base_cfl->get_NCustomers();
    if (verbose >= 1) cout << "  Loaded instance: " << nf << " facilities, " << nc << " customers" << endl;
    
    // Convert to single-sourcing if needed
    if (!base_cfl->get_UnSplittable()) {
        base_cfl->chg_UnSplittable(true);
        if (verbose >= 2) cout << "  Converted to single-sourcing" << endl;
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
 * Creates a set of demand scenarios organized in 4 distinct clusters to better
 * test scenario reduction algorithms. The clusters represent different market
 * conditions: normal, high demand, low demand, and regional variations.
 * 
 * @param original_demands The base demand values
 * @param num_scenarios Total number of scenarios to generate
 * @param variation_range Not used (kept for compatibility)
 * @param seed Random seed for reproducibility
 * @param verbose Whether to print detailed output
 * @return Vector of demand scenarios
 */
vector<vector<double>> generate_demand_scenarios(
    const vector<double>& original_demands,
    int num_scenarios,
    double variation_range = 0.2,
    unsigned int seed = 42,
    int verbose = 0) {
    
    if (verbose >= 2) cout << "  Creating stochastic demand scenarios in 4 clusters..." << endl;
    
    int nc = original_demands.size();
    vector<vector<double>> demand_scenarios;
    
    std::mt19937 gen(seed);  // Fixed seed for reproducibility
    
    // Determine scenarios per cluster
    int scenarios_per_cluster = num_scenarios / 4;
    int remaining = num_scenarios % 4;
    
    // Cluster 0: Around original (small variation ±10%)
    std::uniform_real_distribution<> cluster0_dist(0.9, 1.1);
    int cluster0_count = scenarios_per_cluster + (remaining > 0 ? 1 : 0);
    for (int s = 0; s < cluster0_count; ++s) {
        vector<double> scenario(nc);
        for (int i = 0; i < nc; ++i) {
            scenario[i] = original_demands[i] * cluster0_dist(gen);
        }
        demand_scenarios.push_back(scenario);
    }
    if (remaining > 0) remaining--;
    
    // Cluster 1: High demand (1.3x to 1.5x)
    std::uniform_real_distribution<> cluster1_dist(1.3, 1.5);
    int cluster1_count = scenarios_per_cluster + (remaining > 0 ? 1 : 0);
    for (int s = 0; s < cluster1_count; ++s) {
        vector<double> scenario(nc);
        for (int i = 0; i < nc; ++i) {
            scenario[i] = original_demands[i] * cluster1_dist(gen);
        }
        demand_scenarios.push_back(scenario);
    }
    if (remaining > 0) remaining--;
    
    // Cluster 2: Low demand (0.5x to 0.7x)
    std::uniform_real_distribution<> cluster2_dist(0.5, 0.7);
    int cluster2_count = scenarios_per_cluster + (remaining > 0 ? 1 : 0);
    for (int s = 0; s < cluster2_count; ++s) {
        vector<double> scenario(nc);
        for (int i = 0; i < nc; ++i) {
            scenario[i] = original_demands[i] * cluster2_dist(gen);
        }
        demand_scenarios.push_back(scenario);
    }
    if (remaining > 0) remaining--;
    
    // Cluster 3: Mixed/regional (half high, half low)
    std::uniform_real_distribution<> high_dist(1.2, 1.4);
    std::uniform_real_distribution<> low_dist(0.6, 0.8);
    int cluster3_count = scenarios_per_cluster + remaining;
    for (int s = 0; s < cluster3_count; ++s) {
        vector<double> scenario(nc);
        for (int i = 0; i < nc; ++i) {
            // Alternate regions: first half high, second half low
            if (i < nc / 2) {
                scenario[i] = original_demands[i] * high_dist(gen);
            } else {
                scenario[i] = original_demands[i] * low_dist(gen);
            }
        }
        demand_scenarios.push_back(scenario);
    }
    
    if (verbose >= 1) {
        cout << "  Generated " << num_scenarios << " scenarios in 4 distinct clusters:" << endl;
        cout << "    Cluster 0 (normal): " << cluster0_count << " scenarios around original (±10%)" << endl;
        cout << "    Cluster 1 (high): " << cluster1_count << " scenarios with high demand (1.3x-1.5x)" << endl;
        cout << "    Cluster 2 (low): " << cluster2_count << " scenarios with low demand (0.5x-0.7x)" << endl;
        cout << "    Cluster 3 (mixed): " << cluster3_count << " scenarios with regional variation" << endl;
    }
    
    return demand_scenarios;
}

/**
 * @brief Applies solver configuration and solves a block
 * 
 * Helper function that loads solver configuration, applies it to a block, and solves.
 * 
 * @param block The block to solve
 * @param time_limit Time limit in seconds (-1 to use default from config file)
 * @param verbose Whether to print detailed output
 * @return Pair of (objective value, success flag)
 */
pair<double, bool> solve_with_config(Block* block, double time_limit = -1, int verbose = 0) {
    auto cfg = Configuration::deserialize("BSPar_HiGHS.txt");
    auto* bsc = dynamic_cast<BlockSolverConfig*>(cfg);
    
    double obj = 0.0;
    bool success = false;
    
    if (bsc) {
        bsc->apply(block);
        
        if (!block->get_registered_solvers().empty()) {
            auto solver = block->get_registered_solvers().front();
            
            // If time limit is specified, override the configuration
            if (time_limit > 0) {
                solver->set_par(Solver::dblMaxTime, time_limit);
                if (verbose >= 2) {
                    cout << "  Setting solver time limit to " << time_limit << " seconds" << endl;
                }
            }
            
            int result = solver->compute(false);
            
            if (result == Solver::kOK) {
                obj = solver->get_ub();
                success = true;
            } else if (verbose >= 1) {
                // Provide meaningful error messages based on return code
                cerr << "Solver failed: ";
                switch(result) {
                    case Solver::kInfeasible:
                        cerr << "Problem is infeasible (no solution exists)" << endl;
                        break;
                    case Solver::kUnbounded:
                        cerr << "Problem is unbounded" << endl;
                        break;
                    case 11:  // kStopTime = kOK + 1
                        cerr << "Time limit reached (increase dblMaxTime in BSPar_HiGHS.txt)" << endl;
                        break;
                    case 12:  // kStopIter = kOK + 2
                        cerr << "Iteration limit reached" << endl;
                        break;
                    case Solver::kError:
                        cerr << "Unrecoverable error occurred" << endl;
                        break;
                    default:
                        cerr << "Unknown error (code " << result << ")" << endl;
                }
            }
            
            bsc->clear();
            bsc->apply(block);
        }
        delete bsc;
    }
    
    return {obj, success};
}

/**
 * @brief Computes the extensive form (stochastic) solution with timing
 * 
 * This function solves the extensive form of the stochastic problem using MILPSolver.
 * 
 * @param tss_block The TwoStageStochasticBlock to solve
 * @param num_scenarios Number of scenarios (for scaling the objective)
 * @param time_limit Time limit in seconds (-1 to use default)
 * @param verbose Whether to print detailed output
 * @return SolutionResult with objective, success flag, and timing
 */
SolutionResult compute_extensive_form_solution(
    TwoStageStochasticBlock* tss_block,
    int num_scenarios,
    double time_limit = -1,
    int verbose = 0) {
    
    if (verbose >= 2) cout << "  Solving the extensive form with MILPSolver..." << endl;
    
    auto start = chrono::high_resolution_clock::now();
    auto [obj, success] = solve_with_config(tss_block, time_limit, verbose);
    auto end = chrono::high_resolution_clock::now();
    
    SolutionResult result;
    result.time_ms = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    result.solved = success;
    
    if (success) {
        // The TwoStageStochasticBlock returns the sum of all scenario objectives
        // We always need to divide by the number of scenarios to get the expected value
        result.objective = obj / num_scenarios;
        
        if (verbose >= 1) {
            cout << "  Stochastic objective value (expected): " << fixed << setprecision(2) 
                 << result.objective << endl;
            cout << "  Solution time: " << result.time_ms << " ms" << endl;
        }
    } else if (verbose >= 1) {
        cerr << "Problem could not be solved" << endl;
    }
    
    return result;
}

/**
 * @brief Computes the anticipative (perfect information) solution with timing
 * 
 * This function solves each scenario independently as a deterministic problem
 * and returns the weighted average objective. It serves as the baseline for
 * comparing against stochastic solutions.
 * 
 * @param cfl_path Path to the base CFL instance netCDF file
 * @param demand_scenarios Vector of demand scenarios
 * @param nf Number of facilities
 * @param nc Number of customers
 * @param time_limit Time limit in seconds (-1 to use default)
 * @param verbose If true, prints detailed progress information
 * @return SolutionResult with average objective and individual scenario objectives
 */
SolutionResult compute_anticipative_solution(
    const string& cfl_path,
    const vector<vector<double>>& demand_scenarios,
    int nf, int nc, 
    double time_limit = -1,
    int verbose = 0) {
    
    SolutionResult result;
    result.solved = true;
    
    auto start = chrono::high_resolution_clock::now();
    
    double anticipative_obj = 0.0;
    int num_scenarios = demand_scenarios.size();
    
    for (int s = 0; s < num_scenarios; ++s) {
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
            
            // Solve this scenario
            auto [scenario_obj, solved] = solve_with_config(scenario_cfl, time_limit, verbose);
            
            if (solved) {
                result.scenario_objectives.push_back(scenario_obj);
                anticipative_obj += scenario_obj / num_scenarios; // Equal probabilities
                
                if (verbose >= 2) {
                    cout << "    Scenario " << s+1 << " objective: " 
                         << fixed << setprecision(2) << scenario_obj << endl;
                }
            } else {
                if (verbose >= 1) cerr << "    Failed to solve scenario " << s+1 << endl;
                result.scenario_objectives.push_back(0);
                result.solved = false;
            }
            delete scenario_cfl;
        }
    }
    
    auto end = chrono::high_resolution_clock::now();
    result.time_ms = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    result.objective = anticipative_obj;
    
    if (verbose >= 2) {
        cout << "  Total anticipative solution time: " << result.time_ms << " ms" << endl;
    }
    
    return result;
}

/**
 * @brief Prints comparison between different solution approaches
 * 
 * The Value of Perfect Information (VPI) represents the cost of uncertainty:
 * VPI = Stochastic Solution - Anticipative Solution
 * This should always be >= 0 since the stochastic solution must hedge against
 * uncertainty while the anticipative solution has perfect information.
 * 
 * @param stochastic_obj Objective from stochastic (here-and-now) solution
 * @param anticipative_obj Objective from anticipative (perfect information) solution
 * @param scenario_objectives Individual scenario objectives
 */
void print_solution_comparison(double stochastic_obj, double anticipative_obj,
                              const vector<double>& scenario_objectives) {
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
    
    double vpi = stochastic_obj - anticipative_obj;
    cout << "  Value of Perfect Information (VPI): " 
         << fixed << setprecision(2) << vpi << endl;
    
    if (vpi < 0) {
        cout << "  VPI is negative! This should not happen." << endl;
        cout << "      The stochastic solution should have higher cost than anticipative." << endl;
    } else if (vpi == 0) {
        cout << "  VPI is zero - the problem has no uncertainty impact (unusual)." << endl;
    } else {
        cout << "  VPI = " << vpi << " (cost of uncertainty, as expected)." << endl;
        cout << "  The stochastic solution costs " << (vpi/anticipative_obj * 100) 
             << "% more than perfect information." << endl;
    }
}

/**
 * @brief Updates the BSConfig_SR.txt file to use the specified algorithm
 * 
 * This helper function modifies the BSConfig_SR.txt file by commenting out
 * all intAlgorithm lines and uncommenting the one for the selected method.
 * It also sets the warmstart and shuffle parameters.
 * 
 * @param method The reduction method to use ("baseline", "dupacova", "bestfit", "firstfit")
 * @param use_warmstart Whether to enable warm start for local search
 * @param use_shuffle Whether to enable shuffling for FirstFit
 * @param verbose Whether to print detailed output
 * @return true if successful, false otherwise
 */
bool update_scenario_reduction_config(const string& method, 
                                     bool use_warmstart = false,
                                     bool use_shuffle = false,
                                     int verbose = 0) {
    // Determine which algorithm line to uncomment
    int algorithm = 1;  // Default to Dupacova
    if (method == "baseline") {
        // Baseline doesn't use the config, but set it anyway
        algorithm = 0;
    } else if (method == "dupacova") {
        algorithm = 1;
    } else if (method == "bestfit") {
        algorithm = 2;
    } else if (method == "firstfit") {
        algorithm = 3;
    } else {
        if (verbose >= 1) {
            cerr << "Unknown method: " << method << ", using Dupacova" << endl;
        }
        algorithm = 1;
    }
    
    // Read the original config file
    std::ifstream infile("BSConfig_SR.txt");
    if (!infile.is_open()) {
        if (verbose >= 1) {
            cerr << "Failed to open BSConfig_SR.txt for reading" << endl;
        }
        return false;
    }
    
    vector<string> lines;
    string line;
    while (getline(infile, line)) {
        lines.push_back(line);
    }
    infile.close();
    
    // Modify the lines: comment all intAlgorithm lines, uncomment the selected one
    // Also update warmstart and shuffle parameters
    bool found_algorithm_section = false;
    for (auto& l : lines) {
        // Check if this is an intAlgorithm line
        if (l.find("intAlgorithm") != string::npos && l.find("Algorithm selection") == string::npos) {
            found_algorithm_section = true;
            // Extract the algorithm value from the line
            size_t pos = l.find_first_of("0123");
            if (pos != string::npos) {
                int line_algorithm = l[pos] - '0';
                if (line_algorithm == algorithm) {
                    // Uncomment this line if it's commented
                    if (l[0] == '#') {
                        l = l.substr(2);  // Remove "# "
                    }
                } else {
                    // Comment this line if it's not commented
                    if (l[0] != '#') {
                        l = "# " + l;
                    }
                }
            }
        }
        // Update intUseWarmstart parameter
        else if (l.find("intUseWarmstart") != string::npos && l.find("intUseWarmstart") == 0) {
            // Find the value position (skip spaces after parameter name)
            size_t value_pos = l.find_first_of("01", l.find("intUseWarmstart") + strlen("intUseWarmstart"));
            if (value_pos != string::npos) {
                l[value_pos] = use_warmstart ? '1' : '0';
            }
        }
        // Update intShuffle parameter
        else if (l.find("intShuffle") != string::npos && l.find("intShuffle") == 0) {
            // Find the value position (skip spaces after parameter name)
            size_t value_pos = l.find_first_of("01", l.find("intShuffle") + strlen("intShuffle"));
            if (value_pos != string::npos) {
                l[value_pos] = use_shuffle ? '1' : '0';
            }
        }
    }
    
    if (!found_algorithm_section) {
        if (verbose >= 1) {
            cerr << "Failed to find intAlgorithm section in BSConfig_SR.txt" << endl;
        }
        return false;
    }
    
    // Write the modified config back
    std::ofstream outfile("BSConfig_SR.txt");
    if (!outfile.is_open()) {
        if (verbose >= 1) {
            cerr << "Failed to open BSConfig_SR.txt for writing" << endl;
        }
        return false;
    }
    
    for (const auto& l : lines) {
        outfile << l << endl;
    }
    outfile.close();
    
    if (verbose >= 2) {
        cout << "    Updated BSConfig_SR.txt to use algorithm " << algorithm 
             << " (" << method << ")"
             << ", warmstart=" << (use_warmstart ? "1" : "0")
             << ", shuffle=" << (use_shuffle ? "1" : "0") << endl;
    }
    
    return true;
}

/**
 * @brief Performs scenario reduction using DiscreteScenarioSet
 * 
 * Takes a set of demand scenarios and uses DiscreteScenarioSet's
 * init_representative_pool to select a representative subset.
 * 
 * @param all_scenarios Vector of all demand scenarios
 * @param target_size Number of representative scenarios to select
 * @param nc Number of customers
 * @param method Scenario reduction method to use ("baseline", "dupacova", "bestfit", "firstfit", "milp")
 * @param use_warmstart Whether to enable warm start for local search
 * @param use_shuffle Whether to enable shuffling for FirstFit
 * @param verbose Whether to print detailed output
 * @param reduction_time_ms Output parameter for reduction time in milliseconds
 * @param selected_indices Output parameter for the indices of selected scenarios
 * @return Pair of (selected scenarios, their adjusted probabilities)
 */
pair<vector<vector<double>>, vector<double>> perform_scenario_reduction(
    const vector<vector<double>>& all_scenarios,
    int target_size,
    int nc,
    const string& method = "dupacova",
    bool use_warmstart = false,
    bool use_shuffle = false,
    int verbose = 0,
    const string& cache_dir = "./cache/",
    long long* reduction_time_ms = nullptr,
    vector<int>* selected_indices = nullptr) {
    
    if (verbose >= 1) {
        cout << "  Performing scenario reduction from " << all_scenarios.size() 
             << " to " << target_size << " scenarios using " << method << " method..." << endl;
    }
    
    // Create a temporary netCDF file for DiscreteScenarioSet
    const string temp_dss_file = "temp_dss.nc4";
    
    try {
        // Create netCDF file with scenario data
        netCDF::NcFile file(temp_dss_file, netCDF::NcFile::replace);
        
        // Add dimensions
        auto scenDim = file.addDim("NumberScenarios", all_scenarios.size());
        auto sizeDim = file.addDim("ScenarioSize", nc);
        
        // Add scenario data (must be named "Scenarios" for DiscreteScenarioSet)
        auto scenVar = file.addVar("Scenarios", netCDF::NcDouble(), {scenDim, sizeDim});
        for (size_t s = 0; s < all_scenarios.size(); ++s) {
            scenVar.putVar({s, 0}, {1, static_cast<size_t>(nc)}, all_scenarios[s].data());
        }
        
        // Add uniform probabilities
        auto probVar = file.addVar("Probabilities", netCDF::NcDouble(), scenDim);
        vector<double> probs(all_scenarios.size(), 1.0 / all_scenarios.size());
        probVar.putVar(probs.data());
        
        file.close();
        
        // Now load and use DiscreteScenarioSet
        netCDF::NcFile inFile(temp_dss_file, netCDF::NcFile::read);
        DiscreteScenarioSet dss;
        dss.deserialize(inFile);
        
        // Perform scenario reduction and measure time
        auto reduction_start = chrono::high_resolution_clock::now();
        
        if (method == "baseline") {
            // Use default init_representative_pool (no config needed)
            dss.init_representative_pool(target_size);
            if (verbose >= 2) {
                cout << "    Using default init_representative_pool (baseline method)" << endl;
            }
        } else if (method == "milp" || method == "optimal") {
            // Use MILP solver for optimal scenario reduction
            
            // Load the MILP config
            auto cfg = Configuration::deserialize("BSConfig_SR_HiGHS.txt");
            auto* bsc = dynamic_cast<BlockSolverConfig*>(cfg);
            
            if (bsc) {
                if (verbose >= 2) {
                    cout << "    Using HiGHSMILPSolver for optimal scenario reduction" << endl;
                }
                
                // Update the intLogVerb parameter for MILP solver based on CLI verbose level
                if (verbose > 0 && !bsc->get_SolverConfigs().empty()) {
                    // Get the ComputeConfig for the first (and only) solver
                    auto* compute_config = bsc->get_SolverConfigs().front();
                    if (compute_config) {
                        // Add or update intLogVerb parameter
                        bool found = false;
                        for (auto& [name, value] : compute_config->int_pars) {
                            if (name == "intLogVerb") {
                                value = verbose;
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            compute_config->int_pars.emplace_back("intLogVerb", verbose);
                        }
                        if (verbose >= 2) {
                            cout << "    Set intLogVerb to " << verbose << " in MILP config" << endl;
                        }
                    }
                }
                
                // Set the configuration
                dss.set_config(nullptr, bsc);
                // Then call init_representative_pool
                dss.init_representative_pool(target_size);
                // Note: Don't delete bsc here - ownership transferred to dss
            } else {
                cerr << "Failed to load BSConfig_SR_HiGHS.txt, using default" << endl;
                dss.init_representative_pool(target_size);
            }
        } else {
            // For other methods (dupacova, bestfit, firstfit), use ScenarioReductionSolver
            
            // Update the config file to use the selected method
            if (!update_scenario_reduction_config(method, use_warmstart, use_shuffle, verbose)) {
                cerr << "Failed to update BSConfig_SR.txt, falling back to default" << endl;
                dss.init_representative_pool(target_size);
            } else {
                // Load the updated config
                auto cfg = Configuration::deserialize("BSConfig_SR.txt");
                auto* bsc = dynamic_cast<BlockSolverConfig*>(cfg);
                
                if (bsc) {
                    if (verbose >= 2) {
                        cout << "    Using ScenarioReductionSolver with " << method << " algorithm" << endl;
                    }
                    
                    // Update the intLogVerb parameter based on CLI verbose level
                    // The BlockSolverConfig should apply this to the ScenarioReductionSolver
                    if (verbose > 0 && !bsc->get_SolverConfigs().empty()) {
                        // Get the ComputeConfig for the first (and only) solver
                        auto* compute_config = bsc->get_SolverConfigs().front();
                        if (compute_config) {
                            // Add or update intLogVerb parameter
                            bool found = false;
                            for (auto& [name, value] : compute_config->int_pars) {
                                if (name == "intLogVerb") {
                                    value = verbose;
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                compute_config->int_pars.emplace_back("intLogVerb", verbose);
                            }
                            
                            // Set log file name for ScenarioReductionSolver output
                            // Using cache directory for consistency
                            string log_filename = cache_dir + "scenario_reduction_" + method + ".log";
                            found = false;
                            for (auto& [name, value] : compute_config->str_pars) {
                                if (name == "strLogFileName") {
                                    value = log_filename;
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                compute_config->str_pars.emplace_back("strLogFileName", log_filename);
                            }
                            
                            if (verbose >= 2) {
                                cout << "    Set intLogVerb to " << verbose << " in config" << endl;
                                cout << "    Solver log will be written to: " << log_filename << endl;
                            }
                        }
                    }
                    
                    // Set the configuration
                    dss.set_config(nullptr, bsc);
                    // Then call init_representative_pool
                    dss.init_representative_pool(target_size);
                    // Note: Don't delete bsc here - ownership transferred to dss
                } else {
                    cerr << "Failed to load BSConfig_SR.txt, using default" << endl;
                    dss.init_representative_pool(target_size);
                }
            }
        }
        
        auto reduction_end = chrono::high_resolution_clock::now();
        auto reduction_time = chrono::duration_cast<chrono::milliseconds>(reduction_end - reduction_start).count();
        
        if (reduction_time_ms) {
            *reduction_time_ms = reduction_time;
        }
        
        // Get the indices of selected scenarios
        auto selected_indices_span = dss.get_selected_scenarios();
        
        // Calculate Wasserstein distance between original and reduced distributions
        // Note: The scenario reduction algorithms minimize the ell-th power of Wasserstein distance
        // We need to compute this after reduction to report it
        double ell = dss.get_ell();  // Get the ell parameter (default is 2.0)
        
        // Compute the ell-th power of Wasserstein distance
        // This is the sum of (minimum distance to selected set)^ell weighted by probabilities
        vector<double> min_distances(all_scenarios.size());
        vector<int> selected_idx;
        for (auto idx : selected_indices_span) {
            selected_idx.push_back(static_cast<int>(idx));
        }
        
        for (size_t i = 0; i < all_scenarios.size(); ++i) {
            double min_dist = std::numeric_limits<double>::infinity();
            for (int j : selected_idx) {
                double dist = 0.0;
                for (size_t k = 0; k < nc; ++k) {
                    double diff = all_scenarios[i][k] - all_scenarios[j][k];
                    dist += diff * diff;  // Euclidean distance squared
                }
                dist = std::sqrt(dist);  // Euclidean distance
                dist = std::pow(dist, ell);  // ell-th power
                min_dist = std::min(min_dist, dist);
            }
            min_distances[i] = min_dist;
        }
        
        // Uniform weights for scenarios
        double weight = 1.0 / all_scenarios.size();
        double wasserstein_ell_power = 0.0;
        for (double d : min_distances) {
            wasserstein_ell_power += weight * d;
        }
        
        // Compute actual Wasserstein distance (ell-th root)
        double wasserstein_distance = std::pow(wasserstein_ell_power, 1.0 / ell);
        
        if (verbose >= 1) {
            cout << "  Scenario reduction time: " << reduction_time << " ms" << endl;
            cout << "  Wasserstein-" << ell << " distance: " << wasserstein_distance << endl;
            cout << "  Wasserstein distance (ell-th power): " << wasserstein_ell_power << endl;
        }
        
        // Store indices if requested
        if (selected_indices) {
            selected_indices->clear();
            for (auto idx : selected_indices_span) {
                selected_indices->push_back(static_cast<int>(idx));
            }
            // Sort for consistency
            std::sort(selected_indices->begin(), selected_indices->end());
        }
        
        if (verbose >= 2) {
            cout << "    Selected scenario indices from original pool: ";
            for (size_t i = 0; i < selected_indices_span.size(); ++i) {
                cout << selected_indices_span[i];
                if (i < selected_indices_span.size() - 1) cout << ", ";
            }
            cout << endl;
        }
        
        // Extract the selected scenarios
        vector<vector<double>> selected_scenarios;
        vector<double> adjusted_probabilities;
        
        // Iterate through the representative pool
        // After init_representative_pool(), we're positioned before the first scenario
        // so we need to get it first, then call next_scenario() for subsequent ones
        int scenario_num = 0;
        do {
            auto scenario_span = dss.get_current_scenario();
            vector<double> scenario_vec(scenario_span.begin(), scenario_span.end());
            selected_scenarios.push_back(scenario_vec);
            adjusted_probabilities.push_back(dss.get_current_scenario_probability());
            
            if (verbose >= 2) {
                cout << "    Representative scenario " << scenario_num+1 
                     << " (original index " << selected_indices_span[scenario_num] << ")"
                     << " with probability " << adjusted_probabilities.back();
                // Print sample demand values for verification
                if (scenario_vec.size() >= 3) {
                    cout << " [sample demands: ";
                    // Print a few demands from different parts of the vector
                    cout << scenario_vec[0] << ", " 
                         << scenario_vec[scenario_vec.size()/2] << ", " 
                         << scenario_vec[scenario_vec.size()-1] << "]";
                }
                cout << endl;
            }
            scenario_num++;
        } while (scenario_num < target_size && dss.next_scenario());
        
        // Cleanup
        inFile.close();
        remove(temp_dss_file.c_str());
        
        // Display solver log if it exists and verbose is enabled
        if (verbose >= 1) {
            string log_filename = cache_dir + "scenario_reduction_" + method + ".log";
            std::ifstream log_file(log_filename);
            if (log_file.is_open()) {
                if (verbose >= 2) {
                    cout << "\n    === ScenarioReductionSolver Log ===" << endl;
                }
                string line;
                while (getline(log_file, line)) {
                    cout << "    [Solver] " << line << endl;
                }
                log_file.close();
                if (verbose >= 2) {
                    cout << "    === End of Solver Log ===\n" << endl;
                }
            }
        }
        
        return {selected_scenarios, adjusted_probabilities};
        
    } catch (const exception& e) {
        // Cleanup on error
        remove(temp_dss_file.c_str());
        throw;
    }
}

/**
 * @brief Print scenario reduction results comparison
 * 
 * @param full_result Results from full scenario set
 * @param reduced_result Results from reduced scenario set
 * @param anticipative_full Anticipative solution for full scenario set
 * @param anticipative_reduced Anticipative solution for reduced scenario set
 * @param full_num_scenarios Number of scenarios in full set
 * @param reduced_num_scenarios Number of scenarios in reduced set
 * @param scenario_reduction_time_ms Time taken for scenario reduction in milliseconds
 * @param selected_indices Indices of selected scenarios (sorted)
 */
void print_scenario_reduction_results(
    const SolutionResult& full_result,
    const SolutionResult& reduced_result,
    const SolutionResult& anticipative_full,
    const SolutionResult& anticipative_reduced,
    int full_num_scenarios,
    int reduced_num_scenarios,
    long long scenario_reduction_time_ms = 0,
    const vector<int>& selected_indices = {}) {
    
    cout << "\nScenario Reduction Results:" << endl;
    
    // Scenario reduction time
    if (scenario_reduction_time_ms > 0) {
        cout << "  Scenario reduction time: " << scenario_reduction_time_ms << " ms" << endl;
    }
    
    // Selected scenario indices (first 5, sorted)
    if (!selected_indices.empty()) {
        cout << "  Selected scenarios (first 5, sorted): ";
        for (size_t i = 0; i < min(size_t(5), selected_indices.size()); ++i) {
            cout << selected_indices[i];
            if (i < min(size_t(5), selected_indices.size()) - 1) cout << ", ";
        }
        if (selected_indices.size() > 5) {
            cout << ", ...";
        }
        cout << endl;
    }
    
    // Full problem results
    cout << "  Full problem (" << full_num_scenarios << " scenarios):" << endl;
    if (full_result.solved) {
        cout << "    Objective: " << fixed << setprecision(2) << full_result.objective << endl;
        cout << "    Solution time: " << full_result.time_ms << " ms" << endl;
    } else {
        cout << "    Failed to solve" << endl;
    }
    
    // Reduced problem results
    cout << "  Reduced problem (" << reduced_num_scenarios << " scenarios):" << endl;
    if (reduced_result.solved) {
        cout << "    Objective: " << fixed << setprecision(2) << reduced_result.objective << endl;
        cout << "    Solution time: " << reduced_result.time_ms << " ms" << endl;
    } else {
        cout << "    Failed to solve" << endl;
    }
    
    // Performance metrics
    if (full_result.solved && reduced_result.solved) {
        double approximation_error = abs(full_result.objective - reduced_result.objective);
        double relative_error = approximation_error / full_result.objective * 100;
        double speedup = static_cast<double>(full_result.time_ms) / reduced_result.time_ms;
        
        cout << "\n  Scenario reduction performance:" << endl;
        cout << "    Approximation error: " << fixed << setprecision(2) << approximation_error 
             << " (" << relative_error << "%)" << endl;
        cout << "    Speedup: " << fixed << setprecision(1) << speedup << "x" << endl;
        cout << "    Scenarios reduced: " << full_num_scenarios << " → " << reduced_num_scenarios 
             << " (" << (100.0 * reduced_num_scenarios / full_num_scenarios) << "% retained)" << endl;
    }
    
    // Value of Perfect Information comparison (only if computed)
    if (anticipative_full.solved || anticipative_reduced.solved) {
        cout << "\n  Comparison with perfect information:" << endl;
        
        if (anticipative_full.solved) {
            cout << "    Full problem anticipative (" << full_num_scenarios << " scenarios): " 
                 << fixed << setprecision(2) << anticipative_full.objective << endl;
        }
        
        if (anticipative_reduced.solved) {
            cout << "    Reduced problem anticipative (" << reduced_num_scenarios << " scenarios): " 
                 << fixed << setprecision(2) << anticipative_reduced.objective << endl;
        }
        
        if (full_result.solved && anticipative_full.solved) {
            double vpi_full = full_result.objective - anticipative_full.objective;
            cout << "    VPI (full, " << full_num_scenarios << " vs " << full_num_scenarios << "): " 
                 << fixed << setprecision(2) << vpi_full 
                 << " (" << (vpi_full/anticipative_full.objective * 100) << "% cost of uncertainty)" << endl;
        }
        
        if (reduced_result.solved && anticipative_reduced.solved) {
            double vpi_reduced = reduced_result.objective - anticipative_reduced.objective;
            cout << "    VPI (reduced, " << reduced_num_scenarios << " vs " << reduced_num_scenarios << "): " 
                 << fixed << setprecision(2) << vpi_reduced 
                 << " (" << (vpi_reduced/anticipative_reduced.objective * 100) << "% cost of uncertainty)" << endl;
        }
    }
}

/**
 * @brief Apply scenario data to TwoStageStochasticBlock
 * 
 * @param tss_block The TwoStageStochasticBlock containing the scenario blocks
 * @param demand_scenarios Vector of demand scenarios
 * @param nc Number of customers
 * @param verbose If true, prints detailed progress information
 */
void apply_scenario_data_programmatically(
    TwoStageStochasticBlock* tss_block,
    const vector<vector<double>>& demand_scenarios,
    int nc, int verbose = 0) {
    
    if (verbose >= 2) cout << "  Applying scenario data..." << endl;
    
    try {
        // Try using the apply_scenario_data() method
        for (size_t i = 0; i < demand_scenarios.size() && i < tss_block->get_number_scenarios(); ++i) {
            tss_block->apply_scenario_data(i, demand_scenarios[i], eNoBlck, eNoBlck);
        }
        
        if (verbose >= 2) cout << "  Successfully applied all scenario data" << endl;
        
    } catch (const exception& e) {
        // Fall back to manual application
        if (verbose >= 2) {
            cout << "  Falling back to manual scenario application..." << endl;
        }
        
        using FunctionType = Block::FunctionType<Block::MF_dbl_it, Block::Range>;
        auto method = Block::get_method<FunctionType>(
            "CapacitatedFacilityLocationBlock::chg_customer_demands");
        
        if (!method) {
            throw runtime_error("Failed to get method for chg_customer_demands");
        }
        
        for (size_t i = 0; i < demand_scenarios.size() && i < tss_block->get_number_scenarios(); ++i) {
            auto* scenario_block = tss_block->get_nested_Block(i);
            
            if (auto* cfl_block = dynamic_cast<CapacitatedFacilityLocationBlock*>(scenario_block)) {
                auto demands_it = demand_scenarios[i].begin();
                (*method)(cfl_block, demands_it, Block::Range(0, nc), eNoBlck, eNoBlck);
                if (verbose >= 2) cout << "    Applied scenario " << i << " manually" << endl;
            }
        }
        
        if (verbose >= 2) cout << "  Successfully applied all scenario data manually" << endl;
    }
}

/**
 * @brief Creates a netCDF file for TwoStageStochasticBlock with proper structure
 * 
 * @param filename Output netCDF file path
 * @param cfl_path Path to the base CFL instance netCDF file
 * @param demand_scenarios Vector of demand scenarios
 * @param nf Number of facilities
 * @param nc Number of customers
 * @param scenario_probs Optional scenario probabilities (if empty, uses uniform)
 * @param verbose If true, prints detailed progress information
 */
void create_twostage_netcdf(const string& filename,
                            const string& cfl_path,
                            const vector<vector<double>>& demand_scenarios,
                            int nf, int nc, 
                            const vector<double>& scenario_probs = {},
                            int verbose = 0) {
    if (verbose >= 2) cout << "  Creating netCDF file for TwoStageStochasticBlock..." << endl;
    
    // Load and prepare base CFL
    Block* base_block = Block::deserialize(cfl_path);
    auto* base_cfl = dynamic_cast<CapacitatedFacilityLocationBlock*>(base_block);
    if (!base_cfl) {
        throw runtime_error("Failed to load base CFL for serialization");
    }
    
    // Ensure single-sourcing
    if (!base_cfl->get_UnSplittable()) {
        base_cfl->chg_UnSplittable(true);
        if (verbose >= 2) cout << "    Converted CFL to single-sourcing" << endl;
    }
    
    // Create the netCDF file
    netCDF::NcFile file(filename, netCDF::NcFile::replace);
    auto root = file.addGroup("TwoStageStochasticBlock");
    root.putAtt("type", "TwoStageStochasticBlock");
    root.addDim("NumberScenarios", demand_scenarios.size());
    
    // Create StochasticBlock group
    auto stochGroup = root.addGroup("StochasticBlock");
    stochGroup.putAtt("type", "StochasticBlock");
    
    // Serialize the CFL
    auto blockGroup = stochGroup.addGroup("Block");
    base_cfl->serialize(blockGroup);
    
    // Add minimal DataMapping structure (required for deserialization)
    auto numDataMappings = stochGroup.addDim("NumberDataMappings", 1);
    char dataType = 'D';
    stochGroup.addVar("DataType", netCDF::NcChar(), numDataMappings).putVar(&dataType);
    char caller = 'B';
    stochGroup.addVar("Caller", netCDF::NcChar(), numDataMappings).putVar(&caller);
    
    string functionName = "CapacitatedFacilityLocationBlock::chg_customer_demands";
    stochGroup.addVar("FunctionName", netCDF::NcString(), numDataMappings).putVar({0}, &functionName);
    
    auto setSizeDim = stochGroup.addDim("SetSizeDim", 2);
    vector<unsigned int> setSize = {0, 0};
    stochGroup.addVar("SetSize", netCDF::NcUint(), setSizeDim).putVar(setSize.data());
    
    unsigned char ordered = 0;
    stochGroup.addVar("Ordered", netCDF::NcUbyte(), numDataMappings).putVar(&ordered);
    
    auto setElementsDim = stochGroup.addDim("SetElementsDim", 4);
    vector<unsigned int> setElements = {0, static_cast<unsigned int>(nc), 0, static_cast<unsigned int>(nc)};
    stochGroup.addVar("SetElements", netCDF::NcUint(), setElementsDim).putVar(setElements.data());
    
    // Add AbstractPath for DataMapping
    auto abstractPathGroup = stochGroup.addGroup("AbstractPath");
    abstractPathGroup.addDim("PathDim", 1);
    auto pathTotalLength = abstractPathGroup.addDim("PathTotalLength", 0);
    unsigned int pathStart = 0;
    abstractPathGroup.addVar("PathStart", netCDF::NcUint(), abstractPathGroup.getDim("PathDim")).putVar(&pathStart);
    abstractPathGroup.addVar("PathNodeTypes", netCDF::NcChar(), pathTotalLength);
    abstractPathGroup.addVar("PathGroupIndices", netCDF::NcUint(), pathTotalLength);
    abstractPathGroup.addVar("PathElementIndices", netCDF::NcUint(), pathTotalLength);
    abstractPathGroup.addVar("PathRangeIndices", netCDF::NcUint(), pathTotalLength);
    
    // Create StaticAbstractPath for first-stage variables
    auto staticPathGroup = root.addGroup("StaticAbstractPath");
    auto staticPathDim = staticPathGroup.addDim("PathDim", nf);
    auto staticTotalLengthDim = staticPathGroup.addDim("PathTotalLength", nf);
    
    vector<unsigned int> indices(nf);
    for (int i = 0; i < nf; ++i) indices[i] = i;
    
    staticPathGroup.addVar("PathStart", netCDF::NcUint(), staticPathDim).putVar(indices.data());
    staticPathGroup.addVar("PathNodeTypes", netCDF::NcChar(), staticTotalLengthDim).putVar(vector<char>(nf, 'V').data());
    staticPathGroup.addVar("PathGroupIndices", netCDF::NcUint(), staticTotalLengthDim).putVar(vector<unsigned int>(nf, 0).data());
    staticPathGroup.addVar("PathElementIndices", netCDF::NcUint(), staticTotalLengthDim).putVar(indices.data());
    
    vector<unsigned int> rangeIndices(nf);
    for (int i = 0; i < nf; ++i) rangeIndices[i] = i + 1;
    staticPathGroup.addVar("PathRangeIndices", netCDF::NcUint(), staticTotalLengthDim).putVar(rangeIndices.data());
    
    // Store scenario data
    auto scenarioGenGroup = root.addGroup("ScenarioGenerator");
    scenarioGenGroup.putAtt("type", "DiscreteScenarioSet");
    
    auto scenarioDim = scenarioGenGroup.addDim("NumberScenarios", demand_scenarios.size());
    auto scenarioSizeDim = scenarioGenGroup.addDim("ScenarioSize", nc);
    auto scenarioDataVar = scenarioGenGroup.addVar("ScenarioData", netCDF::NcDouble(), {scenarioDim, scenarioSizeDim});
    
    for (size_t s = 0; s < demand_scenarios.size(); ++s) {
        scenarioDataVar.putVar({s, 0}, {1, static_cast<size_t>(nc)}, demand_scenarios[s].data());
    }
    
    // Use provided probabilities or uniform if not provided
    vector<double> probs;
    if (!scenario_probs.empty() && scenario_probs.size() == demand_scenarios.size()) {
        probs = scenario_probs;
        if (verbose >= 2) {
            cout << "    Using provided probabilities (sum=" 
                 << accumulate(probs.begin(), probs.end(), 0.0) << ")" << endl;
        }
    } else {
        probs.assign(demand_scenarios.size(), 1.0 / demand_scenarios.size());
        if (verbose >= 2) {
            cout << "    Using uniform probabilities" << endl;
        }
    }
    scenarioGenGroup.addVar("Probabilities", netCDF::NcDouble(), scenarioDim).putVar(probs.data());
    
    if (verbose >= 2) {
        cout << "    Created netCDF with " << demand_scenarios.size() << " scenarios" << endl;
    }
    
    delete base_cfl;
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
 * - Demonstrates the Value of Perfect Information (VPI)
 * 
 * Command line options:
 * - --verbose=<level>: Set verbosity level (0=silent, 1=normal, 2=detailed)
 * - -v: Set verbose level to 1, -vv: Set verbose level to 2
 * - -time=<seconds>: Set solver time limit (e.g., -time=300 for 300 seconds)
 * - -n_scen=<number>: Set total number of scenarios (default: 20)
 * - -n_reduced=<number>: Set number of reduced scenarios (default: 3)
 * - -method=<name>: Scenario reduction method (baseline, dupacova (default), bestfit, firstfit, milp)
 * - --save-cache: Save scenarios and results to cache files
 * - --load-cache: Load scenarios from cache files
 * - --load-results: Load pre-computed results from cache
 * - --cache-dir=<path>: Specify cache directory (default: ./cache/)
 * - --compute-vpi: Compute Value of Perfect Information (anticipative solutions)
 * 
 * @param argc Number of command-line arguments
 * @param argv Command-line arguments
 * @return 0 on success, 1 on failure
 */
int main(int argc, char* argv[]) {
    // Parse command line arguments
    int verbose = 0;  // 0=silent, 1=normal, 2=detailed
    double time_limit = -1;  // -1 means use default from BSPar_HiGHS.txt
    int full_num_scenarios = 20;  // Default: 20 scenarios
    int reduced_num_scenarios = 3;  // Default: reduce to 3 scenarios
    string reduction_method = "dupacova";  // Default: Dupacova algorithm
    bool use_warmstart = false;  // Default: no warm start
    bool use_shuffle = false;    // Default: no shuffling
    bool save_cache = false;
    bool load_cache = false;
    bool load_results = false;
    bool compute_vpi = false;
    string cache_dir = "./cache/";
    
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-v") {
            verbose = 1;
        } else if (arg == "-vv") {
            verbose = 2;
        } else if (arg.find("--verbose=") == 0) {
            try {
                verbose = stoi(arg.substr(10));
                if (verbose < 0 || verbose > 2) {
                    cerr << "Verbose level must be 0, 1, or 2: " << arg << endl;
                    return 1;
                }
            } catch (const exception& e) {
                cerr << "Invalid verbose level: " << arg << endl;
                return 1;
            }
        } else if (arg.find("-time=") == 0) {
            // Parse time limit: -time=300 means 300 seconds
            try {
                time_limit = stod(arg.substr(6));
                if (time_limit <= 0) {
                    cerr << "Time limit must be positive: " << arg << endl;
                    return 1;
                }
            } catch (const exception& e) {
                cerr << "Invalid time limit format: " << arg << " (use -time=<seconds>, e.g., -time=300)" << endl;
                return 1;
            }
        } else if (arg.find("-n_scen=") == 0) {
            // Parse number of scenarios: -n_scen=10
            try {
                full_num_scenarios = stoi(arg.substr(8));
                if (full_num_scenarios <= 0) {
                    cerr << "Number of scenarios must be positive: " << arg << endl;
                    return 1;
                }
            } catch (const exception& e) {
                cerr << "Invalid number of scenarios format: " << arg << " (use -n_scen=<number>, e.g., -n_scen=10)" << endl;
                return 1;
            }
        } else if (arg.find("-n_reduced=") == 0) {
            // Parse number of reduced scenarios: -n_reduced=3
            try {
                reduced_num_scenarios = stoi(arg.substr(11));
                if (reduced_num_scenarios <= 0) {
                    cerr << "Number of reduced scenarios must be positive: " << arg << endl;
                    return 1;
                }
            } catch (const exception& e) {
                cerr << "Invalid reduced scenarios format: " << arg << " (use -n_reduced=<number>, e.g., -n_reduced=3)" << endl;
                return 1;
            }
        } else if (arg.find("-method=") == 0) {
            // Parse scenario reduction method: -method=dupacova
            reduction_method = arg.substr(8);
            // Validate method name
            if (reduction_method != "baseline" && reduction_method != "dupacova" && 
                reduction_method != "bestfit" && reduction_method != "firstfit" &&
                reduction_method != "milp" && reduction_method != "optimal") {
                cerr << "Invalid method: " << reduction_method 
                     << " (use baseline, dupacova, bestfit, firstfit, or milp)" << endl;
                return 1;
            }
        } else if (arg == "--save-cache") {
            save_cache = true;
        } else if (arg == "--load-cache") {
            load_cache = true;
        } else if (arg == "--load-results") {
            load_results = true;
        } else if (arg.find("--cache-dir=") == 0) {
            cache_dir = arg.substr(12);
            // Ensure cache_dir ends with slash
            if (!cache_dir.empty() && cache_dir.back() != '/') {
                cache_dir += '/';
            }
        } else if (arg == "--compute-vpi") {
            compute_vpi = true;
        } else if (arg.find("-warmstart=") == 0) {
            // Parse warmstart flag: -warmstart=1 or -warmstart=0
            try {
                use_warmstart = (stoi(arg.substr(11)) != 0);
            } catch (const exception& e) {
                cerr << "Invalid warmstart value: " << arg << " (use -warmstart=0 or -warmstart=1)" << endl;
                return 1;
            }
        } else if (arg.find("-shuffle=") == 0) {
            // Parse shuffle flag: -shuffle=1 or -shuffle=0
            try {
                use_shuffle = (stoi(arg.substr(9)) != 0);
            } catch (const exception& e) {
                cerr << "Invalid shuffle value: " << arg << " (use -shuffle=0 or -shuffle=1)" << endl;
                return 1;
            }
        }
    }
    
    // Validate scenario counts
    if (reduced_num_scenarios > full_num_scenarios) {
        cerr << "Error: Number of reduced scenarios (" << reduced_num_scenarios 
             << ") cannot exceed total scenarios (" << full_num_scenarios << ")" << endl;
        return 1;
    }
    
    if (verbose >= 1) {
        cout << "Configuration:" << endl;
        cout << "  Full scenarios: " << full_num_scenarios << endl;
        cout << "  Reduced scenarios: " << reduced_num_scenarios << endl;
        cout << "  Reduction method: " << reduction_method << endl;
        cout << "  Verbose level: " << verbose << endl;
        if (time_limit > 0) {
            cout << "  Time limit: " << time_limit << " seconds" << endl;
        }
        if (save_cache || load_cache || load_results) {
            cout << "  Cache directory: " << cache_dir << endl;
            if (save_cache) cout << "  Saving to cache: enabled" << endl;
            if (load_cache) cout << "  Loading scenarios from cache: enabled" << endl;
            if (load_results) cout << "  Loading results from cache: enabled" << endl;
        }
        if (compute_vpi) {
            cout << "  Compute VPI: enabled" << endl;
        }
    }
    
    if (verbose >= 2) {
        cout << "=== Two-Stage Stochastic CFL Test ===" << endl;
    } else if (verbose >= 1) {
        cout << "Running Two-Stage Stochastic CFL Test..." << endl;
    }
    
    try {
        // Step 1: Load base CFL instance to get dimensions and demands
        if (verbose >= 1) cout << "\n1. Loading base CFL instance:" << endl;
        const string path = "../../CapacitatedFacilityLocationBlock/data/nc4/Yang/30-200/30-200-1.nc4";
        auto [nf, nc, original_demands] = load_cfl_instance(path, verbose);
        
        // Read warmstart and shuffle settings from BSConfig_SR.txt for cache naming
        int warmstart_setting = 0;
        int shuffle_setting = 0;
        {
            std::ifstream config_file("BSConfig_SR.txt");
            if (config_file.is_open()) {
                string line;
                while (getline(config_file, line)) {
                    // Look for intUseWarmstart line
                    if (line.find("intUseWarmstart") != string::npos && line[0] != '#') {
                        istringstream iss(line);
                        string param_name;
                        iss >> param_name >> warmstart_setting;
                    }
                    // Look for intShuffle line
                    if (line.find("intShuffle") != string::npos && line[0] != '#') {
                        istringstream iss(line);
                        string param_name;
                        iss >> param_name >> shuffle_setting;
                    }
                }
                config_file.close();
            }
        }
        
        // Build cache filename suffix based on method and settings
        string method_suffix = reduction_method;
        if (reduction_method == "firstfit" || reduction_method == "bestfit") {
            if (warmstart_setting == 1) method_suffix += "_warm";
            if (shuffle_setting == 1) method_suffix += "_shuf";
        }
        
        // Cache file names
        string scenarios_cache = cache_dir + "scenarios_" + to_string(full_num_scenarios) + ".nc4";
        string reduced_cache = cache_dir + "scenarios_" + to_string(full_num_scenarios) + 
                              "_reduced_" + to_string(reduced_num_scenarios) + "_" + method_suffix + ".nc4";
        
        vector<vector<double>> all_demand_scenarios;
        vector<vector<double>> reduced_scenarios;
        vector<double> reduced_probs;
        bool scenarios_loaded_from_cache = false;
        
        // Step 2: Create or load stochastic demand scenarios
        // Try to load from cache first if enabled
        if (load_cache || load_results) {
            try {
                if (verbose >= 1) cout << "\n2. Loading stochastic demand scenarios from cache:" << endl;
                auto [loaded_scenarios, loaded_probs] = load_scenarios_from_file(scenarios_cache, verbose);
                all_demand_scenarios = loaded_scenarios;
                scenarios_loaded_from_cache = true;
                
                // Also try to load reduced scenarios if they exist
                try {
                    ScenarioReductionMetrics temp_metrics;
                    auto [loaded_reduced, loaded_reduced_probs] = load_scenarios_from_file(reduced_cache, verbose, &temp_metrics);
                    reduced_scenarios = loaded_reduced;
                    reduced_probs = loaded_reduced_probs;
                    if (verbose >= 1) cout << "  Loaded reduced scenarios from cache" << endl;
                    // Note: metrics will be properly loaded later in the scenario reduction section
                } catch (const exception& e) {
                    // Reduced scenarios not cached, will compute below
                    if (verbose >= 1) cout << "  Reduced scenarios not in cache, will compute" << endl;
                }
            } catch (const exception& e) {
                // Base scenarios not in cache, will generate below
                if (verbose >= 1) {
                    cout << "  Base scenarios not in cache, will generate..." << endl;
                }
            }
        }
        
        // Generate scenarios if not loaded from cache
        if (!scenarios_loaded_from_cache) {
            if (verbose >= 1) cout << "\n2. Creating stochastic demand scenarios:" << endl;
            all_demand_scenarios = generate_demand_scenarios(
                original_demands, full_num_scenarios, 0.2, 42, verbose);
            
            // Save the generated scenarios if caching is enabled
            if (save_cache) {
                save_scenarios_to_file(scenarios_cache, all_demand_scenarios, {}, verbose);
            }
        }
        
        // Step 3: Perform scenario reduction if not already loaded
        long long reduction_time_ms = 0;
        vector<int> selected_scenario_indices;
        ScenarioReductionMetrics reduction_metrics;
        
        if (reduced_scenarios.empty()) {
            if (verbose >= 1) cout << "\n3. Performing scenario reduction:" << endl;
            auto reduction_result = perform_scenario_reduction(all_demand_scenarios, reduced_num_scenarios, nc, reduction_method, use_warmstart, use_shuffle, verbose, cache_dir, &reduction_time_ms, &selected_scenario_indices);
            reduced_scenarios = reduction_result.first;
            reduced_probs = reduction_result.second;
            
            // The perform_scenario_reduction function already calculated metrics internally
            // We need to extract them (for now, we'll recalculate - later we can refactor)
            // Store the metrics
            reduction_metrics.reduction_time_ms = reduction_time_ms;
            reduction_metrics.selected_indices = selected_scenario_indices;
            reduction_metrics.ell = 2.0; // Default value used in perform_scenario_reduction
            
            // Calculate Wasserstein distance (this duplicates code from perform_scenario_reduction, 
            // but we need it here to save to cache)
            vector<double> min_distances(all_demand_scenarios.size());
            for (size_t i = 0; i < all_demand_scenarios.size(); ++i) {
                double min_dist = std::numeric_limits<double>::infinity();
                for (int j : selected_scenario_indices) {
                    double dist = 0.0;
                    for (size_t k = 0; k < nc; ++k) {
                        double diff = all_demand_scenarios[i][k] - all_demand_scenarios[j][k];
                        dist += diff * diff;
                    }
                    dist = std::sqrt(dist);
                    dist = std::pow(dist, reduction_metrics.ell);
                    min_dist = std::min(min_dist, dist);
                }
                min_distances[i] = min_dist;
            }
            
            double weight = 1.0 / all_demand_scenarios.size();
            reduction_metrics.wasserstein_ell_power = 0.0;
            for (double d : min_distances) {
                reduction_metrics.wasserstein_ell_power += weight * d;
            }
            reduction_metrics.wasserstein_distance = std::pow(reduction_metrics.wasserstein_ell_power, 1.0 / reduction_metrics.ell);
            
            // Save reduced scenarios with metrics if requested
            if (save_cache) {
                save_scenarios_to_file(reduced_cache, reduced_scenarios, reduced_probs, verbose, &reduction_metrics);
            }
        } else {
            if (verbose >= 1) cout << "\n3. Using cached reduced scenarios" << endl;
            
            // Try to load metrics from cache
            ScenarioReductionMetrics loaded_metrics;
            auto [loaded_scenarios, loaded_probs] = load_scenarios_from_file(reduced_cache, verbose, &loaded_metrics);
            
            // Use loaded metrics if available
            if (loaded_metrics.wasserstein_distance > 0) {
                reduction_metrics = loaded_metrics;
                reduction_time_ms = loaded_metrics.reduction_time_ms;
                selected_scenario_indices = loaded_metrics.selected_indices;
                
                if (verbose >= 1) {
                    cout << "  Loaded metrics from cache:" << endl;
                    cout << "    Scenario reduction time: " << reduction_time_ms << " ms" << endl;
                    cout << "    Wasserstein-" << reduction_metrics.ell << " distance: " << reduction_metrics.wasserstein_distance << endl;
                    cout << "    Selected indices: ";
                    for (size_t i = 0; i < selected_scenario_indices.size(); ++i) {
                        cout << selected_scenario_indices[i];
                        if (i < selected_scenario_indices.size() - 1) cout << ", ";
                    }
                    cout << endl;
                }
            }
        }
        
        // Cache file names for results
        string full_result_cache = cache_dir + "result_full_" + to_string(full_num_scenarios) + ".nc4";
        string reduced_result_cache = cache_dir + "result_reduced_" + to_string(reduced_num_scenarios) + "_" + reduction_method + ".nc4";
        string anticipative_full_cache = cache_dir + "result_anticipative_" + to_string(full_num_scenarios) + ".nc4";
        string anticipative_reduced_cache = cache_dir + "result_anticipative_reduced_" + to_string(reduced_num_scenarios) + "_" + reduction_method + ".nc4";
        
        SolutionResult full_result, reduced_result, anticipative_full, anticipative_reduced;
        
        if (load_results) {
            // Try to load all results from cache
            try {
                if (verbose >= 1) cout << "\n4. Loading solution results from cache:" << endl;
                full_result = load_solution_result(full_result_cache, verbose);
                reduced_result = load_solution_result(reduced_result_cache, verbose);
                anticipative_full = load_solution_result(anticipative_full_cache, verbose);
                anticipative_reduced = load_solution_result(anticipative_reduced_cache, verbose);
                if (verbose >= 1) cout << "  All results loaded from cache successfully" << endl;
            } catch (const exception& e) {
                if (verbose >= 1) {
                    cout << "  Some results not in cache, will compute missing ones..." << endl;
                }
                load_results = false;  // Fall back to computing
            }
        }
        
        if (!load_results) {
            // Step 4A: Create and solve FULL problem
            if (verbose >= 1) cout << "\n4A. Creating and solving FULL problem (" << full_num_scenarios << " scenarios):" << endl;
            
            // Try to load from cache first
            bool full_loaded = false;
            if (load_cache) {
                try {
                    full_result = load_solution_result(full_result_cache, verbose);
                    full_loaded = true;
                    if (verbose >= 1) cout << "  Loaded full problem result from cache" << endl;
                } catch (...) {}
            }
            
            if (!full_loaded) {
                // Include number of scenarios in filename for proper caching
                const string full_netcdf = cache_dir + "twostage_full_" + to_string(full_num_scenarios) + ".nc4";
                
                // Check if the cached file already exists
                std::ifstream check_file(full_netcdf);
                if (!check_file.good()) {
                    // File doesn't exist, create it
                    create_twostage_netcdf(full_netcdf, path, all_demand_scenarios, nf, nc, {}, verbose);
                } else {
                    check_file.close();
                    if (verbose >= 1) {
                        cout << "  Using cached TwoStageStochasticBlock from " << full_netcdf << endl;
                    }
                }
                
                netCDF::NcFile fullFile(full_netcdf, netCDF::NcFile::read);
                auto fullTssGroup = fullFile.getGroup("TwoStageStochasticBlock");
                auto full_tss_block = make_unique<TwoStageStochasticBlock>();
                full_tss_block->deserialize(fullTssGroup);
                
                apply_scenario_data_programmatically(full_tss_block.get(), all_demand_scenarios, nc, verbose);
                
                full_result = compute_extensive_form_solution(
                    full_tss_block.get(), full_num_scenarios, time_limit, verbose);
                
                if (save_cache && full_result.solved) {
                    save_solution_result(full_result_cache, full_result, verbose);
                }
                
                remove(full_netcdf.c_str());
            }
            
            // Step 4B: Create and solve REDUCED problem
            if (verbose >= 1) cout << "\n4B. Creating and solving REDUCED problem (" << reduced_num_scenarios << " scenarios):" << endl;
            
            bool reduced_loaded = false;
            if (load_cache) {
                try {
                    reduced_result = load_solution_result(reduced_result_cache, verbose);
                    reduced_loaded = true;
                    if (verbose >= 1) cout << "  Loaded reduced problem result from cache" << endl;
                } catch (...) {}
            }
            
            if (!reduced_loaded) {
                // Include both scenario counts and method in filename for proper caching
                const string reduced_netcdf = cache_dir + "twostage_reduced_" + to_string(full_num_scenarios) + 
                                              "_to_" + to_string(reduced_num_scenarios) + "_" + reduction_method + ".nc4";
                
                // Check if the cached file already exists
                std::ifstream check_file(reduced_netcdf);
                if (!check_file.good()) {
                    // File doesn't exist, create it
                    create_twostage_netcdf(reduced_netcdf, path, reduced_scenarios, nf, nc, reduced_probs, verbose);
                } else {
                    check_file.close();
                    if (verbose >= 1) {
                        cout << "  Using cached TwoStageStochasticBlock from " << reduced_netcdf << endl;
                    }
                }
                
                netCDF::NcFile reducedFile(reduced_netcdf, netCDF::NcFile::read);
                auto reducedTssGroup = reducedFile.getGroup("TwoStageStochasticBlock");
                auto reduced_tss_block = make_unique<TwoStageStochasticBlock>();
                reduced_tss_block->deserialize(reducedTssGroup);
                
                apply_scenario_data_programmatically(reduced_tss_block.get(), reduced_scenarios, nc, verbose);
                
                reduced_result = compute_extensive_form_solution(
                    reduced_tss_block.get(), reduced_num_scenarios, time_limit, verbose);
                
                if (save_cache && reduced_result.solved) {
                    save_solution_result(reduced_result_cache, reduced_result, verbose);
                }
                
                remove(reduced_netcdf.c_str());
            }
            
            // Step 5: Compute anticipative solutions if requested
            if (compute_vpi) {
                // Step 5A: Compute anticipative solution for all scenarios
                if (verbose >= 1) cout << "\n5A. Computing anticipative solution (perfect information) for all scenarios:" << endl;
                
                bool anticipative_full_loaded = false;
                if (load_cache) {
                    try {
                        anticipative_full = load_solution_result(anticipative_full_cache, verbose);
                        anticipative_full_loaded = true;
                        if (verbose >= 1) cout << "  Loaded anticipative full result from cache" << endl;
                    } catch (...) {}
                }
                
                if (!anticipative_full_loaded) {
                    anticipative_full = compute_anticipative_solution(
                        path, all_demand_scenarios, nf, nc, time_limit, verbose);
                    
                    if (save_cache && anticipative_full.solved) {
                        save_solution_result(anticipative_full_cache, anticipative_full, verbose);
                    }
                }
                
                // Step 5B: Compute anticipative solution for reduced scenarios
                if (verbose >= 1) cout << "\n5B. Computing anticipative solution (perfect information) for reduced scenarios:" << endl;
                
                bool anticipative_reduced_loaded = false;
                if (load_cache) {
                    try {
                        anticipative_reduced = load_solution_result(anticipative_reduced_cache, verbose);
                        anticipative_reduced_loaded = true;
                        if (verbose >= 1) cout << "  Loaded anticipative reduced result from cache" << endl;
                    } catch (...) {}
                }
                
                if (!anticipative_reduced_loaded) {
                    anticipative_reduced = compute_anticipative_solution(
                        path, reduced_scenarios, nf, nc, time_limit, verbose);
                    
                    if (save_cache && anticipative_reduced.solved) {
                        save_solution_result(anticipative_reduced_cache, anticipative_reduced, verbose);
                    }
                }
            }
        }
        
        // Step 6: Print final comparison
        if (verbose >= 1) {
            if (compute_vpi) {
                cout << "\n6. Final Results (with VPI):" << endl;
            } else {
                cout << "\n6. Final Results:" << endl;
            }
        }
        
        print_scenario_reduction_results(
            full_result, reduced_result,
            anticipative_full, anticipative_reduced,
            full_num_scenarios, reduced_num_scenarios,
            reduction_time_ms, selected_scenario_indices);
        
        if (verbose >= 2) {
            cout << "\n=== Test completed successfully ===" << endl;
        } else if (verbose >= 1) {
            cout << "Test completed successfully." << endl;
        }
        return 0;
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}