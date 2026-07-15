/*--------------------------------------------------------------------------*/
/*-------------------- File ScenarioReductionCommon.h ----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Common structures, state, and driver functions used by scenario reduction
 * tests (CFL, UC, ...).
 *
 * ## Architecture
 *
 * There is no base class here on purpose: the experiment pipeline (load,
 * solve full, reduce, solve reduced, print) is identical across problem
 * types, and each concrete test's main() only differs in a handful of
 * pluggable operations. Those operations are supplied as plain callables in
 * a ProblemHooks value (no inheritance, no virtual dispatch): a class would
 * only be justified if several instances needed to be alive at once, or if
 * genuinely different run-time behaviour needed to be selected per instance;
 * neither holds here (the problem type is fixed at compile time, one per
 * binary).
 *
 * A concrete test's main() therefore looks like:
 *
 *   int main( int argc , char * argv[] ) {
 *    MyProblemState my_state;   // whatever data the hooks below capture
 *    ProblemHooks hooks;
 *    hooks.problem_type = "UC";
 *    hooks.load_problem_instance = [&]( ScenarioReductionState & st ,
 *                                        const std::string & path ) { ... };
 *    hooks.create_srb = [&]( ScenarioReductionState & st , int K ,
 *                            const std::string & method ) { ... };
 *    hooks.build_tssb_for_current_pool = [&]( ScenarioReductionState & st ,
 *                                             const std::string & tmp ) { ... };
 *    hooks.run_cssc = [&]( ScenarioReductionState & st , ScenarioReductionBlock * srb ,
 *                          BlockSolverConfig * bsc , int K ) { ... };
 *    return run_scenario_reduction_test( argc , argv , hooks );
 *   }
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

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "BlockSolverConfig.h"
#include "DiscreteScenarioSet.h"
#include "ScenarioReductionBlock.h"
#include "StochasticBlock.h"
#include "TwoStageStochasticBlock.h"

/*--------------------------------------------------------------------------*/
/*------------------------------ NAMESPACE ---------------------------------*/
/*--------------------------------------------------------------------------*/

namespace SMSpp_di_unipi_it {

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
/*------------------------ ScenarioReductionState ---------------------------*/
/*--------------------------------------------------------------------------*/
/** Plain data holder for one test run. Replaces what used to be the
 * protected data members of AbstractScenarioReductionTest; there is nothing
 * virtual here, it is just state threaded explicitly through the driver
 * functions and the problem-specific hooks. */

struct ScenarioReductionState {

 ScenarioReductionState();
 ~ScenarioReductionState();
 ScenarioReductionState( const ScenarioReductionState & ) = delete;
 ScenarioReductionState & operator=( const ScenarioReductionState & ) = delete;

 ComputeConfig * config = nullptr;

 std::unique_ptr< DiscreteScenarioSet > scenario_set;

 SolutionResult full_result;
 SolutionResult reduced_result;
 SolutionResult anticipative_full;
 SolutionResult anticipative_reduced;
 ScenarioReductionMetrics reduction_metrics;

 Block * base_block = nullptr;                       // non-owning
 std::unique_ptr< StochasticBlock > stochastic_block;

 size_t dimension_scenario = 0;

};  // struct ScenarioReductionState

/*--------------------------------------------------------------------------*/
/*----------------------------- ProblemHooks --------------------------------*/
/*--------------------------------------------------------------------------*/
/** The pluggable, problem-specific operations. Every field except the first
 * four is optional (default-constructed std::function, left unset if the
 * problem doesn't need it / doesn't support the "cssc" method / is happy
 * with the default scenario directory). */

struct ProblemHooks {

 std::string problem_type;  ///< e.g. "CFL", "UC"; used in cache/print names

 /** Load the base problem instance from @p path into @p state.
  *  Must set state.base_block and state.stochastic_block. */
 std::function< void( ScenarioReductionState & state ,
                      const std::string & path ) > load_problem_instance;

 /** Build and return a fully configured ScenarioReductionBlock for the
  *  current state.scenario_set pool (see the equivalent virtual this
  *  replaces, previously documented in AbstractScenarioReductionTest.h, for
  *  the detailed contract). */
 std::function< std::unique_ptr< ScenarioReductionBlock >(
   ScenarioReductionState & state , int K , const std::string & method )
  > create_srb;

 /** Build a TwoStageStochasticBlock using state.base_block and the
  *  currently active pool of state.scenario_set (whatever size it is). */
 std::function< std::unique_ptr< TwoStageStochasticBlock >(
   ScenarioReductionState & state , const std::string & tmp )
  > build_tssb_for_current_pool;

 /** Run the CSSC solver on @p srb using @p bsc as the MILP config
  *  (ownership of @p bsc is transferred to whoever implements this: it must
  *  end up deleted, e.g. via CSSCScenarioReductionSolver::set_milp_config).
  *  Takes state (same as the other hooks) since problem-specific CSSC setup
  *  typically needs state.base_block. Leave unset if the "cssc" method is
  *  not supported: requesting it will then throw with an informative
  *  message. */
 std::function< void( ScenarioReductionState & state , ScenarioReductionBlock * srb ,
                      BlockSolverConfig * bsc , int K ) > run_cssc;

 /** Directory holding pre-generated scenario files, relative to the test
  *  binary's working directory. Leave unset to get the default
  *  "../scenarios/<problem_type>/". */
 std::function< std::string() > get_scenarios_directory;

};  // struct ProblemHooks

/*--------------------------------------------------------------------------*/
/*------------------------- Config-reading helpers --------------------------*/
/*--------------------------------------------------------------------------*/

int         get_int_config( ComputeConfig * config , const std::string & name );
double      get_dbl_config( ComputeConfig * config , const std::string & name );
std::string get_str_config( ComputeConfig * config , const std::string & name );

/*--------------------------------------------------------------------------*/
/*----------------------- run_scenario_reduction_test ------------------------*/
/*--------------------------------------------------------------------------*/
/** Main entry point. Fixed workflow, identical for every problem type:
 *  1. parse_arguments + print_configuration
 *  2. load (calls hooks.load_problem_instance)
 *  3. solve_stochastic_problem  (full N scenarios)
 *  4. solve_anticipative        (optional VPI, full)
 *  5. solve_scenario_reduction  (uses ScenarioReductionBlock)
 *  6. solve_stochastic_problem  (reduced K scenarios)
 *  7. solve_anticipative        (optional VPI, reduced)
 *  8. save_solutions_cache      (optional)
 *  9. print_results             (optional)
 *
 * @param hooks  must at least have load_problem_instance, create_srb, and
 *               build_tssb_for_current_pool set; problem_type non-empty. */

int run_scenario_reduction_test( int argc , char * argv[] , ProblemHooks hooks );

/*--------------------------------------------------------------------------*/
}  // namespace SMSpp_di_unipi_it

#endif /* __ScenarioReductionCommon */

/*--------------------------------------------------------------------------*/
/*------------------------ End File ScenarioReductionCommon.h -------------*/
/*--------------------------------------------------------------------------*/
