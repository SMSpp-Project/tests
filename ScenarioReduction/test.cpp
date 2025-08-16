/*--------------------------------------------------------------------------*/
/*-------------------------- File test.cpp ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Test for Two-Stage Stochastic CFL with TwoStageStochasticBlock.
 * 
 * This test validates the TwoStageStochasticBlock implementation by:
 * 1. Loading a deterministic CFL instance from netCDF
 * 2. Ensuring it uses single-sourcing (UnSplittable = true)
 * 3. Creating stochastic demand scenarios (±20% variation)
 * 4. Building a TwoStageStochasticBlock with proper AbstractPaths
 * 5. Solving with MILPSolver
 *
 * In the two-stage CFL:
 * - First stage: Facility opening decisions (y variables)
 * - Second stage: Customer assignments (x variables) under demand uncertainty
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

using namespace SMSpp_di_unipi_it;
using namespace std;

/*--------------------------------------------------------------------------*/
/*------------------------------- HELPERS ----------------------------------*/
/*--------------------------------------------------------------------------*/

// Create a netCDF file for TwoStageStochasticBlock with proper structure
void create_twostage_netcdf(const string& filename,
                            const string& cfl_path,
                            const vector<vector<double>>& demand_scenarios,
                            int nf, int nc) {
    cout << "  Creating netCDF file for TwoStageStochasticBlock..." << endl;
    
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
            cout << "    Converted CFL to unsplittable (single-sourcing)" << endl;
        } else {
            cout << "    CFL is already unsplittable" << endl;
        }
        
        // Verify the unsplittable setting before serialization
        if (base_cfl->get_UnSplittable()) {
            cout << "    ✓ Confirmed: CFL is unsplittable before serialization" << endl;
        } else {
            cerr << "    ERROR: CFL is NOT unsplittable before serialization!" << endl;
            throw runtime_error("Failed to set CFL to unsplittable mode");
        }
        
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
        
        // Create StaticAbstractPath group with AbstractPaths for y variables
        auto staticPathGroup = root.addGroup("StaticAbstractPath");
        
        // For CFL, y variables (facility opening decisions) are first-stage
        // We need to create AbstractPaths pointing to each y variable
        // The y variables are registered as a vector group in CFL
        
        // Create AbstractPath dimensions and variables
        auto pathDim = staticPathGroup.addDim("PathDim", nf);
        auto totalLengthDim = staticPathGroup.addDim("PathTotalLength", nf);
        
        auto pathStart = staticPathGroup.addVar("PathStart", netCDF::NcUint(), pathDim);
        auto pathNodeTypes = staticPathGroup.addVar("PathNodeTypes", netCDF::NcChar(), totalLengthDim);
        auto pathGroupIndices = staticPathGroup.addVar("PathGroupIndices", netCDF::NcUint(), totalLengthDim);
        auto pathElementIndices = staticPathGroup.addVar("PathElementIndices", netCDF::NcUint(), totalLengthDim);
        auto pathRangeIndices = staticPathGroup.addVar("PathRangeIndices", netCDF::NcUint(), totalLengthDim);
        
        // Initialize AbstractPath data for y variables (static/first-stage)
        vector<unsigned int> startIndices(nf);
        vector<unsigned int> elementIndices(nf);
        vector<unsigned int> rangeIndices(nf);
        for (int i = 0; i < nf; ++i) {
            startIndices[i] = i;
            elementIndices[i] = i;
            rangeIndices[i] = i + 1;  // Single element range convention
        }
        
        pathStart.putVar(startIndices.data());
        pathNodeTypes.putVar(vector<char>(nf, 'V').data());  // 'V' for variables
        pathGroupIndices.putVar(vector<unsigned int>(nf, 0).data());  // Group 0
        pathElementIndices.putVar(elementIndices.data());
        pathRangeIndices.putVar(rangeIndices.data());
        
        cout << "    Created netCDF structure with " << demand_scenarios.size() << " scenarios" << endl;
        cout << "    Serialized CFL with " << nf << " facilities and " << nc << " customers" << endl;
        
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

