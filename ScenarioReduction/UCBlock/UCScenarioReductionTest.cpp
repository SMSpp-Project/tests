/*--------------------------------------------------------------------------*/
/*--------------- File UCScenarioReductionTest.cpp ------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Unit Commitment (UC) specific implementation of the scenario reduction test
 * framework. Tests scenario reduction algorithms on UC problems with demand
 * uncertainty.
 *
 * \author Benoît Tran \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Benoît Tran
 */
/*--------------------------------------------------------------------------*/

#include "UCScenarioReductionTest.h"
#include "DataMapping.h"
#include "DiscreteScenarioSet.h"
#include "ECNetworkBlock.h"
#include "IntermittentUnitBlock.h"
#include "StochasticBlock.h"
#include "ThermalUnitBlock.h"
#include "TwoStageStochasticBlock.h"
#include "UCBlock.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <netcdf>

using namespace std;
namespace fs = std::filesystem;

namespace ScenarioReductionTesting {

 using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*----------------------- METHODS OF UCScenarioReductionTest ---------------*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*--------------------- PROTECTED METHODS ----------------------------------*/
/*--------------------------------------------------------------------------*/

 void UCScenarioReductionTest::load_problem_instance( const std::string & path )
 {
  if( get_int_config( "intLogVerb" ) >= 2 ) {
   cout << "  Loading UC instance from: " << path << endl;
  }

  // Handle TSSB instances - extract base instance path
  // Expected format: path/to/TSSB_EC_XX_Test.nc4
  // Base instance:   path/to/EC_XX_Test.nc4
  string base_instance_path = path;

  // Check if this is a TSSB file
  size_t tssb_pos = path.find("TSSB_");
  if (tssb_pos != string::npos) {
    // Remove "TSSB_" prefix from filename
    size_t last_slash = path.rfind("/");
    if (last_slash != string::npos) {
      string dir = path.substr(0, last_slash + 1);
      string filename = path.substr(last_slash + 1);
      if (filename.starts_with("TSSB_")) {
        filename = filename.substr(5); // Remove "TSSB_" prefix
        base_instance_path = dir + filename;
      }
    }

    if( get_int_config( "intLogVerb" ) >= 2 ) {
     cout << "  Detected TSSB file, loading base instance from: "
          << base_instance_path << endl;
    }
  }

  // Load the base UCBlock instance
  Block * block = Block::deserialize( base_instance_path );
  auto * base_uc = dynamic_cast< UCBlock * >(block);

  if( ! base_uc ) {
   delete block;
   throw runtime_error( "Failed to load UC instance from " + base_instance_path );
  }

  // Set the base_block member (required by abstract class)
  base_block = base_uc;

  // Extract dimensions
  num_time_periods = base_uc->get_time_horizon();
  num_nodes = base_uc->get_number_nodes();
  num_units = base_uc->get_number_units();

  if( get_int_config( "intLogVerb" ) >= 2 ) {
   cout << "  Loaded UCBlock with:" << endl;
   cout << "    Time periods: " << num_time_periods << endl;
   cout << "    Nodes: " << num_nodes << endl;
   cout << "    Units: " << num_units << endl;
  }

  // Find intermittent units (for renewable generation uncertainty)
  intermittent_units.clear();
  for( Index u = 0; u < num_units; ++u ) {
   auto * unit = base_uc->get_unit_block(u);
   if( dynamic_cast< IntermittentUnitBlock * >(unit) ) {
    intermittent_units.push_back(u);
   }
  }

  if( get_int_config( "intLogVerb" ) >= 2 ) {
   cout << "  Found " << intermittent_units.size()
        << " intermittent units for renewable uncertainty" << endl;
  }

  // Create the StochasticBlock parametrically
  stochastic_block = std::make_unique< StochasticBlock >( nullptr , base_block );

  // Store expected dimensions for validation
  expected_renewable_dim = intermittent_units.size() * num_time_periods;
  expected_demand_dim = num_nodes * num_time_periods;

  // Note: We'll create DataMappings after determining the scenario type
  // from the loaded scenario dimensions in setup_data_mappings()
 }

/*--------------------------------------------------------------------------*/

