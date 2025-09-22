/*--------------------------------------------------------------------------*/
/*---------------- File AbstractScenarioReductionTest.h --------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Abstract base class for scenario reduction tests.
 * Provides the common framework for testing scenario reduction algorithms
 * on different problem types.
 *
 * ## Important Design Note
 *
 * This framework does NOT handle scenario generation. Scenarios must be
 * pre-generated using problem-specific generator tools and stored in the
 * appropriate directories before running tests. This separation ensures:
 * - Clean separation of concerns
 * - Reproducible test results
 * - Flexibility in scenario generation strategies
 *
 * Each problem type should provide:
 * 1. A scenario generator tool (e.g., CFLScenarioGenerator)
 * 2. A test implementation inheriting from this class
 * 3. A scenarios directory (e.g., scenarios/CFL/)
 *
 * \author Benoît Tran \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Benoît Tran
 */
/*--------------------------------------------------------------------------*/

#ifndef __AbstractScenarioReductionTest
#define __AbstractScenarioReductionTest

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "ScenarioReductionCommon.h"

#include <chrono>
#include <iostream>

#include "DiscreteScenarioSet.h"
#include "ScenarioReductionSolver.h"
#include "ThinComputeInterface.h" // For ComputeConfig
#include "TwoStageStochasticBlock.h"

/*--------------------------------------------------------------------------*/
/*------------------------------ NAMESPACE ---------------------------------*/
/*--------------------------------------------------------------------------*/

namespace ScenarioReductionTesting {

 using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*----------------- CLASS AbstractScenarioReductionTest --------------------*/
/*--------------------------------------------------------------------------*/

/** @class AbstractScenarioReductionTest
 * @brief Abstract base class for scenario reduction tests
 *
 * This class provides the common framework and workflow for testing
 * scenario reduction algorithms on various stochastic optimization problems.
 * Problem-specific implementations must inherit from this class and implement
 * the pure virtual methods.
 *
 * ## Design Philosophy
 *
 * The framework follows a strict separation of concerns:
 * - **Scenario Generation**: Handled by separate, problem-specific generator
 *      tools which provides a serialized netCDF of a (derived)
 *      DiscreteScenarioSet.
 * - **Scenario Reduction Testing**: Handled by this framework, which assumes
 *      having at its disposal a (derived) DiscreteScenarioSet that gives
 *      scenarios that all individualy lead to a feasible two-stage stochastic
 *      problem.
 *
 * Scenarios must be pre-generated and stored in the appropriate directory
 * (specified by get_scenarios_directory()) before running tests. This ensures
 * separation between generation and testing logic.
 *
 *
 * The class implements the a testing pattern through the run() method,
 * which defines the invariant test workflow while allowing subclasses to
 * customize problem-specific operations.
 */
 class AbstractScenarioReductionTest {
 /*--------------------------------------------------------------------------*/
 /*-------------------------- PUBLIC METHODS --------------------------------*/
 /*--------------------------------------------------------------------------*/
 public:
 /** @brief Constructor */
 AbstractScenarioReductionTest( );

 /** @brief Virtual destructor */
 virtual ~AbstractScenarioReductionTest( );

 /** @brief Main entry point for running the test
  *
  * Implements the testing design pattern. This method defines
  * the invariant algorithm structure (the "template") that all test
  * types must follow:
  *
  * 1. parse_arguments() + print_configuration()
  *
  * 2. load() - Load problem instance and pre-generated scenarios
  *    (calls pure virtual load_problem_instance())
  *
  * 3. solve_stochastic_problem() - Solve with full scenario set
  *
  * [4. solve_anticipative() - Optionally compute anticipative solution
  *                            for full set]
  *
  * 5. solve_scenario_reduction() - Perform scenario reduction in-place
  *
  * 6. solve_stochastic_problem() - Solve with reduced scenario set
  *
  * [7. solve_anticipative() - Optionally compute anticipative solution
  *                            for reduced set]
  *
  * [8. print_results() - Optionally display final comparison results]
  *
  * [9. save_solution_cache - Optionally saves SolutionResult-s to cache]
  *
  * The workflow is fixed and cannot be changed by subclasses. Subclasses
  * customize behavior only by implementing the pure virtual methods and
  * optionally overriding virtual methods with default implementations.
  *
  * @param argc Number of command-line arguments
  * @param argv Command-line arguments
  * @return 0 on success, non-zero on failure
  */
 int run( int argc , char * argv[] );

