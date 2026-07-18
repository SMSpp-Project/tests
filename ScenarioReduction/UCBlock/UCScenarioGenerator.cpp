/*--------------------------------------------------------------------------*/
/*---------------------- File UCScenarioGenerator.cpp ----------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Standalone scenario generator for Unit Commitment (UC) problems.
 * Generates stochastic scenarios for both demand (ActivePowerDemand) and
 * renewable generation (MaxPower) profiles, saving them as DiscreteScenarioSet
 * in netCDF format.
 *
 * The generator properly handles intermittent unit indexing by tracking
 * the actual unit indices in the UC instance, avoiding issues with
 * non-consecutive unit numbering.
 *
 * \author Benoît Tran \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Benoît Tran
 */
/*--------------------------------------------------------------------------*/

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <netcdf>
#include <netcdf.h>   // NC_STRING, NC_CHAR, nc_free_string
#include <random>

#include "BlockSolverConfig.h"
#include "Configuration.h"
#include "DiscreteScenarioSet.h"
#include "IntermittentUnitBlock.h"
#include "Solver.h"
#include "UCBlock.h"

#include "common_utils.h"

using namespace std;
using namespace SMSpp_di_unipi_it;
namespace fs = std::filesystem;

/*--------------------------------------------------------------------------*/
/*------------------------------ STRUCTURES --------------------------------*/
/*--------------------------------------------------------------------------*/

struct UCGeneratorConfig {
 string instance_path;   // Path to base UC instance
 string output_path;     // Output path for scenarios
 string tssb_output_path; // Output path for a full TSSB file (the generate/solve split,
                          // "generate" step); empty = don't write
 int num_scenarios = 20; // Number of scenarios to generate
 double variation_factor =
   0.3;                        // Variation factor for both demand and renewable
 unsigned seed = 42;           // Random seed
 int verbose = 1;              // Verbosity level
 bool enable_demand = true;    // Enable demand uncertainty
 bool enable_renewable = true; // Enable renewable uncertainty
 double validation_timeout = 10.0; // Timeout for validation solving
 bool validate = false; // Whether to validate scenarios
 bool validate_only = false; // Only validate, don't generate scenarios
 string solver_config = "../BSCfg.txt"; // Solver configuration file
};

/*--------------------------------------------------------------------------*/
/*--------------------------- HELPER FUNCTIONS -----------------------------*/
/*--------------------------------------------------------------------------*/

void print_help( const char * program_name ) {
 cout << "Usage: " << program_name << " [options]" << endl;
 cout << "\nOptions:" << endl;
 cout << "  -i, --instance <path>     Path to base UC instance file (required)"
      << endl;
 cout << "  -o, --output <path>       Output path for scenarios" << endl;
 cout << "  -n, --scenarios <number>  Number of scenarios to generate "
  "(default: 20)"
      << endl;
 cout << "  -v, --variation <factor>  Variation factor (default: 0.3)" << endl;
 cout << "  -s, --seed <number>       Random seed (default: 42)" << endl;
 cout << "  --no-demand              Disable demand uncertainty" << endl;
 cout << "  --no-maxpower            Disable renewable uncertainty" << endl;
 cout << "  --verbose <level>        Verbosity level 0-2 (default: 1)" << endl;
 cout << "  --validate               Enable scenario validation (WILL SEGFAULT with demand)"
      << endl;
 cout << "  --no-validate            Skip scenario validation" << endl;
 cout << "  --validate-only          Only validate instance, don't generate "
  "scenarios"
      << endl;
 cout << "  --timeout <seconds>      Validation timeout per scenario (default: "
  "10)"
      << endl;
 cout << "  --solver-config <path>   Solver configuration file (default: "
  "BSCfg.txt)"
      << endl;
 cout << "  -h, --help               Show this help message" << endl;
 cout << "\nExamples:" << endl;
 cout << "  " << program_name << " -i EC_CO_Test.nc4 -n 100 -v 0.3" << endl;
 cout << "  " << program_name << " -i EC_NC_Test.nc4 --no-demand -n 50" << endl;
 cout << "  " << program_name << " -i EC_CO_Test.nc4 --no-maxpower -n 30"
      << endl;
 cout << "  " << program_name
      << " -i EC_CO_Test.nc4 --validate-only  # Just validate instance" << endl;
}

/*--------------------------------------------------------------------------*/

