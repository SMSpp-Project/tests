/*--------------------------------------------------------------------------*/
/*-------------------- File ScenarioReductionCommon.h ----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Common structures and types used by scenario reduction tests.
 *
 * \author Benoît Tran \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Benoît Tran
 */
/*--------------------------------------------------------------------------*/

#ifndef __ScenarioReductionCommon
#define __ScenarioReductionCommon

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <vector>
#include <string>

/*--------------------------------------------------------------------------*/
/*------------------------------ NAMESPACE ---------------------------------*/
/*--------------------------------------------------------------------------*/

namespace ScenarioReductionTesting {

/*--------------------------------------------------------------------------*/
/*------------------------------ STRUCTURES --------------------------------*/
/*--------------------------------------------------------------------------*/

/** @struct SolutionResult
 * @brief Stores the result of solving a problem
 */
struct SolutionResult {
 double objective = 0.0;  ///< Objective value
 bool solved = false;     ///< Whether the problem was solved successfully
 long long time_ms = 0;   ///< Solution time in milliseconds
 std::vector<double> scenario_objectives;  ///< Individual scenario objectives
                                           ///< (for anticipative)
};

/*--------------------------------------------------------------------------*/

/** @struct ScenarioReductionMetrics
 * @brief Stores metrics from scenario reduction
 */
struct ScenarioReductionMetrics {
 long long reduction_time_ms =
     0;             ///< Time taken for scenario reduction in milliseconds
 double ell = 2.0;  ///< The ell parameter used for Wasserstein distance
 double wasserstein_distance = 0.0;  ///< The computed Wasserstein-ell distance
 double wasserstein_ell_power =
     0.0;  ///< The ell-th power of Wasserstein distance
 std::vector<int>
     selected_indices;  ///< Indices of selected representative scenarios
 std::vector<double>
     probabilities;  ///< Probabilities of representative scenarios
};

/*--------------------------------------------------------------------------*/

// Forward declaration for ComputeConfig
// TestConfig functionality is now provided by ComputeConfig with SMS++ standard
// parameter naming conventions

/*--------------------------------------------------------------------------*/
}  // namespace ScenarioReductionTesting

#endif /* __ScenarioReductionCommon */

/*--------------------------------------------------------------------------*/
/*------------------------ End File ScenarioReductionCommon.h -------------*/
/*--------------------------------------------------------------------------*/