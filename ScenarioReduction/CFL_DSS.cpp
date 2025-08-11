/*--------------------------------------------------------------------------*/
/*--------------------------- File CFL_DSS.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of CFL_DSS class - a derived class from DiscreteScenarioSet
 * for Capacitated Facility Location problems with stochastic demands.
 * 
 * \author Nils Peyrouset \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 * 
 * \author Benoît Tran \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Nils Peyrouset, Benoît Tran
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "CFL_DSS.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

/*--------------------------------------------------------------------------*/
/*--------------------------- NAMESPACE ------------------------------------*/
/*--------------------------------------------------------------------------*/

namespace SMSpp_di_unipi_it
{

/*--------------------------------------------------------------------------*/
/*--------------------------- PUBLIC METHODS -------------------------------*/
/*--------------------------------------------------------------------------*/

CFL_DSS::CFL_DSS() : DiscreteScenarioSet() {
    // Initialize base class
}

/*--------------------------------------------------------------------------*/

CFL_DSS::~CFL_DSS() {
    // Destructor - base class handles cleanup
}

/*--------------------------------------------------------------------------*/

void CFL_DSS::set_Block(Block* block_ptr) {
    // Call base class implementation first
    DiscreteScenarioSet::set_Block(block_ptr);
    
    // Reset our CFL block pointer
    cfl_block_ptr = nullptr;
    
    if (!block_ptr) {
        return;  // No block provided
    }
    
    // Check if it's a StochasticBlock
    auto stochastic_block = dynamic_cast<StochasticBlock*>(block_ptr);
    if (!stochastic_block) {
        throw std::invalid_argument("CFL_DSS::set_Block: Expected a StochasticBlock");
    }
    
    // Get the inner block
    Block* inner_block = stochastic_block->get_inner_block();
    if (!inner_block) {
        throw std::invalid_argument("CFL_DSS::set_Block: StochasticBlock has no inner block");
    }
    
    // Check if the inner block is a CapacitatedFacilityLocationBlock
    auto cfl_block = dynamic_cast<CapacitatedFacilityLocationBlock*>(inner_block);
    if (!cfl_block) {
        throw std::invalid_argument("CFL_DSS::set_Block: Inner block is not a CapacitatedFacilityLocationBlock");
    }
    
    // Check if it's single-sourcing (UnSplittable)
    if (!cfl_block->get_UnSplittable()) {
        throw std::invalid_argument("CFL_DSS::set_Block: CFL instance must be single-sourcing (UnSplittable)");
    }
    
    // All checks passed, store the pointer
    cfl_block_ptr = cfl_block;
    
    // Optional: Log success
    std::cout << "CFL_DSS::set_Block: Successfully set single-sourcing CFL block with "
              << cfl_block->get_NFacilities() << " facilities and "
              << cfl_block->get_NCustomers() << " customers" << std::endl;
}

/*--------------------------------------------------------------------------*/

void CFL_DSS::set_CFLBlock(CapacitatedFacilityLocationBlock* block_ptr) {
    cfl_block_ptr = block_ptr;
    // Also set it as the general Block
    set_Block(block_ptr);
}

/*--------------------------------------------------------------------------*/

CapacitatedFacilityLocationBlock::CMatrix
CFL_DSS::compute_transport_cost_matrix(ScenarioIndex n_scenarios,
                                       ScenarioSize scenario_size,
                                       float ell) const {
    
    // Create a transport cost matrix for scenario reduction
    // This represents the distance/cost between scenarios
    CapacitatedFacilityLocationBlock::CMatrix cost_matrix(
        boost::extents[n_scenarios][n_scenarios]
    );
    
    // Initialize the matrix with zeros on diagonal
    for (ScenarioIndex i = 0; i < n_scenarios; ++i) {
        for (ScenarioIndex j = 0; j < n_scenarios; ++j) {
            if (i == j) {
                cost_matrix[i][j] = 0.0;
            } else {
                // TODO: change that
                double diff = std::abs(static_cast<double>(i - j));
                cost_matrix[i][j] = std::pow(diff, ell);
            }
        }
    }
    
    return cost_matrix;
}

/*--------------------------------------------------------------------------*/

double CFL_DSS::compute_scenario_distance(const Eigen::VectorXd& scenario1,
                                         const Eigen::VectorXd& scenario2,
                                         float ell) const {
    
    // TODO: Change that
    
    if (scenario1.size() != scenario2.size()) {
        throw std::invalid_argument("CFL_DSS::compute_scenario_distance: "
                                  "scenarios must have the same dimension");
    }
    
    // Compute the ell-norm: ||x - y||_ell^ell
    double distance = 0.0;
    
    if (ell == 1.0) {
        // L1 norm (Manhattan distance)
        distance = (scenario1 - scenario2).cwiseAbs().sum();
    } else if (ell == 2.0) {
        // L2 norm (Euclidean distance) squared
        distance = (scenario1 - scenario2).squaredNorm();
    } else if (std::isinf(ell)) {
        // L-infinity norm (Maximum distance)
        distance = (scenario1 - scenario2).cwiseAbs().maxCoeff();
    } else {
        // General ell-norm
        Eigen::VectorXd diff = scenario1 - scenario2;
        for (int i = 0; i < diff.size(); ++i) {
            distance += std::pow(std::abs(diff[i]), ell);
        }
    }
    
    return distance;
}

/*--------------------------------------------------------------------------*/

} // end namespace SMSpp_di_unipi_it

/*--------------------------------------------------------------------------*/
/*--------------------------- End File CFL_DSS.cpp -------------------------*/
/*--------------------------------------------------------------------------*/