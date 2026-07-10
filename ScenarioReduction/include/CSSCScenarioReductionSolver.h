/*--------------------------------------------------------------------------*/
/*------------ File CSSCScenarioReductionSolver.h ------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Fully generic implementation of the Cost-Space Scenario Clustering (CSSC)
 * algorithm of Keutchayan given a pool of N scenarios of a two-stage stochastic programming, 
 * select K representativescenarios (and an assignment of every scenario to the 
 * so that solving the reduced K-scenario problem is a good proxy for
 * representative that best approximates it) solving the full N-scenario one.
 *
 * This class works with any TwoStageStochasticBlock whose inner Block can be 
 * solved by a registered MILPSolver. The only piece of problem-specific
 * knowledge it needs from the caller is a VarExtractor (see set_var_extractor())
 * that says which ColVariable(s) of the inner Block are here-and-now (first-stage).
 * This is unavoidably a modelling choice, not something a generic solver can
 * infer on its own.
 *
 * ### Algorithm
 *
 * -  compute_generic_V_matrix() (Step 1): build the N x N cost-space matrix
 *    V, where V[i][j] is the cost of solving scenario j's recourse problem
 *    with the here-and-now decision fixed at scenario i's own optimum
 *    (V[i][i] is scenario i solved on its own, nothing fixed).
 * -  solve_cssc_milp() (Step 2): given V, solve a small MILP (Keutchayan et
 *    al., eq. 24-29) that partitions the N scenarios into K clusters, one
 *    representative per cluster, minimizing the average within-cluster cost
 *    discrepancy.
 * 
 * ### How to use
 *
 * @code
 *   auto cssc = std::make_unique<CSSCScenarioReductionSolver>();
 *   cssc->set_milp_config( bsc );            // solver config
 *   cssc->set_nb_reduced( K );               // number of representatives
 *   cssc->set_var_extractor( []( Block * inner ) {
 *       // problem-specific: collect here-and-now ColVariable pointers
 *       auto * my_block = dynamic_cast< MyBlock * >( inner );
 *       std::vector< ColVariable * > vars;
 *       // ...
 *       return vars;
 *   } );
 *   cssc->set_Block( srb );                  // ScenarioReductionBlock
 *   cssc->compute();
 *   cssc->get_var_solution();
 * @endcode
 *
 * The ScenarioReductionBlock requires:
 *   - a TwoStageStochasticBlock (set via set_stochastic_block)
 *   - a StochasticBlock applicator (set via set_scenario_applicator)
 *   - a DiscreteScenarioSet (set via set_scenario_generator)
 *
 * \author Minh Duc Pham \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 */
/*--------------------------------------------------------------------------*/

#ifndef __CSSCScenarioReductionSolver
#define __CSSCScenarioReductionSolver

/*--------------------------------------------------------------------------*/

#include "BlockSolverConfig.h"
#include "ColVariable.h"
#include "DiscreteScenarioSet.h"
#include "ScenarioReductionBlock.h"
#include "Solver.h"
#include "StochasticBlock.h"
#include "TwoStageStochasticBlock.h"

#include <functional>
#include <memory>
#include <vector>

/*--------------------------------------------------------------------------*/

namespace SMSpp_di_unipi_it {

/*--------------------------------------------------------------------------*/
/*-------------- CLASS CSSCScenarioReductionSolver ------------------*/
/*--------------------------------------------------------------------------*/

class CSSCScenarioReductionSolver : public Solver {

public:

/*--------------------------------------------------------------------------*/

 using Index    = unsigned int;
 using OFValue  = RealObjective::OFValue;

 /** Callable: receives the inner Block (after generate_abstract_variables())
  * and returns pointers to all here-and-now (first-stage) ColVariables.
  * This is the one piece of problem-specific knowledge the caller must
  * supply, there is no generic way to tell "first-stage" variables apart
  * from the rest without knowing what the Block represents. Examples:
  *   - a facility-location problem: pointers to the y[0]...y[nf-1] "open
  *     facility f?" variables of CapacitatedFacilityLocationBlock;
  *   - a unit-commitment problem: pointers to the u[0]...u[T-1] commitment
  *     variables of each ThermalUnitBlock. */
 using VarExtractor = std::function< std::vector< ColVariable * >( Block * ) >;

/*--------------------------------------------------------------------------*/

 CSSCScenarioReductionSolver()  = default;
 ~CSSCScenarioReductionSolver() override;

/*--------------------------------------------------------------------------*/

 /** Attach to a ScenarioReductionBlock. \p block must be a
  * ScenarioReductionBlock carrying a TwoStageStochasticBlock, a
  * StochasticBlock scenario applicator, and a DiscreteScenarioSet -- nothing
  * more specific is required, regardless of the underlying problem type.
  * Call set_nb_reduced() and set_milp_config() before this. */
 void set_Block( Block * block ) override;

 /** Run CSSC: Step 1 (V matrix) + Step 2 (MILP partitioning). */
 int compute( bool changedvars = false ) override;