 void UCScenarioReductionTest::create_twostage_netcdf(
   const std::string & filename ) {
 // TODO: Implement TSSB creation for UC
 // For now, just placeholder
  throw runtime_error(
   "UCScenarioReductionTest::create_twostage_netcdf not implemented yet" );
 }

/*--------------------------------------------------------------------------*/

 std::string UCScenarioReductionTest::get_problem_type( ) const { return "UC"; }

/*--------------------------------------------------------------------------*/

 std::string UCScenarioReductionTest::get_scenarios_directory( ) const {
  return "../scenarios/UCBlock/";
 }

/*--------------------------------------------------------------------------*/

 void UCScenarioReductionTest::print_additional_help( const char * program_name
   ) {
  cout << "\nUC-specific options:" << endl;
  cout << "  --demand-only        Only consider demand uncertainty"
       << endl;
  cout <<
    "  --renewable-only     Only consider renewable generation uncertainty"
       << endl;
  cout << "  --both              Consider both demand and renewable uncertainty"
       << endl;
  cout << "\nUC Instance types:" << endl;
  cout << "  - Thermal instances: UC_Data/T-Ramp/*.nc4" << endl;
  cout << "  - Hydro-thermal instances: UC_Data/HT-Ramp/*.nc4" << endl;
  cout << "  - Energy Community instances: EC_Data/EC_*.nc4" << endl;
  cout << "\nExamples:" << endl;
  cout << "  " << program_name
       << " -i ../../../UCBlock/data/nc4/EC_Data/EC_CO_Test.nc4 -n 20 -r 5"
       << endl;
  cout << "  " << program_name
       << " -i ../../../UCBlock/data/nc4/EC_Data/TSSB_EC_CO_Test.nc4 -n 30 -r 10"
       << endl;
 }

/*--------------------------------------------------------------------------*/

 void UCScenarioReductionTest::validate_scenario_dimension( size_t scenario_dim ) {
  // Check if this matches expected dimensions
  if( scenario_dim == expected_renewable_dim ) {
   // Renewable-only scenarios
   if( get_int_config( "intLogVerb" ) >= 2 ) {
    cout << "  Detected renewable-only scenarios (dimension " << scenario_dim << ")" << endl;
   }
   uncertainty_type = RENEWABLE_ONLY;
  } else if( scenario_dim == expected_demand_dim ) {
   // Demand-only scenarios
   if( get_int_config( "intLogVerb" ) >= 2 ) {
    cout << "  Detected demand-only scenarios (dimension " << scenario_dim << ")" << endl;
   }
   uncertainty_type = DEMAND_ONLY;
  } else if( scenario_dim == expected_renewable_dim + expected_demand_dim ) {
   // Both types
   if( get_int_config( "intLogVerb" ) >= 2 ) {
    cout << "  Detected combined demand and renewable scenarios (dimension " << scenario_dim << ")" << endl;
   }
   uncertainty_type = BOTH;
  } else {
   // Unknown dimension
   cerr << "Warning: Unexpected scenario dimension " << scenario_dim << endl;
   cerr << "  Expected renewable dim: " << expected_renewable_dim << endl;
   cerr << "  Expected demand dim: " << expected_demand_dim << endl;
   cerr << "  Expected combined dim: " << (expected_renewable_dim + expected_demand_dim) << endl;
  }

  // Now set up the appropriate data mappings
  setup_data_mappings();
 }

} // end namespace ScenarioReductionTesting

/*--------------------------------------------------------------------------*/
/*----------------------- End File UCScenarioReductionTest.cpp -------------*/
/*--------------------------------------------------------------------------*/
