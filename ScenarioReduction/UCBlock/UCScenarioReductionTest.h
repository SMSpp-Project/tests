/*--------------------------------------------------------------------------*/
/*---------------- File UCScenarioReductionTest.h -------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * UC-specific hook functions plugged into the shared scenario-reduction
 * driver (see ScenarioReductionCommon.h). There is no UC test class here on
 * purpose: main() builds a ProblemHooks value from the free functions below
 * and calls run_scenario_reduction_test(); everything else (caching, VPI,
 * warmstart, print_results) lives once in ScenarioReductionCommon.
 *
 * \author Benoît Tran \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Benoît Tran
 */
/*--------------------------------------------------------------------------*/

#ifndef __UCScenarioReductionTest
#define __UCScenarioReductionTest

#include "ScenarioReductionCommon.h"
#include "CSSCScenarioReductionSolver.h"
#include "IntermittentUnitBlock.h"
#include "ThermalUnitBlock.h"
#include "UCBlock.h"

#include <memory>
#include <string>
#include <vector>

namespace SMSpp_di_unipi_it {

/*--------------------------------------------------------------------------*/
/*------------------------------ struct UCState -----------------------------*/
/*--------------------------------------------------------------------------*/
/** UC-specific data captured by the ProblemHooks lambdas built in main();
 *  replaces what used to be private members of a UCScenarioReductionTest
 *  subclass. Plain data, nothing virtual: see ScenarioReductionCommon.h for
 *  why the whole framework is free functions + hooks rather than a class
 *  hierarchy. */

 struct UCState {

  /** Which uncertain parameters appear in the scenario vectors */
  enum class UncertaintyType { kDemandOnly , kRenewableOnly , kBoth };

  UncertaintyType             uncertainty_type = UncertaintyType::kRenewableOnly;
  std::vector< Index >        intermittent_units;
  size_t                      num_time_periods = 0;
  size_t                      num_nodes        = 0;
  std::string                 instance_file_path;  // for EC Block copy workaround

  // Owned object that must outlive the SRB (for CSSC); reset at the start
  // of each uc_create_srb() call
  std::unique_ptr< TwoStageStochasticBlock > srb_tssb;

 };  // struct UCState

/*--------------------------------------------------------------------------*/
/*--------------------------- UC hook functions ------------------------------*/
/*--------------------------------------------------------------------------*/
/** Build a ProblemHooks from these in main(), e.g.:
 *
 *   UCState uc_state;
 *   ProblemHooks hooks;
 *   hooks.problem_type = "UC";
 *   hooks.load_problem_instance = [&]( auto & st , auto & path ) {
 *    uc_load_problem_instance( st , uc_state , path ); };
 *   hooks.create_srb = [&]( auto & st , int K , auto & method ) {
 *    return uc_create_srb( st , uc_state , K , method ); };
 *   hooks.build_tssb_for_current_pool = [&]( auto & st , auto & tmp ) {
 *    return uc_build_tssb_for_current_pool( st , uc_state , tmp ); };
 *   hooks.run_cssc = [&]( auto & st , auto * srb , auto * bsc , int K ) {
 *    uc_run_cssc( st , uc_state , srb , bsc , K ); };
 *   hooks.get_scenarios_directory = [] { return "../scenarios/UCBlock/"; };
 *   return run_scenario_reduction_test( argc , argv , hooks ); */

 /** Load base UCBlock instance. Sets state.base_block, state.stochastic_block,
  *  and populates uc_state (time horizon, nodes, intermittent units...). */
 void uc_load_problem_instance( ScenarioReductionState & state ,
                                UCState & uc_state ,
                                const std::string & path );

 /** Build a fully configured ScenarioReductionBlock.
  *
  *  Always sets: scenario_generator (state.scenario_set).
  *  For CSSC additionally builds a TwoStageStochasticBlock + applicator,
  *  owned by uc_state.srb_tssb so it stays alive as long as the SRB is. */
 std::unique_ptr< ScenarioReductionBlock >
 uc_create_srb( ScenarioReductionState & state , UCState & uc_state ,
               int K , const std::string & method );

 /** Build a TSSB from the base UCBlock + current state.scenario_set pool */
 std::unique_ptr< TwoStageStochasticBlock >
 uc_build_tssb_for_current_pool( ScenarioReductionState & state ,
                                 UCState & uc_state , const std::string & tmp );

 /** UC CSSC: uses CSSCScenarioReductionSolver with a UC-specific
  *  VarExtractor lambda that identifies commitment variables */
 void uc_run_cssc( ScenarioReductionState & state , UCState & uc_state ,
                   ScenarioReductionBlock * srb , BlockSolverConfig * bsc ,
                   int K );

}  

#endif /* __UCScenarioReductionTest */

/*--------------------------------------------------------------------------*/
/*---------------- End File UCScenarioReductionTest.h ---------------------*/
/*--------------------------------------------------------------------------*/