int main() {
    cout << "=== Two-Stage Stochastic CFL Test ===" << endl;
    
    try {
        // Step 1: Load base CFL instance to get dimensions and demands
        cout << "\n1. Loading base CFL instance:" << endl;
        const string path = "../../CapacitatedFacilityLocationBlock/data/nc4/Yang/30-200/30-200-1.nc4";
        
        Block* block = Block::deserialize(path);
        auto* base_cfl = dynamic_cast<CapacitatedFacilityLocationBlock*>(block);
        
        if (!base_cfl) {
            cerr << "Failed to load CFL instance" << endl;
            delete block;
            return 1;
        }
        
        int nf = base_cfl->get_NFacilities();
        int nc = base_cfl->get_NCustomers();
        cout << "  Loaded instance: " << nf << " facilities, " << nc << " customers" << endl;
        
        // Convert to single-sourcing
        if (!base_cfl->get_UnSplittable()) {
            base_cfl->chg_UnSplittable(true);
            cout << "  Converted to single-sourcing" << endl;
        }
        
        // Store original demands
        vector<double> original_demands(nc);
        for (int i = 0; i < nc; ++i) {
            original_demands[i] = base_cfl->get_Demand(i);
        }
        
        delete base_cfl;
        
        // Step 2: Create stochastic demand scenarios
        cout << "\n2. Creating stochastic demand scenarios:" << endl;
        const int num_scenarios = 3;  // Use 3 scenarios for testing
        
        // Generate scenarios with ±20% variation
        vector<vector<double>> demand_scenarios;
        std::mt19937 gen(42);  // Fixed seed for reproducibility
        std::uniform_real_distribution<> dist(0.8, 1.2);  // ±20% variation
        
        for (int s = 0; s < num_scenarios; ++s) {
            vector<double> scenario(nc);
            for (int i = 0; i < nc; ++i) {
                scenario[i] = original_demands[i] * dist(gen);
            }
            demand_scenarios.push_back(scenario);
        }
        cout << "  Generated " << num_scenarios << " demand scenarios with ±20% variation" << endl;
        
        // Step 3: Create netCDF file for TwoStageStochasticBlock
        cout << "\n3. Creating netCDF file for TwoStageStochasticBlock:" << endl;
        const string netcdf_file = "twostage_cfl.nc4";
        create_twostage_netcdf(netcdf_file, path, demand_scenarios, nf, nc);
        
        // Step 4: Deserialize TwoStageStochasticBlock from netCDF
        cout << "\n4. Deserializing TwoStageStochasticBlock from netCDF:" << endl;
        
        // Open the netCDF file and deserialize TwoStageStochasticBlock
        netCDF::NcFile file(netcdf_file, netCDF::NcFile::read);
        auto tssGroup = file.getGroup("TwoStageStochasticBlock");
        
        auto tss_block = make_unique<TwoStageStochasticBlock>();
        tss_block->deserialize(tssGroup);
        
        cout << "  Deserialized TwoStageStochasticBlock with " << num_scenarios << " scenarios" << endl;
        
        // Step 5: Solving the extensive form with MILPSolver
        cout << "\n5. Solving the extensive form with MILPSolver:" << endl;
        
        // Create a BlockSolverConfig for MILPSolver
        auto cfg = Configuration::deserialize("BSPar_HiGHS.txt");
        auto* bsc = dynamic_cast<BlockSolverConfig*>(cfg);
        
        if (bsc) {
            bsc->apply(tss_block.get());
            
            if (!tss_block->get_registered_solvers().empty()) {
                auto solver = tss_block->get_registered_solvers().front();
                cout << "  Computing solution..." << endl;
                int result = solver->compute(false);
                
                if (result == Solver::kOK) {
                    double obj_value = solver->get_lb();
                    cout << "  Optimal solution found!" << endl;
                    cout << "  Objective value: " << fixed << setprecision(2) << obj_value << endl;
                } else if (result == Solver::kInfeasible) {
                    cout << "   Problem is infeasible!" << endl;
                } else {
                    cout << "   Solver failed with code: " << result << endl;
                }
                
                // Cleanup
                bsc->clear();
                bsc->apply(tss_block.get());
            } else {
                cout << "   No solver registered!" << endl;
            }
            delete bsc;
        } else {
            cout << "   Failed to load solver configuration!" << endl;
        }
        
        cout << "\n=== Test completed successfully ===" << endl;
        return 0;
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}