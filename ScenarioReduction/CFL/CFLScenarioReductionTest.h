/*--------------------------------------------------------------------------*/
/*------------------ File CFLScenarioReductionTest.h ----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * CFL-specific implementation of scenario reduction test.
 * Extends AbstractScenarioReductionTest for Capacitated Facility Location
 * problems.
 *
 * \author Benoît Tran \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Benoît Tran
 */
/*--------------------------------------------------------------------------*/

#ifndef __CFLScenarioReductionTest
#define __CFLScenarioReductionTest

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include "AbstractScenarioReductionTest.h"

/*--------------------------------------------------------------------------*/
/*------------------------------ NAMESPACE ---------------------------------*/
/*--------------------------------------------------------------------------*/

namespace ScenarioReductionTesting {

 using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------ CLASS CFLScenarioReductionTest -----------------------*/
/*--------------------------------------------------------------------------*/

/** @class CFLScenarioReductionTest
 * @brief CFL-specific implementation of scenario reduction test
 *
 * This class implements the problem-specific methods for testing scenario
 * reduction on Capacitated Facility Location problems. It handles:
 * - Loading CFL instances
 * - Managing demand scenarios
 * - Creating two-stage stochastic CFL problems
 * - Applying scenarios to CFL blocks
 */
 class CFLScenarioReductionTest : public AbstractScenarioReductionTest {
 /*--------------------------------------------------------------------------*/
 /*-------------------------- PUBLIC METHODS --------------------------------*/
 /*--------------------------------------------------------------------------*/
 public:
 /** @brief Constructor */
 CFLScenarioReductionTest( ) = default;

 /** @brief Destructor */
 ~CFLScenarioReductionTest( ) override = default;

 /*--------------------------------------------------------------------------*/
 /*------------------------ PROTECTED METHODS ------------------------------*/
 /*--------------------------------------------------------------------------*/
 protected:
 // ==================== Implementation of Pure Virtual Methods ==============

 /** @brief Load CFL instance from file
  * @param path Path to the CFL instance file
  */
 void load_problem_instance( const std::string & path ) override;

 /** @brief Create two-stage stochastic CFL problem
  * Uses internal scenario_set and base_block.
  * @param filename Output filename for netCDF
  */
 void create_twostage_netcdf( const std::string & filename ) override;

 /** @brief Get problem type description
  * @return "CFL"
  */
 std::string get_problem_type( ) const override { return "CFL"; }

 /** @brief Get the scenarios directory for CFL
  * @return "../scenarios/CFL/" (centralized location)
  */
 std::string get_scenarios_directory( ) const override {
  return "../scenarios/CFL/";
 }

 /*--------------------------------------------------------------------------*/
 /*------------------------- PRIVATE METHODS --------------------------------*/
 /*--------------------------------------------------------------------------*/
 private:
 /** @brief Extract base demands from CFL block
  * @return Vector of original customer demands
  */
 std::vector< double > extract_base_demands( );

 /*--------------------------------------------------------------------------*/
 /*------------------------- PRIVATE MEMBERS --------------------------------*/
 /*--------------------------------------------------------------------------*/
 private:
 // Problem dimensions
 size_t num_facilities = 0; ///< Number of facilities
 size_t num_customers = 0;  ///< Number of customers

 // Base data
 std::vector< double > base_demands;        ///< Original customer demands
 std::vector< double > facility_costs;      ///< Facility opening costs
 std::vector< double > facility_capacities; ///< Facility capacities

 }; // class CFLScenarioReductionTest

/*--------------------------------------------------------------------------*/
} // namespace ScenarioReductionTesting

#endif /* __CFLScenarioReductionTest */

/*--------------------------------------------------------------------------*/
/*------------------ End File CFLScenarioReductionTest.h -------------------*/
/*--------------------------------------------------------------------------*/