UCGeneratorConfig parse_arguments( int argc , char * argv[] ) {
 UCGeneratorConfig config;

 for( int i = 1; i < argc; ++i ) {
  string arg = argv[ i ];

  if( arg == "-h" || arg == "--help" ) {
   print_help( argv[ 0 ] );
   exit( 0 );
  }
  else if( arg == "-i" || arg == "--instance" ) {
   if( i + 1 < argc ) {
    config.instance_path = argv[ ++i ];
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else if( arg == "-o" || arg == "--output" ) {
   if( i + 1 < argc ) {
    config.output_path = argv[ ++i ];
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else if( arg == "-n" || arg == "--scenarios" ) {
   if( i + 1 < argc ) {
    config.num_scenarios = stoi( argv[ ++i ] );
    if( config.num_scenarios <= 0 ) {
     cerr << "Number of scenarios must be positive" << endl;
     exit( 1 );
    }
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else if( arg == "-v" || arg == "--variation" ) {
   if( i + 1 < argc ) {
    config.variation_factor = stod( argv[ ++i ] );
    if( config.variation_factor < 0 ) {
     cerr << "Variation factor must be non-negative" << endl;
     exit( 1 );
    }
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else if( arg == "-s" || arg == "--seed" ) {
   if( i + 1 < argc ) {
    config.seed = stoi( argv[ ++i ] );
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else if( arg == "--no-demand" ) {
   config.enable_demand = false;
  }
  else if( arg == "--no-maxpower" ) {
   config.enable_renewable = false;
  }
  else if( arg == "--verbose" ) {
   if( i + 1 < argc ) {
    config.verbose = stoi( argv[ ++i ] );
    if( config.verbose < 0 || config.verbose > 3 ) {
     cerr << "Verbose level must be 0, 1, 2, or 3" << endl;
     exit( 1 );
    }
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else if( arg == "--validate" ) {
   config.validate = true;
  }
  else if( arg == "--no-validate" ) {
   config.validate = false;
  }
  else if( arg == "--validate-only" ) {
   config.validate_only = true;
   config.validate = true;
  }
  else if( arg == "--timeout" ) {
   if( i + 1 < argc ) {
    config.validation_timeout = stod( argv[ ++i ] );
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else if( arg == "--solver-config" ) {
   if( i + 1 < argc ) {
    config.solver_config = argv[ ++i ];
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else if( arg == "--tssb-output" ) {
   if( i + 1 < argc ) {
    config.tssb_output_path = argv[ ++i ];
   }
   else{
    cerr << "Missing value after " << arg << endl;
    exit( 1 );
   }
  }
  else{
   cerr << "Unknown option: " << arg << endl;
   cerr << "Use -h or --help for usage information" << endl;
   exit( 1 );
  }
 }

 // Check required arguments
 if( config.instance_path.empty() ) {
  cerr << "Error: Instance path is required (-i or --instance)" << endl;
  cerr << "Use -h or --help for usage information" << endl;
  exit( 1 );
 }

 // Check that at least one uncertainty type is enabled
 if( ! config.enable_demand && ! config.enable_renewable ) {
  cerr << "Error: At least one uncertainty type must be enabled" << endl;
  cerr << "Cannot use both --no-demand and --no-maxpower" << endl;
  exit( 1 );
 }

 // Set default output path if not specified
 if( config.output_path.empty() ) {
  fs::path instance_file( config.instance_path );
  string suffix;
  if( config.enable_demand && config.enable_renewable ) {
   suffix = "_apdmp";
  }
  else if( config.enable_demand ) {
   suffix = "_apd";
  }
  else{
   suffix = "_mp";
  }
  // Save to centralized scenarios directory
  config.output_path = "../scenarios/UCBlock/" + instance_file.stem().string()
    +
    suffix + "_scenarios.nc4";
 }

 return config;
}

/*--------------------------------------------------------------------------*/

struct UCData {
 vector< double > demand_profile;             // ActivePowerDemand flattened
 vector< vector< double > > renewable_profiles; // MaxPower for each renewable unit
 vector< int > renewable_unit_indices;        // Indices of intermittent units
 vector< int > thermal_unit_indices;          // Indices of ThermalUnitBlock units
                                              // (here-and-now commitment vars)
 int num_periods = 0;
 int num_nodes = 0;
 int num_renewable_units = 0;
};

/*--------------------------------------------------------------------------*/

UCData load_uc_instance(
  const string & instance_path ,
  const UCGeneratorConfig & config ) {
 UCData data;

 if( config.verbose >= 1 ) {
  cout << "Loading UC instance: " << instance_path << endl;
 }

 try {
  netCDF::NcFile file( instance_path , netCDF::NcFile::read );
  auto block = file.getGroup( "Block_0" );

  // Load demand profile if needed
  if( config.enable_demand ) {
   auto demand_var = block.getVar( "ActivePowerDemand" );
   if( ! demand_var.isNull() ) {
    // Get dimensions
    auto dims = demand_var.getDims();
    if( dims.size() != 2 ) {
     throw runtime_error( "ActivePowerDemand should have 2 dimensions" );
    }
    // ActivePowerDemand is [TimeHorizon, NumberNodes] in netCDF
    // But C++ API returns dimensions in reverse order
    data.num_periods = dims[ 1 ].getSize(); // TimeHorizon (actually dims[1])
    data.num_nodes = dims[ 0 ].getSize();   // NumberNodes (actually dims[0])


    // Read demand data safely
    size_t total_size = data.num_periods * data.num_nodes;
    if( total_size == 0 ) {
     throw runtime_error( "ActivePowerDemand has zero size" );
    }

    // Read data from file
    vector< double > temp_demand( total_size );
    demand_var.getVar( temp_demand.data() );


    // The file has data in [time][node] order (time-major)
    // UCBlock expects [node][time] order (node-major)
    // So we need to transpose
    data.demand_profile.resize( total_size );
    for( size_t n = 0; n < data.num_nodes; n++ ) {
     for( size_t t = 0; t < data.num_periods; t++ ) {
      // Source: temp_demand[t * num_nodes + n] (file: time-major)
      // Dest: demand_profile[n * num_periods + t] (UCBlock: node-major)
      data.demand_profile[ n * data.num_periods + t ] =
        temp_demand[ t * data.num_nodes + n ];
     }
    }



    if( config.verbose >= 2 ) {
     cout << "  Loaded demand: " << data.num_periods << " periods × "
          << data.num_nodes << " nodes = " << data.demand_profile.size()
          << " values" << endl;
    }
   }
   else{
    cerr << "Warning: No ActivePowerDemand found in instance" << endl;
   }
  }

  // Load renewable profiles if needed
  if( config.enable_renewable ) {
   // Find all IntermittentUnitBlock groups
   auto groups = block.getGroups();
   for( const auto &[ name , group ] : groups ) {
    if( name.find( "UnitBlock_" ) != string::npos ) {
     // Extract unit index from name (e.g., "UnitBlock_5" -> 5)
     int unit_index = -1;
     size_t underscore_pos = name.find( "_" );
     if( underscore_pos != string::npos ) {
      try {
       unit_index = stoi( name.substr( underscore_pos + 1 ) );
      } catch( ... ) {
       continue;
      }
     }

     auto type_attr = group.getAtt( "type" );
     if( ! type_attr.isNull() ) {
      string unit_type;
      type_attr.getValues( unit_type );

      if( unit_type == "IntermittentUnitBlock" ) {
       auto max_power = group.getVar( "MaxPower" );
       if( ! max_power.isNull() ) {
        auto power_dims = max_power.getDims();
        if( power_dims.empty() || power_dims[ 0 ].getSize() == 0 ) {
         if( config.verbose >= 2 ) {
          cout << "    Warning: Skipping IntermittentUnitBlock " << name
               << " with empty MaxPower" << endl;
         }
         continue;
        }

        size_t profile_size = power_dims[ 0 ].getSize();
        vector< double > profile( profile_size );
        max_power.getVar( profile.data() );
        data.renewable_profiles.push_back( profile );
        data.renewable_unit_indices.push_back( unit_index );
        data.num_renewable_units++;

        if( config.verbose >= 2 ) {
         cout << "    Found intermittent unit " << name << " at index " << unit_index << endl;
        }
       }
      }
     }
    }
   }

   if( config.verbose >= 2 ) {
    cout << "  Loaded renewable profiles: " << data.num_renewable_units
         << " units × " << data.num_periods << " periods" << endl;
   }
  }

  // Find all ThermalUnitBlock groups (needed for the TSSB's
  // StaticAbstractPath: their commitment variables are the here-and-now
  // decisions CSSC fixes). Always scanned, independent of enable_renewable.
  {
   auto groups = block.getGroups();
   for( const auto &[ name , group ] : groups ) {
    if( name.find( "UnitBlock_" ) != string::npos ) {
     int unit_index = -1;
     size_t underscore_pos = name.find( "_" );
     if( underscore_pos != string::npos ) {
      try { unit_index = stoi( name.substr( underscore_pos + 1 ) ); }
      catch( ... ) { continue; }
     }
     auto type_attr = group.getAtt( "type" );
     if( ! type_attr.isNull() ) {
      string unit_type;
      type_attr.getValues( unit_type );
      if( unit_type == "ThermalUnitBlock" ) {
       data.thermal_unit_indices.push_back( unit_index );
       if( config.verbose >= 2 )
        cout << "    Found thermal unit " << name << " at index "
             << unit_index << endl;
      }
     }
    }
   }
  }

 } catch( const netCDF::exceptions::NcException & e ) {
  cerr << "Error loading UC instance: " << e.what() << endl;
  exit( 1 );
 }

 return data;
}

/*--------------------------------------------------------------------------*/

vector< vector< double > > generate_demand_scenarios(
  const vector< double > & base_demand ,
  int num_clusters ,
  mt19937 & gen ,
  double variation_factor ) {

 vector< vector< double > > scenarios;

 // Cluster 1: High demand (1.2x to 1.4x)
 uniform_real_distribution<> high_dist( 1.2 , 1.4 );
 vector< double > high_scenario( base_demand.size() );
 for( size_t i = 0; i < base_demand.size(); ++i ) {
  high_scenario[ i ] = base_demand[ i ] * high_dist( gen );
 }
 scenarios.push_back( high_scenario );

 // Cluster 2: Base demand (0.9x to 1.1x)
 uniform_real_distribution<> base_dist( 0.9 , 1.1 );
 vector< double > base_scenario( base_demand.size() );
 for( size_t i = 0; i < base_demand.size(); ++i ) {
  base_scenario[ i ] = base_demand[ i ] * base_dist( gen );
 }
 scenarios.push_back( base_scenario );

 // Cluster 3: Low demand (0.6x to 0.8x)
 uniform_real_distribution<> low_dist( 0.6 , 0.8 );
 vector< double > low_scenario( base_demand.size() );
 for( size_t i = 0; i < base_demand.size(); ++i ) {
  low_scenario[ i ] = base_demand[ i ] * low_dist( gen );
 }
 scenarios.push_back( low_scenario );

 return scenarios;
}

/*--------------------------------------------------------------------------*/

vector< vector< double > > generate_renewable_scenarios(
  const vector< vector< double > > & base_profiles ,
  int num_clusters ,
  mt19937 & gen ,
  double variation_factor ) {

 vector< vector< double > > scenarios;
 size_t total_size = base_profiles.size() *
   ( base_profiles.empty() ? 0 : base_profiles[ 0 ].size() );

 // Cluster 1: High renewable (sunny/windy - 1.2x to 1.5x)
 uniform_real_distribution<> high_dist( 1.2 , 1.5 );
 vector< double > high_scenario;
 high_scenario.reserve( total_size );
 for( const auto & profile : base_profiles ) {
  for( double val : profile ) {
   // Preserve zeros (night time for solar)
   high_scenario.push_back( val == 0 ? 0 : val * high_dist( gen ) );
  }
 }
 scenarios.push_back( high_scenario );

 // Cluster 2: Base renewable (0.8x to 1.2x)
 uniform_real_distribution<> base_dist( 0.8 , 1.2 );
 vector< double > base_scenario;
 base_scenario.reserve( total_size );
 for( const auto & profile : base_profiles ) {
  for( double val : profile ) {
   base_scenario.push_back( val == 0 ? 0 : val * base_dist( gen ) );
  }
 }
 scenarios.push_back( base_scenario );

 // Cluster 3: Low renewable (cloudy/calm - 0.3x to 0.7x)
 uniform_real_distribution<> low_dist( 0.3 , 0.7 );
 vector< double > low_scenario;
 low_scenario.reserve( total_size );
 for( const auto & profile : base_profiles ) {
  for( double val : profile ) {
   low_scenario.push_back( val == 0 ? 0 : val * low_dist( gen ) );
  }
 }
 scenarios.push_back( low_scenario );

 return scenarios;
}

/*--------------------------------------------------------------------------*/

vector< vector< double > > combine_scenarios(
  const vector< vector< double > > & demand_scenarios ,
  const vector< vector< double > > & renewable_scenarios ,
  const UCGeneratorConfig & config ) {

 vector< vector< double > > combined_scenarios;

 // Always add base scenario first (original data)
 vector< double > base_scenario;
 if( ! demand_scenarios.empty() && ! renewable_scenarios.empty() ) {
  // Combine middle scenarios (base clusters)
  base_scenario.insert(
    base_scenario.end() ,
    demand_scenarios[ 1 ].begin() ,
    demand_scenarios[ 1 ].end() );
  base_scenario.insert(
    base_scenario.end() ,
    renewable_scenarios[ 1 ].begin() ,
    renewable_scenarios[ 1 ].end() );
 }
 else if( ! demand_scenarios.empty() ) {
  base_scenario = demand_scenarios[ 1 ]; // Base demand cluster
 }
 else{
  base_scenario = renewable_scenarios[ 1 ]; // Base renewable cluster
 }
 combined_scenarios.push_back( base_scenario );

 // Generate all combinations
 if( config.enable_demand && config.enable_renewable ) {
  // Cartesian product of demand and renewable scenarios
  for( const auto & demand : demand_scenarios ) {
   for( const auto & renewable : renewable_scenarios ) {
    if( & demand == & demand_scenarios[ 1 ] &&
      & renewable == & renewable_scenarios[ 1 ] ) {
     continue; // Skip base scenario (already added)
    }
    vector< double > scenario;
    scenario.reserve( demand.size() + renewable.size() );
    scenario.insert( scenario.end() , demand.begin() , demand.end() );
    scenario.insert( scenario.end() , renewable.begin() , renewable.end() );
    combined_scenarios.push_back( scenario );
   }
  }
 }
 else if( config.enable_demand ) {
  // Only demand scenarios (skip the base already added)
  for( size_t i = 0; i < demand_scenarios.size(); ++i ) {
   if( i != 1 ) { // Skip base
    combined_scenarios.push_back( demand_scenarios[ i ] );
   }
  }
 }
 else{
  // Only renewable scenarios (skip the base already added)
  for( size_t i = 0; i < renewable_scenarios.size(); ++i ) {
   if( i != 1 ) { // Skip base
    combined_scenarios.push_back( renewable_scenarios[ i ] );
   }
  }
 }

 // Ensure we have the requested number of scenarios
 while( combined_scenarios.size() < static_cast< size_t >( config.num_scenarios )
   ) {
  // Duplicate scenarios with small random perturbations
  mt19937 gen( config.seed + combined_scenarios.size() );
  uniform_real_distribution<> perturb( 0.95 , 1.05 );

  size_t idx = combined_scenarios.size() % ( combined_scenarios.size() - 1 ) + 1
  ;
  vector< double > new_scenario = combined_scenarios[ idx ];
  for( double & val : new_scenario ) {
   if( val != 0 ) { // Don't perturb zeros
    val *= perturb( gen );
   }
  }
  combined_scenarios.push_back( new_scenario );
 }

 // Trim to exact number if we have too many
 if( combined_scenarios.size() > static_cast< size_t >( config.num_scenarios ) ) {
  combined_scenarios.resize( config.num_scenarios );
 }

 return combined_scenarios;
}

/*--------------------------------------------------------------------------*/

bool validate_scenario(
  const string & instance_path ,
  const UCData & original_data ,
  const vector< double > & scenario ,
  const UCGeneratorConfig & config ,
  bool is_base_scenario = false ) {

 if( config.verbose >= 3 ) {
  cout << "\n  [validate_scenario] is_base=" << is_base_scenario
       << ", demand_size=" << original_data.demand_profile.size()
       << ", periods=" << original_data.num_periods
       << ", nodes=" << original_data.num_nodes << endl;
 }

 // Create a fresh UCBlock for each validation to avoid state corruption
 UCBlock * uc_block = new UCBlock();

 // Load the instance
 netCDF::NcFile nc_file( instance_path , netCDF::NcFile::read );
 auto block_group = nc_file.getGroup( "Block_0" );
 if( block_group.isNull() ) {
  delete uc_block;
  return false;
 }
 uc_block->deserialize( block_group );

 // Generate abstract representation (variables and constraints)
 // This is needed before we can modify demand values
 if( config.verbose >= 3 ) {
  cout << "  [DEBUG] Generating abstract variables and constraints..." << endl;
 }
 uc_block->generate_abstract_variables();
 uc_block->generate_abstract_constraints();


 // If this is not the base scenario, apply the scenario modifications
 if( ! is_base_scenario ) {
  // Apply scenario to UCBlock
  // First, extract demand and renewable parts from the combined scenario
  size_t demand_size = original_data.demand_profile.size();
  size_t renewable_total_size =
    original_data.num_renewable_units * original_data.num_periods;

  // Apply demand changes if demand uncertainty is enabled
  if( config.enable_demand && demand_size > 0 ) {
   vector< double > scenario_demand(
     scenario.begin() ,
     scenario.begin() + demand_size );

   // Pass all data (nodes * periods) with full range
   // UCBlock expects the range to cover all node-time pairs
   try {
    uc_block->set_active_power_demand(
      scenario_demand.begin() ,
      Block::Range( 0 , demand_size ) );
   } catch( const exception & e ) {
    if( config.verbose >= 1 ) {
     cerr << "    ERROR in set_active_power_demand: " << e.what() << endl;
    }
    delete uc_block;
    return false;
   }


  }

  // Apply renewable changes if renewable uncertainty is enabled
  if( config.enable_renewable && original_data.num_renewable_units > 0 ) {
   // Get the renewable portion of the scenario
   size_t start_idx = config.enable_demand ? demand_size : 0;

   // Apply to each intermittent unit using the correct indices
   for( size_t idx = 0; idx < original_data.renewable_unit_indices.size(); ++idx ) {
    int unit_index = original_data.renewable_unit_indices[idx];

    // Get the specific unit block by index
    auto * unit_block = dynamic_cast< IntermittentUnitBlock * >(
      uc_block->get_unit_block( unit_index ) );

    if( unit_block ) {
     // Extract this unit's profile from the scenario
     size_t profile_start = start_idx + ( idx * original_data.num_periods );
     vector< double > unit_profile(
       scenario.begin() + profile_start ,
       scenario.begin() + profile_start + original_data.num_periods );

     // Use time range for set_maximum_power
     unit_block->set_maximum_power(
       unit_profile.begin() ,
       Block::Range( 0 , original_data.num_periods ) );

    } else {
     cerr << "Warning: Could not get IntermittentUnitBlock at index "
          << unit_index << endl;
    }
   }
  }
 } // end if (!is_base_scenario)

 // Load solver configuration
 auto cfg = Configuration::deserialize( config.solver_config );
 if( ! cfg ) {
  if( config.verbose >= 1 ) {
   cerr << "Failed to load solver configuration: " << config.solver_config
        << endl;
  }
  return false;
 }

 bool success = false;

 try {
  s_config_Block( uc_block , cfg , config.solver_config );

  // Get and configure the solver
  if( ! uc_block->get_registered_solvers().empty() ) {
   auto solver = uc_block->get_registered_solvers().front();

   if( solver ) {
    // Set timeout
    solver->set_par( Solver::dblMaxTime , config.validation_timeout );

    // Solve
    int result = solver->compute( false );

    // Check if feasible
    if( result == Solver::kOK ) {
     success = true;
     if( config.verbose >= 3 ) {
      double obj = solver->get_ub();
      cout << " (obj: " << fixed << setprecision( 2 ) << obj << ")";
     }
    }
   }
  }
 } catch( const exception & e ) {
  if( config.verbose >= 2 ) {
   cerr << "Solver exception: " << e.what() << endl;
  }
 }

 delete cfg;

 // Clean up the UCBlock
 delete uc_block;

 return success;
}

/*--------------------------------------------------------------------------*/

vector< double > regenerate_scenario(
  const UCData & base_data ,
  int cluster ,
  mt19937 & gen ,
  const UCGeneratorConfig & config ) {

 vector< double > new_scenario;
 uniform_real_distribution<> mild_variation( 0.85 , 1.15 );

 // Generate demand part if enabled
 if( config.enable_demand ) {
  vector< double > demand_scenario( base_data.demand_profile.size() );

  switch( cluster ) {
  case 0: // Normal cluster
   for( size_t i = 0; i < base_data.demand_profile.size(); ++i ) {
    demand_scenario[ i ] = base_data.demand_profile[ i ] * mild_variation( gen )
    ;
   }
   break;

  case 1: // High demand cluster
   for( size_t i = 0; i < base_data.demand_profile.size(); ++i ) {
    uniform_real_distribution<> dist( 1.1 , 1.3 );
    demand_scenario[ i ] = base_data.demand_profile[ i ] * dist( gen );
   }
   break;

  case 2: // Low demand cluster
   for( size_t i = 0; i < base_data.demand_profile.size(); ++i ) {
    uniform_real_distribution<> dist( 0.7 , 0.9 );
    demand_scenario[ i ] = base_data.demand_profile[ i ] * dist( gen );
   }
   break;
  }

  new_scenario.insert(
    new_scenario.end() ,
    demand_scenario.begin() ,
    demand_scenario.end() );
 }

 // Generate renewable part if enabled
 if( config.enable_renewable ) {
  for( const auto & base_profile : base_data.renewable_profiles ) {
   vector< double > renewable_scenario( base_profile.size() );

   switch( cluster ) {
   case 0: // Normal cluster
    for( size_t i = 0; i < base_profile.size(); ++i ) {
     renewable_scenario[ i ] = base_profile[ i ] * mild_variation( gen );
    }
    break;

   case 1: // High renewable cluster
    for( size_t i = 0; i < base_profile.size(); ++i ) {
     uniform_real_distribution<> dist( 1.1 , 1.3 );
     renewable_scenario[ i ] = base_profile[ i ] * dist( gen );
    }
    break;

   case 2: // Low renewable cluster
    for( size_t i = 0; i < base_profile.size(); ++i ) {
     uniform_real_distribution<> dist( 0.7 , 0.9 );
     renewable_scenario[ i ] = base_profile[ i ] * dist( gen );
    }
    break;
   }

   new_scenario.insert(
     new_scenario.end() ,
     renewable_scenario.begin() ,
     renewable_scenario.end() );
  }
 }

 return new_scenario;
}

/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*------------------------ nc_copy_group_recursive -------------------------*/
/*--------------------------------------------------------------------------*/
/* Deep-copy a netCDF group (attributes, dimensions, variables, sub-groups).
 * Used to clone the base instance's Block_0 group verbatim into the TSSB
 * file's StochasticBlock/Block sub-group, sidestepping any block-specific
 * serialize() call entirely (this generator never deserializes the instance
 * into a live UCBlock object at all, it only ever reads raw netCDF). */

static void nc_copy_group_recursive( const netCDF::NcGroup & src ,
                                     netCDF::NcGroup & dst ) {
 for( const auto & [ name , att ] : src.getAtts() ) {
  try { string val; att.getValues( val ); dst.putAtt( name , val ); }
  catch( ... ) {}  // skip non-string attributes
 }

 for( const auto & [ name , dim ] : src.getDims() )
  dst.addDim( name , dim.getSize() );

 for( const auto & [ name , var ] : src.getVars() ) {
  auto type  = var.getType();
  auto sdims = var.getDims();

  size_t total = 1;
  for( const auto & d : sdims ) total *= d.getSize();

  vector< netCDF::NcDim > ddims;
  for( const auto & d : sdims ) {
   auto found = dst.getDim( d.getName() , netCDF::NcGroup::ParentsAndCurrent );
   if( found.isNull() )
    throw runtime_error( "nc_copy_group_recursive: dim not found: "
                         + d.getName() );
   ddims.push_back( found );
  }

  auto dvar = dst.addVar( name , type , ddims );
  if( total == 0 ) continue;

  auto tid = type.getId();
  if( tid == NC_STRING ) {
   vector< char * > ptrs( total , nullptr );
   var.getVar( ptrs.data() );
   vector< const char * > cptrs( total );
   for( size_t i = 0 ; i < total ; ++i )
    cptrs[ i ] = ptrs[ i ] ? ptrs[ i ] : "";
   dvar.putVar( cptrs.data() );
   nc_free_string( static_cast< size_t >( total ) , ptrs.data() );
  }
  else if( tid == NC_CHAR ) {
   // NC_CHAR is the only "text" type netCDF-cxx4's char* get/putVar overload
   // accepts; NC_BYTE/NC_UBYTE are numeric and must use the generic branch
   // below, or the library throws "Attempt to convert between text & numbers".
   vector< char > buf( total );
   var.getVar( buf.data() );
   dvar.putVar( buf.data() );
  }
  else {
   vector< double > buf( total );
   var.getVar( buf.data() );
   dvar.putVar( buf.data() );
  }
 }

 for( const auto & [ name , child ] : src.getGroups() ) {
  auto dst_child = dst.addGroup( name );
  nc_copy_group_recursive( child , dst_child );
 }
}

/*--------------------------------------------------------------------------*/
/*---------------------------- save_tssb_netcdf ----------------------------*/
/*--------------------------------------------------------------------------*/
/* Generate step: write a full, self-contained TwoStageStochastic-
 * Block file (base UC instance + StaticAbstractPath to the ThermalUnitBlock
 * commitment variables + StochasticBlock/DataMapping(s) for demand and/or
 * renewable + the DiscreteScenarioSet), in the generic "Block_0" +
 * SMS++_file_type=1 format Block::deserialize(filename) expects. Everything
 * problem-specific is baked into the file here, once; the generic
 * scenario_reduction_solve program never needs to know it is UC at all. */

static void save_tssb_netcdf(
  const string & filename ,
  const string & instance_path ,
  const vector< vector< double > > & scenarios ,
  const UCGeneratorConfig & config ,
  const UCData & data ) {
 if( config.verbose >= 1 )
  cout << "\nSaving TSSB to: " << filename << endl;

 fs::path filepath( filename );
 if( filepath.has_parent_path() )
  fs::create_directories( filepath.parent_path() );

 const int N  = static_cast< int >( scenarios.size() );
 const int T  = data.num_periods;
 const int nd = data.num_nodes;
 const auto & thermal = data.thermal_unit_indices;
 const int nth = static_cast< int >( thermal.size() );
 const auto & intermittent = data.renewable_unit_indices;
 const int ni = static_cast< int >( intermittent.size() );
 const bool has_demand    = config.enable_demand;
 const bool has_renewable = config.enable_renewable;

 netCDF::NcFile f( filename , netCDF::NcFile::replace );
 f.putAtt( "SMS++_file_type" , netCDF::NcInt() , 1 );

 auto g = f.addGroup( "Block_0" );
 g.putAtt( "type" , "TwoStageStochasticBlock" );
 g.putAtt( "id" , "0" );
 g.addDim( "NumberScenarios" , N );

 // ---- StaticAbstractPath: one 2-node 'B'+'V' path per ThermalUnitBlock ----
 // 'B' navigates to the unit's own nested Block (group index = its index
 // among UCBlock's units), 'V' selects the whole "u_thermal" (commitment)
 // static variable group via a range [0, T). Named group lookup avoids
 // depending on ThermalUnitBlock's variable registration order.
 {
  auto pg    = g.addGroup( "StaticAbstractPath" );
  auto pdim  = pg.addDim( "PathDim" , nth );
  auto tldim = pg.addDim( "PathTotalLength" , nth * 2 );

  vector< unsigned int > starts( nth );
  for( int k = 0 ; k < nth ; ++k ) starts[ k ] = static_cast< unsigned int >( k * 2 );

  vector< char > node_types( nth * 2 );
  vector< string > group_names( nth * 2 );
  vector< unsigned int > elem_idx( nth * 2 , 0 );
  vector< unsigned int > range_idx( nth * 2 , 0 );

  for( int k = 0 ; k < nth ; ++k ) {
   node_types[ 2*k ]      = 'B';
   group_names[ 2*k ]     = to_string( thermal[ k ] );
   node_types[ 2*k + 1 ]  = 'V';
   group_names[ 2*k + 1 ] = "u_thermal";
   elem_idx[ 2*k + 1 ]    = 0;
   range_idx[ 2*k + 1 ]   = static_cast< unsigned int >( T );
  }

  pg.addVar( "PathStart"     , netCDF::NcUint() , pdim  ).putVar( starts.data() );
  pg.addVar( "PathNodeTypes" , netCDF::NcChar() , tldim ).putVar( node_types.data() );
  {
   auto gv = pg.addVar( "PathGroupIndices" , netCDF::NcString() , tldim );
   vector< const char * > cptrs( group_names.size() );
   for( size_t i = 0 ; i < group_names.size() ; ++i ) cptrs[ i ] = group_names[ i ].c_str();
   gv.putVar( cptrs.data() );
  }
  pg.addVar( "PathElementIndices" , netCDF::NcUint() , tldim ).putVar( elem_idx.data() );
  pg.addVar( "PathRangeIndices"   , netCDF::NcUint() , tldim ).putVar( range_idx.data() );
 }

 // ---- StochasticBlock: inner UCBlock (raw copy) + DataMapping(s) ----------
 {
  auto sg = g.addGroup( "StochasticBlock" );
  sg.putAtt( "type" , "StochasticBlock" );

  // Raw netCDF copy of the base instance's own Block_0, instead of
  // deserializing to a live UCBlock and calling ->serialize(): sidesteps
  // the ECNetworkBlock::serialize() bug entirely, whether or not it has
  // been fixed upstream yet.
  auto bg = sg.addGroup( "Block" );
  {
   netCDF::NcFile src( instance_path , netCDF::NcFile::read );
   nc_copy_group_recursive( src.getGroup( "Block_0" ) , bg );
  }

  int num_mappings = 0;
  if( has_demand    ) num_mappings += 1;
  if( has_renewable ) num_mappings += ni;

  auto ndm = sg.addDim( "NumberDataMappings" , num_mappings );
  vector< char > dtypes( num_mappings , 'D' );
  vector< char > callers( num_mappings , 'B' );
  sg.addVar( "DataType" , netCDF::NcChar() , ndm ).putVar( dtypes.data() );
  sg.addVar( "Caller"   , netCDF::NcChar() , ndm ).putVar( callers.data() );

  vector< string > fn;
  if( has_demand )
   fn.push_back( "UCBlock::set_active_power_demand" );
  if( has_renewable )
   for( int k = 0 ; k < ni ; ++k )
    fn.push_back( "IntermittentUnitBlock::set_maximum_power" );
  {
   auto fv = sg.addVar( "FunctionName" , netCDF::NcString() , ndm );
   for( int m = 0 ; m < num_mappings ; ++m )
    fv.putVar( { static_cast< size_t >( m ) } , fn[ m ] );
  }

  vector< unsigned int > ss_vec( 2 * num_mappings , 0u );
  auto ssd = sg.addDim( "SetSizeDim" , ss_vec.size() );
  sg.addVar( "SetSize" , netCDF::NcUint() , ssd ).putVar( ss_vec.data() );

  // SetElements: 4 values per mapping -> {from_start, from_end, to_start, to_end}
  // Scenario vector layout: [demand: nd*T][renewable_0: T]...[renewable_{ni-1}: T]
  vector< unsigned int > se_vec;
  {
   const unsigned int T_u  = static_cast< unsigned int >( T );
   const unsigned int ndT  = static_cast< unsigned int >( nd * T );
   unsigned int roff = 0u;
   if( has_demand ) {
    se_vec.insert( se_vec.end() , { 0u , ndT , 0u , ndT } );
    roff = ndT;
   }
   if( has_renewable )
    for( int k = 0 ; k < ni ; ++k )
     se_vec.insert( se_vec.end() ,
      { roff + k * T_u , roff + ( k+1 ) * T_u , 0u , T_u } );
  }
  auto sed = sg.addDim( "SetElementsDim" , se_vec.size() );
  sg.addVar( "SetElements" , netCDF::NcUint() , sed ).putVar( se_vec.data() );

  // AbstractPath for the DataMapping targets themselves:
  // - demand mapping:    empty path (caller is the UCBlock itself)
  // - renewable mapping: length-1 'B' path to the IntermittentUnitBlock child
  {
   vector< unsigned int > path_starts;
   vector< char >         path_node_types;
   vector< string >       path_group_names;
   vector< unsigned int > path_elem_idx;

   unsigned int cur = 0;
   if( has_demand )
    path_starts.push_back( cur );  // empty path: length 0
   if( has_renewable )
    for( int k = 0 ; k < ni ; ++k ) {
     path_starts.push_back( cur );
     path_node_types.push_back( 'B' );
     path_group_names.push_back( to_string( intermittent[ k ] ) );
     path_elem_idx.push_back( 0 );
     ++cur;
    }

   const size_t num_paths = path_starts.size();
   const size_t total_len = path_node_types.size();

   auto apg     = sg.addGroup( "AbstractPath" );
   auto ap_pdim = apg.addDim( "PathDim" , num_paths );
   auto ap_tdim = apg.addDim( "PathTotalLength" , total_len );

   apg.addVar( "PathStart" , netCDF::NcUint() , ap_pdim ).putVar( path_starts.data() );
   if( total_len > 0 ) {
    apg.addVar( "PathNodeTypes" , netCDF::NcChar() , ap_tdim )
       .putVar( path_node_types.data() );
    {
     auto gv = apg.addVar( "PathGroupIndices" , netCDF::NcString() , ap_tdim );
     vector< const char * > cptrs( path_group_names.size() );
     for( size_t i = 0 ; i < path_group_names.size() ; ++i )
      cptrs[ i ] = path_group_names[ i ].c_str();
     gv.putVar( cptrs.data() );
    }
    apg.addVar( "PathElementIndices" , netCDF::NcUint() , ap_tdim )
       .putVar( path_elem_idx.data() );
   }
   else {
    apg.addVar( "PathNodeTypes"      , netCDF::NcChar() , ap_tdim );
    apg.addVar( "PathGroupIndices"   , netCDF::NcString() , ap_tdim );
    apg.addVar( "PathElementIndices" , netCDF::NcUint() , ap_tdim );
   }
  }
 }

 // ---- DiscreteScenarioSet: reuse the class's own serializer ---------------
 {
  DiscreteScenarioSet dss;
  vector< double > weights( N , 1.0 / N );
  dss.load_from_memory( scenarios , weights );
  auto dg = g.addGroup( "DiscreteScenarioSet" );
  dss.serialize( dg );
 }

 if( config.verbose >= 1 )
  cout << "Successfully saved TSSB (" << N << " scenarios, " << nth
       << " thermal units, " << ni << " intermittent units)" << endl;
}

/*--------------------------------------------------------------------------*/

void save_scenarios_netcdf(
  const string & filename ,
  const vector< vector< double > > & scenarios ,
  const UCGeneratorConfig & config ,
  const UCData & data ) {

 if( config.verbose >= 1 ) {
  cout << "\nSaving scenarios to: " << filename << endl;
 }

 // Create directory if needed
 fs::path filepath( filename );
 if( filepath.has_parent_path() ) {
  fs::create_directories( filepath.parent_path() );
 }

 // Create netCDF file
 netCDF::NcFile file( filename , netCDF::NcFile::replace );

 // Add metadata attributes
 file.putAtt( "generator" , "UCScenarioGenerator" );
 file.putAtt( "instance" , config.instance_path );
 file.putAtt( "num_scenarios" , netCDF::NcInt() , config.num_scenarios );
 file.putAtt( "variation_factor" , netCDF::NcDouble() , config.variation_factor
   );
 file.putAtt( "seed" , netCDF::NcUint() , config.seed );
 file.putAtt( "demand_enabled" , netCDF::NcInt() , config.enable_demand ? 1 : 0
   );
 file.putAtt(
   "renewable_enabled" ,
   netCDF::NcInt() ,
   config.enable_renewable ? 1 : 0 );

 // Add component sizes
 if( config.enable_demand ) {
  file.putAtt(
    "demand_size" ,
    netCDF::NcInt() ,
    static_cast< int >( data.demand_profile.size() ) );
 }
 if( config.enable_renewable ) {
  file.putAtt( "renewable_units" , netCDF::NcInt() , data.num_renewable_units )
  ;
  file.putAtt(
    "renewable_size" ,
    netCDF::NcInt() ,
    data.num_renewable_units * data.num_periods );
 }

 // Add timestamp
 auto now = chrono::system_clock::now();
 auto time_t = chrono::system_clock::to_time_t( now );
 string timestamp = ctime( & time_t );
 timestamp.pop_back(); // Remove newline
 file.putAtt( "generated_at" , timestamp );

 // Add dimensions
 auto scenDim = file.addDim( "NumberScenarios" , scenarios.size() );
 auto sizeDim = file.addDim( "ScenarioSize" , scenarios[ 0 ].size() );

 // Add scenario data
 auto scenVar =
   file.addVar( "Scenarios" , netCDF::NcDouble() , { scenDim , sizeDim } );
 for( size_t s = 0; s < scenarios.size(); ++s ) {
  scenVar.putVar( { s , 0 } , { 1 , scenarios[ 0 ].size() } , scenarios[ s ].
    data() );
 }

 // Add uniform probabilities
 auto probVar = file.addVar( "Probabilities" , netCDF::NcDouble() , scenDim );
 vector< double > probs( scenarios.size() , 1.0 / scenarios.size() );
 probVar.putVar( probs.data() );

 if( config.verbose >= 1 ) {
  cout << "Successfully saved " << scenarios.size() << " scenarios" << endl;
  cout << "Scenario size: " << scenarios[ 0 ].size() << " values" << endl;
  if( config.enable_demand && config.enable_renewable ) {
   cout << "  Demand: " << data.demand_profile.size() << " values" << endl;
   cout << "  Renewable: " << ( data.num_renewable_units * data.num_periods )
        << " values" << endl;
  }
 }
}

/*--------------------------------------------------------------------------*/
/*--------------------------------- MAIN -----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char * argv[] ) {
 try {
  // Parse command line arguments
  UCGeneratorConfig config = parse_arguments( argc , argv );

  // Print configuration
  if( config.verbose >= 1 ) {
   cout << "UC Scenario Generator" << endl;
   cout << "=====================" << endl;
   cout << "Instance: " << config.instance_path << endl;
   if( ! config.validate_only ) {
    cout << "Output: " << config.output_path << endl;
    cout << "Scenarios: " << config.num_scenarios << endl;
    cout << "Variation: " << config.variation_factor << endl;
    cout << "Seed: " << config.seed << endl;
   }
   cout << "Uncertainty types:" << endl;
   cout << "  Demand: " << ( config.enable_demand ? "enabled" : "disabled" )
        << endl;
   cout << "  Renewable: " << ( config.enable_renewable ? "enabled" : "disabled" )
        << endl;
   if( config.validate ) {
    cout << "Validation: "
         << ( config.validate_only ? "validate-only mode" : "enabled" ) << endl;
    cout << "Solver config: " << config.solver_config << endl;
    cout << "Timeout: " << config.validation_timeout << " seconds" << endl;
   }
   cout << endl;
  }

  // Load UC instance data
  UCData data = load_uc_instance( config.instance_path , config );

  // Validate if needed
  if( config.validate ) {
   if( config.verbose >= 1 ) {
    cout << "\nValidating base instance...";
   }

   // Create base scenario (just the original data)
   vector< double > base_scenario;
   if( config.enable_demand ) {
    base_scenario.insert(
      base_scenario.end() ,
      data.demand_profile.begin() ,
      data.demand_profile.end() );
   }
   if( config.enable_renewable ) {
    for( const auto & profile : data.renewable_profiles ) {
     base_scenario.insert( base_scenario.end() , profile.begin() , profile.end
        () );
    }
   }

   bool valid = validate_scenario(
    config.instance_path ,
    data ,
    base_scenario ,
    config ,
    true );   // true = is_base_scenario
   if( config.verbose >= 1 ) {
    if( valid ) {
     cout << " VALID" << endl;
    }
    else{
     cout << " INVALID (infeasible or timeout)" << endl;
    }
   }

   if( config.validate_only ) {
    return valid ? 0 : 1;
   }
  }

  // Initialize random generator
  mt19937 gen( config.seed );

  // Generate demand scenarios
  vector< vector< double > > demand_scenarios;
  if( config.enable_demand ) {
   if( config.verbose >= 1 ) {
    cout << "\nGenerating demand scenarios (3 clusters)..." << endl;
   }
   demand_scenarios = generate_demand_scenarios(
    data.demand_profile ,
    3 ,
    gen ,
    config.variation_factor );
  }

  // Generate renewable scenarios
  vector< vector< double > > renewable_scenarios;
  if( config.enable_renewable ) {
   if( config.verbose >= 1 ) {
    cout << "Generating renewable scenarios (3 clusters)..." << endl;
   }
   renewable_scenarios = generate_renewable_scenarios(
    data.renewable_profiles ,
    3 ,
    gen ,
    config.variation_factor );
  }

  // Combine scenarios
  if( config.verbose >= 1 ) { cout << "Combining scenarios..." << endl; }
  vector< vector< double > > combined_scenarios =
    combine_scenarios( demand_scenarios , renewable_scenarios , config );

  // Track cluster assignments for regeneration
  vector< int > cluster_assignment;
  for( int i = 0; i < 3; ++i ) {  // 3 clusters
   int scenarios_per_cluster = config.num_scenarios / 3;
   if( i == 2 ) scenarios_per_cluster += config.num_scenarios % 3;
   for( int j = 0; j < scenarios_per_cluster; ++j ) {
    cluster_assignment.push_back( i );
   }
  }

  // Validate scenarios if enabled
  if( config.validate ) {
   if( config.verbose >= 1 ) {
    cout << "\nValidating generated scenarios..." << endl;
   }

   int invalid_count = 0;
   for( size_t s = 0; s < combined_scenarios.size(); ++s ) {
    if( config.verbose >= 2 ) {
     cout << "  Validating scenario " << s << "...";
    }

    bool valid =
      validate_scenario( config.instance_path , data , combined_scenarios[ s ] , config ,
     false );

    if( valid ) {
     if( config.verbose >= 2 ) {
      cout << " OK" << endl;
     }
     else if( config.verbose >= 1 ) {
      cout << "." << flush;
     }
    }
    else{
     invalid_count++;
     if( config.verbose >= 2 ) {
      cout << " INVALID - regenerating" << endl;
     }
     else if( config.verbose >= 1 ) {
      cout << "!" << flush;
     }

     // Keep trying to generate a valid scenario
     bool valid = false;
     const int max_regen_attempts = 5;

     // Try each cluster from current down to 0
     while( ! valid && cluster_assignment[ s ] >= 0 ) {
      int regen_attempts = 0;

      while( ! valid && regen_attempts < max_regen_attempts ) {
       combined_scenarios[ s ] =
         regenerate_scenario( data , cluster_assignment[ s ] , gen , config );

       valid = validate_scenario(
        config.instance_path ,
        data ,
        combined_scenarios[ s ] ,
        config ,
        false );
       regen_attempts++;

       if( ! valid && config.verbose >= 2 ) {
        cout << "    Regeneration attempt " << regen_attempts
             << " failed at cluster " << cluster_assignment[ s ] << endl;
       }
      }

      // If still invalid and not at bottom cluster, move down
      if( ! valid && cluster_assignment[ s ] > 0 ) {
       if( config.verbose >= 2 ) {
        cout << "    Moving from cluster " << cluster_assignment[ s ]
             << " to cluster " << ( cluster_assignment[ s ] - 1 ) << endl;
       }
       cluster_assignment[ s ]--;
      }
      else if( ! valid ) {
       break;
      }
     }

     if( ! valid ) {
      cerr << "\nWarning: Could not generate valid scenario for scenario " << s
           << " even after trying all lower demand clusters" << endl;
     }
    }
   }

   if( config.verbose >= 1 && invalid_count > 0 ) {
    cout << "\n\nValidation summary: " << invalid_count
         << " scenarios were invalid and regenerated" << endl;
   }
   else if( config.verbose >= 1 ) {
    cout << "\n\nAll scenarios validated successfully!" << endl;
   }
  }

  // Save scenarios
  if( config.verbose >= 1 ) {
   cout << "\nSaving scenarios to: " << config.output_path << endl;
  }
  save_scenarios_netcdf( config.output_path , combined_scenarios , config , data
    );

  // Generate step: also write a full TSSB file, if requested
  if( ! config.tssb_output_path.empty() ) {
   save_tssb_netcdf( config.tssb_output_path , config.instance_path ,
                     combined_scenarios , config , data );
  }

  if( config.verbose >= 1 ) { cout << "\nGeneration complete" << endl; }

 } catch( const exception & e ) {
  cerr << "Error: " << e.what() << endl;
  return 1;
 }

 return 0;
}

/*--------------------------------------------------------------------------*/
/*-------------------- End File UCScenarioGenerator.cpp --------------------*/
/*--------------------------------------------------------------------------*/