 /*--------------------------------------------------------------------------*/
 /*------------------------- PROTECTED METHODS ------------------------------*/
 /*--------------------------------------------------------------------------*/
 protected:
 // ====================== Pure Virtual Methods ==========================
 // These must be implemented by problem-specific subclasses

 /** @brief Load problem instance from file
  *
  * Subclasses must implement this to load their specific problem type
  * and extract necessary data.
  *
  * This method MUST:
  * - Load the problem instance from the given path,
  * - Set the base_block member to point to the loaded block,
  * - Create and set the stochastic_block member with appropriate
  * DataMappings that specify how scenarios modify the base block.
  *
  * @param path Path to the problem instance file
  */
 virtual void load_problem_instance( const std::string & path ) = 0;

 /** @brief Create a two-stage stochastic problem
  *
  * Creates a TwoStageStochasticBlock using the current state of
  * scenario_set. This is problem-specific due to DataMapping requirements.
  *
  * Uses the internal scenario_set and base_block members.
  *
  * @param filename Output filename for the netCDF file
  */
 virtual void create_twostage_netcdf( const std::string & filename ) = 0;

 /** @brief Get problem-specific description
  * @return String describing the problem type (e.g., "CFL", "UCBlock")
  */
 virtual std::string get_problem_type( ) const = 0;

 // ================== Virtual Methods with Default Implementation =========
 // These can be overridden if needed

 /** @brief Get the default scenarios directory path
  *
  * Subclasses must override to specify where pre-generated scenario files
  * are stored. This directory should contain DiscreteScenarioSet files
  * (.nc4 format) generated by the problem-specific scenario generator.
  *
  * Convention: "scenarios/<ProblemType>/"
  * Example: CFL implementation returns "scenarios/CFL/"
  *
  * @return Path to the directory containing scenario files
  */
 virtual std::string get_scenarios_directory( ) const = 0;

 /** @brief Get the scenario file for the current instance
  *
  * Maps a problem instance to its corresponding pre-generated scenario file.
  * Default implementation follows the naming convention:
  * - Instance: path/to/instanceName.ext
  * - Scenarios: scenarios/ProblemType/instanceName_scenarios.nc4
  *
  * Subclasses can override this to implement different naming conventions.
  *
  * @param instance_path Path to the problem instance
  * @return Path to the corresponding pre-generated scenario file
  */
 virtual std::string get_scenario_file( const std::string & instance_path )
 const;

 // ====================== Common Protected Methods =======================

 /** @brief Get dimension of scenario/uncertainty vector
  * Returns the dimension of each scenario vector, automatically determined
  * from the DiscreteScenarioSet during scenario loading.
  * @return Size of each scenario vector
  */
 virtual size_t get_scenario_dimension( ) const { return dimension_scenario; }

 /** @brief Get the base block for the problem
  * @return Pointer to the base block (set by load_problem_instance)
  */
 Block *get_base_block( );

 /** @brief Apply a scenario to the base block
  *
  * Modifies the base block to reflect the given scenario realization.
  * Uses the stochastic_block's DataMappings to apply the scenario
  * generically.
  *
  * @param scenario The scenario data to apply
  */
 void apply_scenario_to_block( const std::vector< double > & scenario );

 /** @brief Parse command-line arguments */
 void parse_arguments( int argc , char * argv[] );

 /** @brief Print help message */
 void print_help( const char * program_name );

 /** @brief Print configuration */
 void print_configuration( );

 /** @brief Load the problem instance and scenarios
  *
  * Loads the problem instance and scenarios.
  * Scenarios are loaded in the following priority order:
  * 1. User-specified file (-f option)
  * 2. Default scenario file based on instance name
  *
  * Scenarios must be pre-generated using the problem-specific generator
  * and stored in the appropriate directory before running tests.
  *
  * @note load() calls load_problem_instance() which is pure virtual.
  *
  * @throws runtime_error if instance or scenarios cannot be loaded
  */
 void load( );