 /** Write the solution (representatives + assignments + weights) into the
  * ScenarioReductionBlock. */
 void get_var_solution( Configuration * solc = nullptr ) override;

 bool has_var_solution() override { return ! ind_red.empty(); }

 /** Returns the Wasserstein distance of the selected scenario set. */
 OFValue get_var_value() override { return f_solution_value; }

/*--------------------------------------------------------------------------*/

 /** Transfer ownership of the BlockSolverConfig used for both Step 1
  * sub-problem solves and the Step 2 partitioning MILP. */
 void set_milp_config( BlockSolverConfig * cfg ) {
  delete f_milp_config;
  f_milp_config = cfg;
  }

 /** Register the problem-specific variable extractor (must be called before
  * compute()).  The lambda receives the inner Block (fully initialised via
  * generate_abstract_variables) and must return ColVariable* pointers to all
  * here-and-now variables. */
 void set_var_extractor( VarExtractor extractor ) {
  f_var_extractor = std::move( extractor );
  }

 /** Required in direct-notification mode (set_fix_with_modification(true),
  * the default); unused in reload mode. Registers the callable responsible
  * for telling the inner Block's attached Solver that scenario data just
  * changed, using whatever targeted Modification the Block type exposes for
  * the purpose (e.g. CapacitatedFacilityLocationBlock::chg_customer_demands).
  * Called right after every StochasticBlock::set_data(), with the inner
  * Block and the just-injected scenario data vector. */
 using DataInjector = std::function< void( Block * ,
                                           const std::vector< double > & ) >;
 void set_data_injector( DataInjector injector ) {
  f_data_injector = std::move( injector );
  }

 /** Set the number of representative scenarios K (must be called before
  * compute(); must satisfy 0 < K <= N). */
 void set_nb_reduced( int K ) { f_K = K; }

 /** Wall-clock time limit (seconds) for the Step-2 partitioning MILP*/
 void set_milp_time_limit( double seconds ) { f_milp_time_limit = seconds; }

 void set_fix_with_modification( bool fwm ) { f_fix_with_mod = fwm; }

/*--------------------------------------------------------------------------*/

private:

 TwoStageStochasticBlock * f_tssb             = nullptr;
 StochasticBlock         * f_stoch_applicator = nullptr;
 BlockSolverConfig       * f_milp_config      = nullptr;
 VarExtractor              f_var_extractor;
 DataInjector              f_data_injector;   ///< direct-notification mode: sends the targeted Modification after set_data()
 bool                      f_fix_with_mod = true;  ///< true = direct-notification mode, false = reload mode (see set_fix_with_modification())
 double                    f_milp_time_limit = 120.0;  ///< Step-2 MILP wall-clock limit (s); <=0 disables

 int f_N = 0;  ///< total scenarios in pool
 int f_K = 0;  ///< target number of representatives

 /** Pool index mapping: relative [0..f_N-1] → absolute DSS scenario index. */
 std::vector< Index > f_pool_map;

 /** Scenario weights (uniform 1/N). */
 std::vector< double > f_weights;

 /** Pairwise Euclidean distances between scenario feature vectors. */
 std::vector< std::vector< double > > f_dist;

 /** Solution state (set by solve_cssc_milp, read by get_var_solution). */
 std::vector< bool >  reduced_atoms;
 std::vector< Index > ind_red;           ///< relative indices of representatives
 std::vector< Index > f_representatives; ///< same as ind_red (for get_var_solution)
 std::vector< Index > f_assignments;     ///< assignment[i] = relative rep index
 std::vector< Index > indices_to_choose; ///< non-representative indices

 double f_solution_value = 0.0;  ///< Wasserstein distance

 std::vector< std::vector< double > > f_V_matrix;

 /** Step 1: build the NxN cost-space matrix V (V[i][j] = cost of scenario i's
  * optimal first-stage applied to scenario j), reusing one inner Block with
  * incremental scenario injection (StochasticBlock::set_data(), optionally
  * followed by the registered DataInjector). */
 std::vector< std::vector< double > > compute_generic_V_matrix();

 /** Step 2: solve the MILP partitioning problem (fully generic, uses only
  * SMS++ AbstractBlock / ColVariable / LinearFunction / FRowConstraint). */
 void solve_cssc_milp( const std::vector< std::vector< double > > & V );

 void update_reduced_atoms() {
  std::fill( reduced_atoms.begin() , reduced_atoms.end() , false );
  for( auto r : ind_red ) reduced_atoms[ r ] = true;
  }

 SMSpp_insert_in_factory_h;

 };  // end( class CSSCScenarioReductionSolver )

}  // end( namespace SMSpp_di_unipi_it )

#endif  /* CSSCScenarioReductionSolver.h included */

/*--------------------------------------------------------------------------*/
/*------- End File CSSCScenarioReductionSolver.h -------------------*/
/*--------------------------------------------------------------------------*/
