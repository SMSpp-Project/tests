/*--------------------------------------------------------------------------*/
/*---------------- File UCScenarioReductionTest.h -------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Header file for the Unit Commitment (UC) specific implementation of the
 * scenario reduction test framework.
 *
 * \author Benoît Tran \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Benoît Tran */
/*--------------------------------------------------------------------------*/

#ifndef __UCScenarioReductionTest
#define __UCScenarioReductionTest
/* self-identification: #endif at the end of the file */

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "AbstractScenarioReductionTest.h"
#include <memory>
#include <string>
#include <vector>

/*--------------------------------------------------------------------------*/
/*-------------------------- NAMESPACE -------------------------------------*/
/*--------------------------------------------------------------------------*/

namespace SMSpp_di_unipi_it {
// Forward declarations
 class UCBlock;
 class IntermittentUnitBlock;
 class StochasticBlock;
 class ECNetworkBlock;
} // namespace SMSpp_di_unipi_it

namespace ScenarioReductionTesting {

 using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*---------------------- CLASS UCScenarioReductionTest ---------------------*/
/*--------------------------------------------------------------------------*/
/** @class UCScenarioReductionTest
 *
 * UC-specific implementation of AbstractScenarioReductionTest.
 *
 * This class implements scenario reduction testing for Unit Commitment
 * problems, handling UC instances with demand and/or renewable generation
 * uncertainty. It supports various UC instance types including:
 * - Pure thermal UC instances
 * - Hydro-thermal UC instances
 * - Energy Community UC instances with distributed resources
 *
 * The class manages the creation of stochastic UC problems and evaluates
 * scenario reduction quality for UC-specific uncertain parameters.
 */
 class UCScenarioReductionTest : public AbstractScenarioReductionTest {

 /*--------------------------------------------------------------------------*/
 /*----------------------- PUBLIC METHODS -----------------------------------*/
 /*--------------------------------------------------------------------------*/
 public:
 /// Constructor
 UCScenarioReductionTest( ) = default;

 /// Destructor
 virtual ~UCScenarioReductionTest( ) = default;

 /*--------------------------------------------------------------------------*/
 /*----------------------- PROTECTED METHODS --------------------------------*/
 /*--------------------------------------------------------------------------*/
 protected:
 /// Load a UC problem instance from file
 /** Loads a UC instance from netCDF format, supporting various UC types:
  * - Classic thermal UC (T-Ramp instances)
  * - Hydro-thermal UC (HT-Ramp instances)
  * - Energy Community UC (EC_Data instances)
  *
  * Sets up base_block and stochastic_block members.
  *
  * @param path Path to the UC instance file (netCDF format)
  * @throws runtime_error if file cannot be loaded or is invalid
  */
 void load_problem_instance( const std::string & path ) override;

 /// Create a TwoStageStochasticBlock netCDF file for UC
 /** Creates a two-stage stochastic UC problem with the current scenario_set.
  * The first stage typically includes unit commitment decisions,
  * while the second stage handles dispatch under uncertainty.
  *
  * @param filename Output filename for the TSSB netCDF file
  * @throws runtime_error if file cannot be created
  */
 void create_twostage_netcdf( const std::string & filename ) override;

 /// Get the problem type identifier
 /** @return "UC" */
 std::string get_problem_type( ) const override;

 /// Get the directory for storing UC scenarios
 /** @return "../scenarios/UCBlock/" */
 std::string get_scenarios_directory( ) const override;

 /// Print UC-specific help information
 /** Displays additional help for UC-specific options and examples.
  *
  * @param program_name Name of the executable for example commands
  */
 void print_additional_help( const char * program_name );

 /// Override to check for demand scenarios and throw error
 /** This method is called by the base class when loading scenarios.
  * We override it to detect if demand scenarios are being used
  * and throw an error since they're not implemented yet.
  */
 void validate_scenario_dimension( size_t scenario_dim );

 /*--------------------------------------------------------------------------*/
 /*----------------------- PRIVATE DATA -------------------------------------*/
 /*--------------------------------------------------------------------------*/
 private:
 /// Type of uncertainty being considered
 enum UncertaintyType {
  DEMAND_ONLY ,    ///< Only demand uncertainty
  RENEWABLE_ONLY , ///< Only renewable generation uncertainty
  BOTH            ///< Both demand and renewable uncertainty
 };

 /// Current uncertainty type
 UncertaintyType uncertainty_type = RENEWABLE_ONLY; // Default to renewable for now

 /// Problem dimensions
 size_t num_time_periods = 0;  ///< Time horizon of the UC problem
 size_t num_nodes = 0;         ///< Number of nodes in network
 size_t num_units = 0;         ///< Total number of units

 /// Indices of intermittent units (for renewable uncertainty)
 std::vector< Index > intermittent_units;

 /// Expected scenario dimensions for validation
 size_t expected_renewable_dim = 0;  ///< Expected dim for renewable scenarios
 size_t expected_demand_dim = 0;     ///< Expected dim for demand scenarios

 /// Number of thermal units (if applicable)
 size_t num_thermal_units = 0;

 /// Number of hydro units (if applicable)
 size_t num_hydro_units = 0;

 /// Whether this is an Energy Community instance
 bool is_energy_community = false;

 }; // end class UCScenarioReductionTest

/*--------------------------------------------------------------------------*/
/*----------------------- NAMESPACE CLOSING --------------------------------*/
/*--------------------------------------------------------------------------*/

} // end namespace ScenarioReductionTesting

#endif /* __UCScenarioReductionTest */

/*--------------------------------------------------------------------------*/
/*---------------- End File UCScenarioReductionTest.h ---------------------*/
/*--------------------------------------------------------------------------*/