 /** @brief Perform scenario reduction */
 void solve_scenario_reduction( );

 /** @brief Solve stochastic problems (full and reduced) */
 void solve_stochastic_problem( );

 /** @brief Compute anticipative solutions (perfect information) */
 void solve_anticipative( );

 /** @brief Print final results */
 void print_results( );

 // ====================== Serialize/Deserialize =======================
 /** TODO: should rename these methods as serialize or deserialize
  * for the sake of familiarity with SMS++ patterns.
  */
 /** @brief Load scenarios from file
  * @param filename Path to scenario file
  * @return Loaded scenarios
  */
 std::vector< std::vector< double > > load_scenarios_from_file(
   const std::string & filename );

 /** @brief Save solution results to cache */
 void save_solution_cache(
   const std::string & filename ,
   const SolutionResult & result );

 /** @brief Load solution results from cache */
 SolutionResult load_solution_cache( const std::string & filename );

 /** @brief Save all solution results to cache files
  * Saves full_result, reduced_result, anticipative_full, and
  * anticipative_reduced to their respective cache files if
  * config.save_results is true.
  */
 void save_solutions_cache( );

 /** @brief Solve a block with configured solver
  * @param block The block to solve
  * @return Pair of (objective value, success flag)
  */
 std::pair< double , bool > solve( Block * block );

 /** @brief Compute extensive form solution
  * @param tss_block The two-stage stochastic block
  * @param scenarios The scenarios
  * @param probabilities Scenario probabilities
  * @return Solution result
  */
 SolutionResult compute_extensive_form( TwoStageStochasticBlock * tss_block );

 /** @brief Compute anticipative solution
  * @param scenarios The scenarios
  * @return Solution result
  */
 SolutionResult solve_anticipative_solution( );

 /** @brief Update scenario reduction solver configuration */
 bool update_SR_config( const std::string & method , bool warmstart , bool
   shuffle );

 /** @brief Create a TwoStageStochasticBlock from current scenario_set state
  * Creates a temporary netCDF file and loads it as TwoStageStochasticBlock.
  * @return Unique pointer to the created TwoStageStochasticBlock
  */
 std::unique_ptr< TwoStageStochasticBlock > create_twostage_block( );

 /*--------------------------------------------------------------------------*/
 /*------------------------ PROTECTED MEMBERS -------------------------------*/
 /*--------------------------------------------------------------------------*/
 protected:
 // Configuration using SMS++ ComputeConfig with standard parameter names
 ComputeConfig * config = nullptr;

 // Helper methods to access config parameters (read-only)
 int         get_int_config( const std::string & name ) const;
 double      get_dbl_config( const std::string & name ) const;
 std::string get_str_config( const std::string & name ) const;

 // Scenario data - single DiscreteScenarioSet that transitions from full to
 // reduced
 std::unique_ptr< DiscreteScenarioSet >
 scenario_set;     ///< Current scenario set (full then reduced)

 // Results
 SolutionResult full_result;          ///< Result for full problem
 SolutionResult reduced_result;       ///< Result for reduced problem
 SolutionResult anticipative_full;    ///< Anticipative solution (full)
 SolutionResult anticipative_reduced; ///< Anticipative solution (reduced)
 ScenarioReductionMetrics reduction_metrics; ///< Reduction metrics

 // Base block and stochastic wrapper
 Block * base_block = nullptr; ///< The base block (owned by subclass)
 std::unique_ptr< StochasticBlock >
 stochastic_block;     ///< Stochastic wrapper with DataMappings

 // Scenario dimension (kept for compatibility)
 size_t dimension_scenario = 0; ///< Dimension of each scenario vector

 }; // class AbstractScenarioReductionTest

/*--------------------------------------------------------------------------*/
} // namespace ScenarioReductionTesting

#endif /* __AbstractScenarioReductionTest */

/*--------------------------------------------------------------------------*/
/*------------------ End File AbstractScenarioReductionTest.h --------------*/
/*--------------------------------------------------------------------------*/
